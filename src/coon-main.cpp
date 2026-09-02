
#include "include-general.h"
#include "esp_log.h"

bool relayState;
bool trigger = false;

Preferences coonPrefs;
bool OTAinProgress = false;
unsigned long ota_progress_millis = 0;

#ifdef WIFI
bool wifiEnabled = true;
bool wifiConnected = false;
unsigned long wifiStartTime = 0;
#else
bool wifiEnabled = false;
bool wifiConnected = false;
#endif

int timerDelay = 1000; // this sets loop time after housekeeping tasks are done
int loopDelay = 10;
time_t lastUpdate, updateTime;
unsigned long lastTime = 0;
unsigned long now;
struct tm *ptm;
char prbuf[PRBUF]; // PRBUF needs to be defined in include.h

Adafruit_INA219 ina219;
movingAvg shuntAvg(10);

// ─── OTA callbacks ──────────────────────────────────────────────────────────────
void onOTAStart() {
  Serial.println("OTA update started!");
  OTAinProgress = true;
}

void onOTAProgress(size_t current, size_t final) {
  if (now - ota_progress_millis > 1000) {
    ota_progress_millis = now;
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  OTAinProgress = false;
  if (success) Serial.println("OTA update finished successfully!");
  else Serial.println("There was an error during OTA update!");
}

#ifdef DEEPSLEEP
// Deep sleep variables
int awakeTimer = 300;  // stay awake for X seconds each time you wake up
// RTC memory variables (persist across deep sleep)
RTC_DATA_ATTR int bootCount = 0;

// Function to print the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: log::toAll("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: log::toAll("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: log::toAll("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: log::toAll("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: log::toAll("Wakeup caused by ULP program"); break;
    default: log::toAll("Wakeup was not caused by deep sleep: " + String((int)wakeup_reason)); break;
  }
}
#else
int bootCount = 0;
#endif
unsigned long startTime; // Time when the device started

void setup() {
  Serial.begin(115200); delay(300);
  startTime = millis();

  // Setup custom panic handler
  setup_custom_panic_handler();

  bool ina219Found = ina219.begin();
  shuntAvg.begin();

  // ─── INA219 startup diagnostics ────────────────────────────────────────────
  // Report everything the INA219 can give us. These reads reflect whatever is
  // present on Vin+/Vin- and are independent of the relay state (the relay only
  // switches the load path; the INA219 senses the rail regardless).
  if (!ina219Found) {
    Serial.println("INA219: begin() FAILED - chip not detected on I2C bus (check wiring/address)");
  } else {
    Serial.println("INA219: detected, reading startup diagnostics...");
    float shuntvoltage_mV = ina219.getShuntVoltage_mV();
    float busvoltage_V    = ina219.getBusVoltage_V();
    float current_mA      = ina219.getCurrent_mA();
    float power_mW        = ina219.getPower_mW();
    // Load voltage = bus voltage plus the drop across the shunt (shunt in mV -> V)
    float loadvoltage_V   = busvoltage_V + (shuntvoltage_mV / 1000.0);

    Serial.printf("INA219: Shunt voltage: %.3f mV\n", shuntvoltage_mV);
    Serial.printf("INA219: Bus voltage:   %.3f V\n",  busvoltage_V);
    Serial.printf("INA219: Load voltage:  %.3f V\n",  loadvoltage_V);
    Serial.printf("INA219: Current:       %.3f mA\n", current_mA);
    Serial.printf("INA219: Power:         %.3f mW\n", power_mW);
    Serial.printf("INA219: last I2C op success: %s\n", ina219.success() ? "yes" : "no");
  }

  // Mount filesystem (needed for console log and WiFi credentials)
  if (LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
    Serial.println("opened LittleFS");
  } else {
    Serial.println("failed to open LittleFS - trying format");
    if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
      Serial.println("LittleFS formatted and mounted");
    } else {
      Serial.println("LittleFS mount failed even after format");
    }
  }

#ifdef WIFI
  checkLittleFS();
  gotWifiCreds = readWiFiCredentials();
#endif
  if (!log::initConsole()) {
    log::toAll("failed to open console log");
  } else {
    log::toAll("console log open");
  }

  // Increment boot number and print it every reboot
  ++bootCount;
  snprintf(logbuf, LOGBUF_SIZE, "Boot number: %d", bootCount);
  log::toAll(logbuf);

