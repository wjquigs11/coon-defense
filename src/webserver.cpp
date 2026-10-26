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
#ifdef COON
    String relayStateValue = getRelayState();
    buttons= "<h4>Relay #1 - GPIO " + String(relayGPIO) + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"1\" "+ relayStateValue +"><span class=\"slider\"></span></label>";
#endif
    return buttons;
  }
  return String();
}

#ifdef COON
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
#endif

// Current status as JSON (served on /readings and pushed via /events)
String getSensorReadings() {
  // Clear previous readings to prevent nesting
  readings.clear();
#ifdef COON
  readings["relay"] = relayState ? "on" : "off";
#endif
  readings["time"] = String(millis());
  String jsonString;
  serializeJson(readings, jsonString);
  return jsonString;
}

#endif // WIFI
