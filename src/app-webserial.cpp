#ifdef WEBSERIAL
#include "include-general.h"

/* app-specific webserial handlers
    This allows me to avoid changes in webserial.cpp that would cause conflicts when merging from main into a project-specific branch.
    All app-specific changes should be made in this file.
    Add #define for APPHANDLER in platformio.ini if you are going to use app-specific handlers.

    Handler signature: void(*)(String* words, int totalWords)
      words[0] is the command (or first sub-word); totalWords is the count.
*/
#ifndef APPHANDLER
Handler appHandler = nullptr;
Handler togHandler = nullptr;
String appCommandList[] = {""};
String appToggleList[] = {""};
#else
String appCommandList[] = {"hello", "test", ""};
String appToggleList[] = {"myfeature", ""};

// Example application-specific command handler
void myAppHandler(String* words, int totalWords) {
  String command = words[0];
  if (command.startsWith("hel")) {
    log::toAll("Hello from app handler!");
    return;
  }
  if (command.startsWith("test")) {
    log::toAll("Test command received");
    return;
  }
  // Add more application-specific commands here
  snprintf(logbuf, LOGBUF_SIZE, "Unknown app command: %s", command.c_str());
  log::toAll(logbuf);
}

// Example toggle handler for application-specific toggles
void myToggleHandler(String* words, int totalWords) {
  String toggle = words[0];
  if (toggle.startsWith("myfea")) {
    static bool myFeatureEnabled = false;
    myFeatureEnabled = !myFeatureEnabled;
    snprintf(logbuf, LOGBUF_SIZE, "My feature: %s", myFeatureEnabled ? "enabled" : "disabled");
    log::toAll(logbuf);
    return;
  }
}

Handler appHandler = myAppHandler;
Handler togHandler = myToggleHandler;

#endif // APPHANDLER
#endif
