#include "include-general.h"
#ifdef WEBSERIAL

bool debugFlag = false;

// ─── Utilities ─────────────────────────────────────────────────────────────────

static int ASIZE(String* arr) {
  int count = 0;
  while (!arr[count].isEmpty()) count++;
  return count;
}

static String formatMacAddress(const String& macAddress) {
  String result = "{";
  for (int i = 0; i < (int)macAddress.length(); i += 3) {
    if (i > 0) result += ", ";
    result += "0x" + macAddress.substring(i, i + 2);
  }
  result += "};";
  return result;
}

static void i2cScan(TwoWire& wire) {
  byte error, address;
  int nDevices = 0;

  log::toAll("Scanning...");

  for (address = 1; address < 127; address++) {
    wire.beginTransmission(address);
    error = wire.endTransmission();
    sprintf(prbuf, "%2X", address);

    if (error == 0) {
      snprintf(logbuf, LOGBUF_SIZE, "I2C device found at address 0x%s", prbuf);
      log::toAll(logbuf);
      nDevices++;
    } else if (error == 4) {
      snprintf(logbuf, LOGBUF_SIZE, "error at address 0x%s", prbuf);
      log::toAll(logbuf);
    }
  }

  if (nDevices == 0)
    log::toAll("No I2C devices found");
  else {
    snprintf(logbuf, LOGBUF_SIZE, "%d device(s) found", nDevices);
    log::toAll(logbuf);
  }
}

// ─── Command lists ─────────────────────────────────────────────────────────────

static String commandList[] = {"restart", "hostname", "status", "wifi", "conslog", "littlefs", "toggle", "timer", "scan", "on", "off", ""};
static String toggleList[] = {"debug", "log", ""};

// ─── Command handler ───────────────────────────────────────────────────────────

