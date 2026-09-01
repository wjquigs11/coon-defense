#include <LittleFS.h>
#include "include-general.h"
#include "logto.h"

char logbuf[LOGBUF_SIZE];

extern bool serverStarted;
File consLog;

bool log::logToSerial = true;

#define CONSOLE_LOG_MAX 8192  // truncate when log exceeds this size

// Thread-safe queue for deferred file writes
static QueueHandle_t logQueue = NULL;

static void ensureQueue() {
  if (logQueue == NULL) {
    logQueue = xQueueCreate(LOG_RING_SIZE, LOG_LINE_MAX);
  }
}

void log::toAll(const char* s) {
  if (logToSerial) {
    Serial.println(s);
#ifdef WEBSERIAL
    if (serverStarted) {
      WebSerial.println(s);
    }
#endif
    // Buffer the line for deferred file write (with timestamp)
    ensureQueue();
    if (logQueue) {
      char line[LOG_LINE_MAX];
      time_t now_t = time(nullptr);
      struct tm tm;
      localtime_r(&now_t, &tm);
      int offset = snprintf(line, LOG_LINE_MAX, "%02d:%02d:%02d ",
        tm.tm_hour, tm.tm_min, tm.tm_sec);
      strncpy(line + offset, s, LOG_LINE_MAX - offset - 1);
      line[LOG_LINE_MAX - 1] = '\0';
      xQueueSend(logQueue, line, 0);  // non-blocking, drops if full
    }
  }
}

// Call from loop() to flush buffered lines to LittleFS
void log::flush() {
  if (!consLog || !logQueue) return;
  char line[LOG_LINE_MAX];
  while (xQueueReceive(logQueue, line, 0) == pdTRUE) {
    consLog.println(line);
  }
  consLog.flush();
}

bool log::initConsole(String conslogName) {
  ensureQueue();

  // If log exists and is too large, delete it
  if (LittleFS.exists(conslogName)) {
    File f = LittleFS.open(conslogName, "r");
    if (f && f.size() > CONSOLE_LOG_MAX) {
      f.close();
      LittleFS.remove(conslogName);
      Serial.println("console.log truncated (size limit reached)");
    } else if (f) {
      f.close();
    }
  }

  // Create file if it doesn't exist
  if (!LittleFS.exists(conslogName)) {
    File f = LittleFS.open(conslogName, "w");
    if (!f) {
      Serial.println("initConsole: failed to create file");
      return false;
    }
    f.close();
  }

  consLog = LittleFS.open(conslogName, "a");
  if (!consLog) return false;
  return consLog.println("ESP console log.") > 0;
}
