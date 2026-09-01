#ifdef WIFI
#include "include-general.h"

bool serverStarted;
JsonDocument readings;

String host = "back";

String getRelayState();

// Replaces placeholder with button section in your web page
String coon_processor(const String& var) {
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    String relayStateValue = getRelayState();
    buttons= "<h4>Relay #1 - GPIO " + String(relayGPIO) + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"1\" "+ relayStateValue +"><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

String getRelayState() {
  if(RELAY_NO){
    if(digitalRead(relayGPIO)){
      return "";
    }
    else {
      return "checked";
    }
  }
  else {
    if(digitalRead(relayGPIO)){
      return "checked";
    }
    else {
      return "";
    }
  }
  return "";
}

// Get Sensor Readings and return JSON object
String getSensorReadings() {
  // Clear previous readings to prevent nesting
  readings.clear();

  // Use the cached water level (refreshed by the non-blocking loopWater sampler)
  // so this can be called from the web/event path without triggering a blocking read.
  int waterlevel = getCachedWaterLevel();
  readings["waterlevel"] = String(waterlevel);
  readings["distanceInch"] = String(distanceInch);
  readings["time"] = String(millis());
  String jsonString;
  serializeJson(readings, jsonString);
  return jsonString;
}

String rain_processor(const String& var) {
  log::toAll("processor: " + var);
  if (var == "DISTANCEINCH")
    return String(distanceInch,0);
  if (var == "TANKTOP")
    return String(tanktop, 2);
  if (var == "TANKBOTTOM")
    return String(tankbottom, 2);
  if (var == "CAPACITY")
    return String(capacity, 2);
  return String();
}

#endif // WIFI
