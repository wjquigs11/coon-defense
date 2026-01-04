#ifdef WEBSERIAL
#include "include-general.h"

/* app-specific webserial handlers
    This allows me to avoid changes in webserial.cpp that would cause conflicts when merging from main into a project-specific branch
    All changes should be made in this file.
    add #define for APPHANDLER in platformio.ini if you are going to use app-specific handlers
    TBD: change argument to an array of Strings
*/
#ifndef APPHANDLER
Handler appHandler = nullptr;
Handler togHandler = nullptr;
String appCommandList[] = {};
String appToggleList[] = {};
#else
String appCommandList[] = {"hello", "test", ""};
String appToggleList[] = {"myfeature", ""};

// Example application-specific command handler
void myAppHandler(String command) {
  if (command.startsWith("hel")) {
    log::toAll("Hello from app handler!");
    return;
  }
  if (command.startsWith("test")) {
    log::toAll("Test command received");
    return;
  }
  // Add more application-specific commands here
  log::toAll("Unknown app command: " + command);
}

// Example toggle handler for application-specific toggles
void myToggleHandler(String toggle) {
  if (toggle.startsWith("myfea")) {
    static bool myFeatureEnabled = false;
    myFeatureEnabled = !myFeatureEnabled;
    log::toAll("My feature: " + String(myFeatureEnabled ? "enabled" : "disabled"));
    return;
  }
  // Add more application-specific toggles here
  log::toAll("Unknown toggle: " + toggle);
}

// Define the handler function pointers that webserial.cpp expects
Handler appHandler = myAppHandler;
Handler togHandler = myToggleHandler;

#endif // APPHANDLER
#endif