#ifdef DEEPSLEEP
  // Print the wakeup reason for ESP32
  print_wakeup_reason();

  // Configure the wake up source - set ESP32 to wake up every TIME_TO_SLEEP seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  log::toAll("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) + " Seconds every " + String(awakeTimer) + " seconds");
#endif

  coonPrefs.begin("ESPprefs", false);
  timerDelay = coonPrefs.getInt("timerdelay", 10000);
  if (timerDelay<1) {
    timerDelay = 10;
    coonPrefs.putInt("timerdelay", 1000);
  }
  log::toAll("timerDelay " + String(timerDelay));

  // set up ultrasonic water sensor
  setupWater();

  wifiEnabled = coonPrefs.getBool("wifi", true);
#ifdef WIFI
  host = coonPrefs.getString("hostname", host);
  log::toAll("hostname: " + host);
  bool doubleReset = coonPrefs.getBool("DRD", false);
  coonPrefs.putBool("DRD", true);
  if (wifiEnabled) {
    if (doubleReset) {
      log::toAll("double reset detected");
      coonPrefs.putBool("DRD", false);
      coonPrefs.end();
      //resetWifi();
    } else {
      // set up DRD if another reboot happens in 10 seconds
      coonPrefs.putBool("DRD", true);
      setupWifi(); // Async - will connect in background via event handlers
    }
    
    // Only start the normal web server when we are NOT in captive-portal mode.
    // startPortal() (called from setupWifi when gotWifiCreds is false) registers
    // its own "/" route serving wifimanager.html. Registering startWebServer()
    // as well would double-register "/" and the first-registered handler wins,
    // which would shadow the host-based redirect. gotWifiCreds tells us which
    // path setupWifi() took.
    if (gotWifiCreds) {
      startWebServer();
    } else {
      log::toAll("captive portal active; skipping normal web server routes");
    }
    serverStarted = true;

#ifdef WEBSERIAL
    WebSerial.begin(&server);
    // Attach a callback function to handle incoming messages
    WebSerial.onMessage(WebSerialonMessage);
#endif
#ifdef ELEGANTOTA
    ElegantOTA.begin(&server);
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);
#endif

    log::toAll("HTTP server started");

    // Load schedules from LittleFS
    loadSchedules();

    // mDNS will be initialized by WiFi event handler when connected
  }
#endif // WIFI
  log::flush();
  pinMode(relayGPIO,OUTPUT);
  if (RELAY_NO) digitalWrite(relayGPIO,LOW);
}

static int loopcount = 0;

