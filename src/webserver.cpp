#ifdef WIFI
#include "include-general.h"

bool serverStarted;
JsonDocument readings;

String host = "coondefense";

String getSensorReadings() {
  readings["sensor"] = "0";
  String jsonString;
  serializeJson(readings,jsonString);
  return jsonString;
}

String getRelayState();

// Replaces placeholder with button section in your web page
String processor(const String& var) {
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

#endif // WIFI
