
#ifndef LOGTO_H
#define LOGTO_H

#include <Arduino.h>

#define LOGBUF_SIZE 256
extern char logbuf[LOGBUF_SIZE];

#define LOG_RING_SIZE 32
#define LOG_LINE_MAX 128

class log {
public:
    static bool logToSerial;
    log() {}
    static void toAll(const char* s);
    // Compatibility overload so existing `log::toAll("x " + String(y))`
    // call sites continue to work after the migration to const char*.
    static void toAll(const String& s) { toAll(s.c_str()); }
    static void flush();  // call from loop() to write buffered lines to file
    static bool initConsole(String conslogName = "/console.log");
};
#endif