void loop() {
#ifdef ELEGANTOTA
  ElegantOTA.loop();
#endif
#ifdef WEBSERIAL
  WebSerial.loop();
#endif
  now = millis();
  static unsigned long lastEventTime, lastTimeTime, startTime;
  // loop ultrasonic water (non-blocking sampling)
  loopWater();
#ifdef WEBSERIAL
  // Dispatch a line typed on the serial console through the same
  // command handler used by the WebSerial interface.
  pollSerialConsole();
#endif
#ifdef WIFI
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
    dnsServer.processNextRequest(); // for captive portal
  // Periodic WiFi health check / reconnect (async wifi manages captive-portal fallback)
  wifiCheck();
  static bool drdCleared = false;

  // Clear DRD flag after DRD_TIMEOUT seconds for double reset detection
  // unless reset occurs within the timeout period
  if (!drdCleared && (now > (DRD_TIMEOUT * 1000))) {
    coonPrefs.putBool("DRD", false);
    drdCleared = true;
    log::toAll("DRD timeout - cleared double reset flag");
  }

  // Check schedule events
  checkScheduleEvents();
#endif
  if (!OTAinProgress && (now - lastEventTime > timerDelay || lastEventTime == 0)) {
    lastEventTime = now;
#if defined(WIFI) && defined(NTP)
    // Get current time from NTP-synchronized system clock if available
    if (isNtpSyncSuccessful()) {
      // Use the NTP-synchronized time
      lastUpdate = getEpochTime();
    } else if (updateTime > 0) {
      // Fallback: Use the browser's timestamp as base and add elapsed milliseconds
      lastUpdate = updateTime + (now - lastEventTime);
    } else {
      // If neither NTP nor browser time is available, use millis() as last resort
      // This will be incorrect since ESP32 millis() is not a Unix timestamp
      lastUpdate = now;
    }
    
    // kind of annoying that it resyncs right after startup sync but I guess I can live with it for now
    // Periodically resync NTP time (once per day)
    static unsigned long lastNTPSync = 0;
    if (isNtpSyncSuccessful() && (now - lastNTPSync > 86400000 || lastNTPSync == 0)) {
      resyncNTP();
      lastNTPSync = now;
    }
#endif
#if 0
    // Use system time directly instead of lastUpdate
    time_t now_time;
    time(&now_time);
    ptm = localtime(&now_time);
    sprintf(prbuf,"%d [%02d/%02d %02d:%02d:%02d] ",loopcount,ptm->tm_mon+1,ptm->tm_mday,ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    log::toAll(String(prbuf));
    consLog.flush();
#endif
    float shuntvoltage = 0;
    float busvoltage = 0;
    float current_mA = 0;
    float loadvoltage = 0;
    int shuntRead;
    shuntvoltage = -ina219.getShuntVoltage_mV();
    if (shuntvoltage > 0)
      shuntAvg.reading((int)(shuntvoltage*1000));
    shuntRead = shuntAvg.getAvg();
    busvoltage = ina219.getBusVoltage_V();
    current_mA = -ina219.getCurrent_mA();
    loadvoltage = busvoltage + (shuntvoltage / 1000);
    if (shuntvoltage > 15) {
      trigger = true;
      time_t now_time;
      time(&now_time);
      struct tm *timeinfo = localtime(&now_time);
      char timeStr[64];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
      log::toAll("VOLTAGE SPIKE DETECTED at " + String(timeStr) + "! ShuntV: " + String(shuntvoltage) + "mV, Avg: " + String(shuntRead));
    }
#if 0
    // Voltage spike detection trigger using percentage over moving average
    float shuntReadFloat = shuntRead / 1000.0; // Convert back to mV for comparison
    float spikeThreshold = 5.0; // 500% increase over average (5x multiplier)
    
    if (abs(shuntReadFloat) > 0.0) { // Only check if we have a meaningful baseline (> 1mV)
      float currentRatio = abs(shuntvoltage) / abs(shuntReadFloat);
      if (currentRatio > spikeThreshold) {
        if (!trigger) {
          trigger = true;
          time_t now_time;
          time(&now_time);
          struct tm *timeinfo = localtime(&now_time);
          char timeStr[64];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
          log::toAll("VOLTAGE SPIKE DETECTED at " + String(timeStr) + "! ShuntV: " + String(shuntvoltage) + "mV, Avg: " + String(shuntReadFloat) + "mV, Ratio: " + String(currentRatio) + "x");
        }
      } else if (trigger && currentRatio < 2.0) {
        // Reset trigger when voltage returns to within 2x of average
        trigger = false;
        //log::toAll("Voltage spike ended - trigger reset. Ratio: " + String(currentRatio) + "x");
        Serial.printf("*** TRIGGER RESET ***\n");
      }
    }
#endif    
    //Serial.print(">Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
    //Serial.print(">ShuntV: "); Serial.println(shuntvoltage);
    //Serial.print(">ShuntVAvg: "); Serial.println(shuntReadFloat);
    //Serial.print(">Trigger: "); Serial.println(trigger ? "TRUE" : "FALSE");
    //Serial.print(">Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
    //Serial.print(">Current:       "); Serial.print(current_mA); Serial.println(" mA");
    //Serial.println("");
#if defined(WIFI)
    // Push latest readings to any connected web clients
    readings["lastUpdate"] = String(lastUpdate);
    events.send(getSensorReadings().c_str(), "new_readings", now);
#endif
    // Flush queued log lines to LittleFS
    log::flush();
  } // end of timerDelay
/*
  if (Serial.available() > 0) {
    String input = "";
    while (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        // Ignore newline and carriage return characters
        break;
      }
      input += c;
    }
    if (input.length() > 0) {
      Serial.print("Received: ");
      Serial.println(input);
      if (input == "value") {
        // do something with serial input
        Serial.println("Value command received");
      }
    }
  }
*/  
#ifdef DEEPSLEEP
    // Check if it's time to go to sleep
    if ((millis() - startTime) > (awakeTimer * 1000)) {
      log::toAll("Going to sleep in 5 seconds...");

      // Flush any pending data
      log::flush();
  #ifdef WEBSERIAL
      WebSerial.flush();
  #endif

      // Give time for final communications
      delay(5000);

      // Enter deep sleep
      log::toAll("Entering deep sleep for " + String(TIME_TO_SLEEP) + " seconds");
      esp_deep_sleep_start();
      // Code after this point will not be executed
    }
#endif
  //delay(loopDelay);
}
