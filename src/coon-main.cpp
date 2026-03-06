
#include "include-general.h"
#include "esp_log.h"

bool relayState;
bool trigger = false;

File consLog;
Preferences coonPrefs;

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
struct tm *ptm;
char prbuf[PRBUF]; // PRBUF needs to be defined in include.h

Adafruit_INA219 ina219;
movingAvg shuntAvg(10);

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
  Serial.println(SDA);
  Serial.println(SCL);
  ina219.begin();  
  shuntAvg.begin();

  // Enable WiFi debugging
  //Serial.println("Enabling WiFi debugging...");
  //esp_log_level_set("wifi", ESP_LOG_VERBOSE);
#ifdef WIFI
  if (LittleFS.begin()) {
    Serial.println("opened LittleFS");
    checkLittleFS();
    gotWifiCreds = readWiFiCredentials();
  } else {
    Serial.println("failed to open LittleFS");
  }

  consLog = LittleFS.open("/console.log", "a", true);
  if (!consLog) {
    log::toAll("failed to open console log");
  }
  if (consLog.println("ESP console log.")) {
    log::toAll("console log written");
  } else {
    log::toAll("console log write failed");
  }
#endif
  // Increment boot number and print it every reboot
  ++bootCount;
  log::toAll("Boot number: " + String(bootCount));

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
    
    // Start the web server regardless of WiFi connection status
    // In fallback mode, it will serve from AP mode
    startWebServer();
    serverStarted = true;
    
#ifdef ELEGANTOTA
    ElegantOTA.begin(&server);
#endif
#ifdef WEBSERIAL
    WebSerial.begin(&server);
    // Attach a callback function to handle incoming messages
    WebSerial.onMessage(WebSerialonMessage);
#endif
    
    log::toAll("HTTP server started");
    
    // Load schedules from LittleFS
    loadSchedules();
    
    // mDNS will be initialized by WiFi event handler when connected
  }
#endif // WIFI
  consLog.flush();
  pinMode(relayGPIO,OUTPUT);
  if (RELAY_NO) digitalWrite(relayGPIO,LOW);
}

static int loopcount = 0;
unsigned long now;

void loop() {
#ifdef ELEGANTOTA
  ElegantOTA.loop();
#endif
#ifdef WEBSERIAL
  WebSerial.loop();
#endif
  now = millis();
  static unsigned long lastEventTime, lastTimeTime, startTime;
  // loop ultrasonic water
  loopWater();
#ifdef WIFI
  ArduinoOTA.handle();
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
    dnsServer.processNextRequest(); // for captive portal
  // update web page
  static bool drdCleared = false;
  static bool wifiTimeoutChecked = false;
  
  // Clear DRD flag after DRD_TIMEOUT seconds for double reset detection
  // unless reset occurs within the timeout period
  if (!drdCleared && (now > (DRD_TIMEOUT * 1000))) {
    coonPrefs.putBool("DRD", false);
    drdCleared = true;
    log::toAll("DRD timeout - cleared double reset flag");
  }
  
  // Check for WiFi connection timeout (20 seconds - increased for better reliability)
  if (wifiEnabled && !wifiConnected && !wifiTimeoutChecked && (now - wifiStartTime > 20000)) {
    wifiTimeoutChecked = true;
    wl_status_t status = WiFi.status();
    log::toAll("WiFi timeout check - Status: " + String(status) +
      " (" + (status == WL_CONNECTED ? "CONNECTED" :
              status == WL_NO_SSID_AVAIL ? "NO_SSID_AVAIL" :
              status == WL_CONNECT_FAILED ? "CONNECT_FAILED" :
              status == WL_CONNECTION_LOST ? "CONNECTION_LOST" :
              status == WL_DISCONNECTED ? "DISCONNECTED" :
              status == WL_IDLE_STATUS ? "IDLE" : "UNKNOWN") + ")");
    
    if (status != WL_CONNECTED) {
      log::toAll("WiFi connection timeout after 20 seconds - stopping STA mode");
      // Stop the connection timer if it's still running
      extern TimerHandle_t connectTimer;
      if (connectTimer != NULL) {
        xTimerDelete(connectTimer, 0);
        connectTimer = NULL;
      }
      // Stop STA mode before starting AP
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(100);
      log::toAll("Starting captive portal due to timeout");
      startPortal();
    }
  }
  
  // Check schedule events
  checkScheduleEvents();
#endif
  if (now - lastEventTime > timerDelay || lastEventTime == 0) {
    lastEventTime = now;
#if defined(WIFI) && defined(NTP00)
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
    
    // Send the timestamp in milliseconds since epoch (Unix timestamp)
    readings["lastUpdate"] = String(lastUpdate);
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
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
      consLog.flush();
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