static void handleCommand(String dataS) {
  dataS.trim();
  if (dataS.isEmpty()) return;

  // Tokenize
  String words[10];
  int wordCount = 0;
  int startIndex = 0, endIndex = 0;
  while (endIndex != -1 && wordCount < 10) {
    endIndex = dataS.indexOf(' ', startIndex);
    words[wordCount] = (endIndex == -1)
      ? dataS.substring(startIndex)
      : dataS.substring(startIndex, endIndex);
    words[wordCount].trim();
    words[wordCount].toLowerCase();
    wordCount++;
    startIndex = endIndex + 1;
  }
  if (wordCount == 0) return;

  String cmd = words[0];

  // ─── Help ────────────────────────────────────────────────────────────────
  if (cmd == "?" || cmd == "help") {
    int pos = snprintf(logbuf, LOGBUF_SIZE, "commands:");
    for (int j = 0; j < ASIZE(commandList); j++)
      pos += snprintf(logbuf + pos, LOGBUF_SIZE - pos, " %s", commandList[j].c_str());
    log::toAll(logbuf);
    if (ASIZE(appCommandList) > 0) {
      pos = snprintf(logbuf, LOGBUF_SIZE, "app commands:");
      for (int j = 0; j < ASIZE(appCommandList); j++)
        pos += snprintf(logbuf + pos, LOGBUF_SIZE - pos, " %s", appCommandList[j].c_str());
      log::toAll(logbuf);
    }
    return;
  }

  // ─── Restart ─────────────────────────────────────────────────────────────
  if (cmd == "r" || cmd == "restart") {
    log::toAll("restarting...");
    delay(100);
    ESP.restart();
  }

  // ─── Relay control (coon-defense) ─────────────────────────────────────────
  if (cmd == "on") {
    relayState = true;
    digitalWrite(relayGPIO, RELAY_NO ? LOW : HIGH);
    log::toAll("relay on");
    return;
  }
  if (cmd == "off") {
    relayState = false;
    digitalWrite(relayGPIO, RELAY_NO ? HIGH : LOW);
    log::toAll("relay off");
    return;
  }

  // ─── I2C Scan ────────────────────────────────────────────────────────────
  if (cmd == "scan") {
    i2cScan(Wire);
    return;
  }

  // ─── Hostname ────────────────────────────────────────────────────────────
  if (cmd.startsWith("host")) {
    if (wordCount > 1 && words[1].length() > 0) {
      if (words[1].length() > 63) {
        log::toAll("hostname too long (max 63 chars)");
        return;
      }
      host = words[1];
      coonPrefs.putString("hostname", host);
      snprintf(logbuf, LOGBUF_SIZE, "hostname set to %s (restart to apply)", host.c_str());
      log::toAll(logbuf);
    } else {
      snprintf(logbuf, LOGBUF_SIZE, "hostname: %s  ip: %s", host.c_str(), WiFi.localIP().toString().c_str());
      log::toAll(logbuf);
    }
    return;
  }

  // ─── Timer ───────────────────────────────────────────────────────────────
  if (cmd.startsWith("time")) {
    if (wordCount > 1 && words[1].length() > 0) {
      timerDelay = atoi(words[1].c_str());
      if (timerDelay < 100) timerDelay = 100;
      coonPrefs.putInt("timerdelay", timerDelay);
      snprintf(logbuf, LOGBUF_SIZE, "timerDelay set to %d", timerDelay);
      log::toAll(logbuf);
    } else {
      snprintf(logbuf, LOGBUF_SIZE, "timerDelay: %d", timerDelay);
      log::toAll(logbuf);
    }
    return;
  }

  // ─── Status ──────────────────────────────────────────────────────────────
  if (cmd == "status" || cmd == "s") {
#ifdef NTP
    if (isNtpSyncSuccessful()) {
      snprintf(logbuf, LOGBUF_SIZE, "Time: %s", getFormattedTime().c_str());
      log::toAll(logbuf);
    }
#endif
    snprintf(logbuf, LOGBUF_SIZE, "hostname: %s  ip: %s", host.c_str(), WiFi.localIP().toString().c_str());
    log::toAll(logbuf);
    snprintf(logbuf, LOGBUF_SIZE, "WiFi: %s  RSSI: %d dBm",
      WiFi.status() == WL_CONNECTED ? "connected" : "disconnected", WiFi.RSSI());
    log::toAll(logbuf);
    snprintf(logbuf, LOGBUF_SIZE, "heap free: %u  min: %u", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    log::toAll(logbuf);
    snprintf(logbuf, LOGBUF_SIZE, "uptime: %lu s", millis() / 1000);
    log::toAll(logbuf);
    snprintf(logbuf, LOGBUF_SIZE, "relay: %s", relayState ? "on" : "off");
    log::toAll(logbuf);
    log::toAll(getSensorReadings().c_str());
    return;
  }

  // ─── WiFi info ───────────────────────────────────────────────────────────
  if (cmd.startsWith("wifi")) {
    if (wordCount > 1 && words[1] == "reset") {
      resetWifi();
      return;
    }
    if (wordCount > 2 && words[1] == "ssid") {
      newWifiSsid = words[2];
      log::toAll("new ssid staged");
      if (newWifiSsid.length() > 0 && newWifiPass.length() > 0) {
        saveNewWifiCredentials(newWifiSsid.c_str(), newWifiPass.c_str());
        snprintf(logbuf, LOGBUF_SIZE, "saved wifi ssid: %s", newWifiSsid.c_str());
        log::toAll(logbuf);
        newWifiSsid = "";
        newWifiPass = "";
      }
      return;
    }
    if (wordCount > 2 && words[1] == "pass") {
      newWifiPass = words[2];
      log::toAll("new wifi password staged");
      if (newWifiSsid.length() > 0 && newWifiPass.length() > 0) {
        saveNewWifiCredentials(newWifiSsid.c_str(), newWifiPass.c_str());
        snprintf(logbuf, LOGBUF_SIZE, "saved wifi ssid: %s", newWifiSsid.c_str());
        log::toAll(logbuf);
        newWifiSsid = "";
        newWifiPass = "";
      }
      return;
    }
    if (wordCount > 2 && words[1] == "del") {
      snprintf(logbuf, LOGBUF_SIZE, "deleting wifi %s", words[2].c_str());
      log::toAll(logbuf);
      deleteWifiCredentials(words[2].c_str());
      return;
    }
    snprintf(logbuf, LOGBUF_SIZE, "hostname: %s  SSID: %s  ip: %s  MAC: %s",
      host.c_str(), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
      formatMacAddress(WiFi.macAddress()).c_str());
    log::toAll(logbuf);
    return;
  }

  // ─── LittleFS ────────────────────────────────────────────────────────────
  if (cmd.startsWith("littlefs")) {
    if (wordCount > 1) {
      if (words[1] == "ls") {
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
          snprintf(logbuf, LOGBUF_SIZE, "  %s (%d bytes)", file.name(), (int)file.size());
          log::toAll(logbuf);
          file.close();
          file = root.openNextFile();
        }
        root.close();
        return;
      }
      if (words[1] == "status") {
        size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
        snprintf(logbuf, LOGBUF_SIZE, "LittleFS: %u/%u bytes (%.1f%%)", (unsigned)used, (unsigned)total, 100.0f * used / total);
        log::toAll(logbuf);
        return;
      }
      if (words[1] == "read" && wordCount > 2) {
        String path = words[2];
        if (!path.startsWith("/")) path = "/" + path;
        File file = LittleFS.open(path);
        if (!file || file.isDirectory()) { log::toAll("failed to open"); return; }
        while (file.available()) {
          String line = file.readStringUntil('\n');
          log::toAll(line.c_str());
        }
        file.close();
        return;
      }
      if (words[1] == "rm" && wordCount > 2) {
        String path = words[2];
        if (!path.startsWith("/")) path = "/" + path;
        if (LittleFS.exists(path)) {
          log::toAll(LittleFS.remove(path) ? "removed" : "failed to remove");
        } else {
          log::toAll("not found");
        }
        return;
      }
      if (words[1] == "format") {
        LittleFS.format();
        log::toAll("LittleFS formatted");
        return;
      }
    }
    log::toAll("littlefs {ls|status|read <file>|rm <file>|format}");
    return;
  }

  // ─── Console log ─────────────────────────────────────────────────────────
  if (cmd.startsWith("conslog")) {
    if (wordCount > 1 && words[1].startsWith("reset")) {
      consLog.close();
      log::initConsole();
      log::toAll("console log reset");
    } else {
      File logFile = LittleFS.open("/console.log", "r");
      if (!logFile) { log::toAll("failed to open console log"); return; }
      const int maxLines = 20;
      String lineBuffer[maxLines];
      int lineCount = 0, bufferIndex = 0;
      while (logFile.available()) {
        lineBuffer[bufferIndex] = logFile.readStringUntil('\n');
        bufferIndex = (bufferIndex + 1) % maxLines;
        if (lineCount < maxLines) lineCount++;
      }
      for (int k = 0; k < lineCount; k++)
        log::toAll(lineBuffer[(bufferIndex + k) % maxLines].c_str());
      logFile.close();
    }
    return;
  }

  // ─── Toggle ──────────────────────────────────────────────────────────────
  if (cmd.startsWith("tog")) {
    if (wordCount > 1) {
      if (words[1].startsWith("debu")) {
        debugFlag = !debugFlag;
        snprintf(logbuf, LOGBUF_SIZE, "debug: %s", debugFlag ? "on" : "off");
        log::toAll(logbuf);
        return;
      }
      if (words[1] == "log") {
        log::logToSerial = !log::logToSerial;
        snprintf(logbuf, LOGBUF_SIZE, "serial log: %s", log::logToSerial ? "on" : "off");
        log::toAll(logbuf);
        return;
      }
      if (togHandler) {
        togHandler(&words[1], wordCount - 1);
        return;
      }
    }
    log::toAll("toggle {debug|log}");
    return;
  }

  // ─── App-specific handler ────────────────────────────────────────────────
  if (appHandler) {
    appHandler(&words[0], wordCount);
    return;
  }

  snprintf(logbuf, LOGBUF_SIZE, "Unknown command: %s", cmd.c_str());
  log::toAll(logbuf);
}

// ─── Entry points ──────────────────────────────────────────────────────────────

void WebSerialonMessage(uint8_t* data, size_t len) {
  String input = String((char*)data).substring(0, len);
  handleCommand(input);
}

void pollSerialConsole() {
  static String serialBuf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (serialBuf.length() > 0) {
        handleCommand(serialBuf);
        serialBuf = "";
      }
    } else {
      serialBuf += c;
    }
  }
}

#endif // WEBSERIAL
