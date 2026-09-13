#ifdef COON
#include "include-general.h"
#include "esp_log.h"

bool relayState;
bool trigger = false;

Preferences prefs;

#ifdef INA219
Adafruit_INA219 ina219;
movingAvg shuntAvg(10);
bool ina219Found = false;  // set by ina219.begin() in setup()

// Read and log all available INA219 measurements. Uses log::toAll so the output
// reaches the serial console, the on-device console log, and WebSerial. The
// `context` string is prefixed so you can tell what triggered the reading
// (e.g. "startup", "relay on", "relay off", "schedule").
void logIna219Diagnostics(const char* context) {
  if (!ina219Found) {
    snprintf(logbuf, LOGBUF_SIZE,
      "INA219 [%s]: not detected on I2C bus (check wiring/address)", context);
    log::toAll(logbuf);
    return;
  }

  float shuntvoltage_mV = ina219.getShuntVoltage_mV();
  float busvoltage_V    = ina219.getBusVoltage_V();
  float current_mA      = ina219.getCurrent_mA();
  float power_mW        = ina219.getPower_mW();
  // Load voltage = bus voltage plus the drop across the shunt (shunt mV -> V)
  float loadvoltage_V   = busvoltage_V + (shuntvoltage_mV / 1000.0);

  snprintf(logbuf, LOGBUF_SIZE,
    "INA219 [%s]: shunt=%.3f mV  bus=%.3f V  load=%.3f V  current=%.3f mA  power=%.3f mW  ok=%s",
    context, shuntvoltage_mV, busvoltage_V, loadvoltage_V, current_mA, power_mW,
    ina219.success() ? "yes" : "no");
  log::toAll(logbuf);
}
#endif // INA219

void setupCoon() {
  pinMode(relayGPIO,OUTPUT);
  // Put the relay in its inactive state explicitly. This avoids leaving the
  // output at the GPIO's power-up level during startup.
  digitalWrite(relayGPIO, RELAY_NO ? LOW : HIGH);
  relayState = false;
}

void loopCoon() {
#ifdef INA219
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
#endif // INA219
}
#endif // COON
