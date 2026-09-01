#ifdef WIFI
/*
Start wifi. If there are credentials saved in a file, use them.
If not, start a captive portal (using DNS).
*/
#include "include-general.h"

bool gotWifiCreds = false;
const char* wifiPath = "/wifi.json";

WiFiCredentials wifi[MAX_WIFI_NETWORKS];
int wifiCount = 0;

// Staging area for webserial wifi add (set ssid and pass separately, save when both present)
String newWifiSsid;
String newWifiPass;

TimerHandle_t connectTimer = NULL;
static int connectIdx = 0;
static int connectCount = 0;
#define RETRY_PER_SSID 3

// Flag for wifiConnectTask to poll
static volatile bool wifiConnectPending = false;

bool littleFSDebug = true;   // TEMP: enabled to diagnose wifi.json first-read issue

// check LittleFS even if wifi is not enabled
void checkLittleFS() {
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  Serial.print("LittleFS Total space: ");
  Serial.print(totalBytes);
  Serial.println(" bytes");
  Serial.print("LittleFS Used space: ");
  Serial.print(usedBytes);
  Serial.println(" bytes");
  Serial.print("LittleFS Free space: ");
  Serial.print(totalBytes - usedBytes);
  Serial.println(" bytes");
  float usedPercentage = ((float)usedBytes / totalBytes) * 100;
  Serial.print("LittleFS Usage: ");
  Serial.print(usedPercentage);
  Serial.println("%");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  Serial.println("Files on LittleFS:");
  while (file) {
    Serial.print("  ");
    Serial.print(file.name());
    Serial.print(" - ");
    Serial.print(file.size());
    Serial.println(" bytes");
    file = root.openNextFile();
  }
}

// Read WiFi credentials from wifi.json file
bool readWiFiCredentials() {
  File file = LittleFS.open(wifiPath, "r");
  if (!file) {
      Serial.println("Failed to open wifi.json");

      if (!LittleFS.exists(wifiPath)) {
          Serial.println("Error: wifi.json does not exist");
      }
      else if (file.isDirectory()) {
          Serial.println("Error: Path is a directory, not a file");
      }
      else if (!LittleFS.begin()) {
          Serial.println("Error: LittleFS not mounted");
      }
      else if (LittleFS.totalBytes() == 0) {
          Serial.println("Error: LittleFS has no space");
      }
      else {
          Serial.println("Error: Unknown file open error (corruption/permissions?)");
      }
      return false;
  }

  // Read the whole file into a String first so we can dump the raw bytes for
  // diagnostics, then parse from that String (equivalent to parsing the file).
  String rawJson = file.readString();
  file.close();

  if (littleFSDebug) {
    snprintf(logbuf, LOGBUF_SIZE, "wifi.json raw (%d bytes): %s",
      (int)rawJson.length(), rawJson.c_str());
    log::toAll(logbuf);
  }

  // Parse JSON array from the raw contents
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, rawJson);

  if (error) {
    snprintf(logbuf, LOGBUF_SIZE, "Failed to parse wifi.json: %s", error.c_str());
    log::toAll(logbuf);
    return false;
  }

  if (littleFSDebug) {
    snprintf(logbuf, LOGBUF_SIZE, "wifi.json parsed: isArray=%d, size=%d",
      (int)doc.is<JsonArray>(), (int)doc.as<JsonArray>().size());
    log::toAll(logbuf);
  }

  JsonArray arr = doc.as<JsonArray>();
  wifiCount = 0;

  for (JsonObject obj : arr) {
    if (wifiCount >= MAX_WIFI_NETWORKS) break;

    if (!obj["ssid"].is<const char*>()) {
      if (littleFSDebug) {
        log::toAll("skipping entry: ssid missing or not a string (is<const char*> false)");
      }
      continue;
    }

    wifi[wifiCount].ssid = obj["ssid"].as<String>();
    wifi[wifiCount].password = obj["password"] | "";
    wifi[wifiCount].ip = obj["ip"] | "";
    wifi[wifiCount].gateway = obj["gateway"] | "";

    snprintf(logbuf, LOGBUF_SIZE, "WiFi[%d] - SSID: %s, Password length: %d, IP: %s, Gateway: %s",
      wifiCount, wifi[wifiCount].ssid.c_str(), (int)wifi[wifiCount].password.length(),
      wifi[wifiCount].ip.c_str(), wifi[wifiCount].gateway.c_str());
    log::toAll(logbuf);

    wifiCount++;
  }

  if (wifiCount > 0) {
    snprintf(logbuf, LOGBUF_SIZE, "Loaded %d WiFi network(s)", wifiCount);
    log::toAll(logbuf);
    return true;
  }

  log::toAll("No valid WiFi credentials found in wifi.json");
  return false;
}

// for captive portal
DNSServer dnsServer;

// Search for parameter in HTTP POST request
const char* PARAM_HOSTNAME = "hostname";
const char* PARAM_SSID = "ssid";
const char* PARAM_PASSWORD = "pass";
const char* PARAM_IP = "ip";
const char* PARAM_GATEWAY = "gateway";

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
  // Set the TTL for DNS response and start the DNS server
  log::toAll("starting DNS server for captive portal");
  dnsServer.setTTL(3600);
  dnsServer.start(53, "*", localIP);
  // we modify loop() delay here; not sure if this is necessary
  loopDelay = DNS_INTERVAL;
}

void startPortal() {
    // Connect to Wi-Fi network with SSID and password
    log::toAll("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-SETUP", NULL);

    IPAddress IP = WiFi.softAPIP();
    snprintf(logbuf, LOGBUF_SIZE, "AP IP address: %s", IP.toString().c_str());
    log::toAll(logbuf);

    setUpDNSServer(dnsServer, IP);
  String localIPURL = "http://" + IP.toString();

  // https://github.com/CDFER/Captive-Portal-ESP32
  //======================== Webserver ========================
  // Required
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });  // windows 11 captive portal workaround
  server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });

  // Background responses
  server.on("/generate_204", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });       // android captive portal redirect
  server.on("/redirect", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });           // microsoft redirect
  server.on("/hotspot-detect.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
  server.on("/canonical.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });     // firefox captive portal call home
  server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });                   // firefox captive portal call home
  server.on("/ncsi.txt", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });           // windows call home

  // return 404 to webpage icon
  server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });  // webpage icon

  // the catch all
  server.onNotFound([localIPURL](AsyncWebServerRequest *request) {
    snprintf(logbuf, LOGBUF_SIZE, "CAPTIVE PORTAL onNotFound: %d %s %s -> redirecting",
      (int)request->method(), request->host().c_str(), request->url().c_str());
    log::toAll(logbuf);
    request->redirect(localIPURL);
  });

  // Web Server Root URL: serve wifimanager.html from captive portal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
  });

  // Return list of saved SSIDs as JSON array
  server.on("/wifilist", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    for (int i = 0; i < wifiCount; i++) {
      if (i > 0) json += ",";
      json += "\"";
      json += wifi[i].ssid;
      json += "\"";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // Delete selected SSIDs
  server.on("/wifidelete", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    int deleted = 0;
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if (p->isPost() && p->name() == "ssid") {
        deleteWifiCredentials(p->value().c_str());
        deleted++;
      }
    }
    snprintf(logbuf, LOGBUF_SIZE, "Deleted %d SSID(s) via web", deleted);
    log::toAll(logbuf);
    request->send(200, "text/plain", "OK");
  });

  server.on("/wifimanager", HTTP_POST, [](AsyncWebServerRequest *request) {
      String ssid, password, ip, gateway;
      int params = request->params();
      for (int i=0; i<params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()) {
          if (p->name() == PARAM_HOSTNAME && p->value().length() > 0) {
            host = p->value().c_str();
            coonPrefs.putString("hostname", host);
            snprintf(logbuf, LOGBUF_SIZE, "host set to: %s", host.c_str());
            log::toAll(logbuf);
          }
          if (p->name() == PARAM_SSID) {
            ssid = p->value().c_str();
            snprintf(logbuf, LOGBUF_SIZE, "SSID set to: %s", ssid.c_str());
            log::toAll(logbuf);
          }
          if (p->name() == PARAM_PASSWORD) {
            password = p->value().c_str();
            snprintf(logbuf, LOGBUF_SIZE, "Password set to: %s", password.c_str());
            log::toAll(logbuf);
          }
          if (p->name() == PARAM_IP) {
            ip = p->value().c_str();
            snprintf(logbuf, LOGBUF_SIZE, "IP Address set to: %s", ip.c_str());
            log::toAll(logbuf);
          }
          if (p->name() == PARAM_GATEWAY) {
            gateway = p->value().c_str();
            snprintf(logbuf, LOGBUF_SIZE, "Gateway set to: %s", gateway.c_str());
            log::toAll(logbuf);
          }
          snprintf(logbuf, LOGBUF_SIZE, "POST[%s]: %s", p->name().c_str(), p->value().c_str());
          log::toAll(logbuf);
        }
      }
      if (ssid.length() > 0 && password.length() > 0) {
        // Save the WiFi credentials to the JSON file (newest first)
        JsonDocument doc;
        JsonArray arr;

        if (LittleFS.exists(wifiPath)) {
          File readFile = LittleFS.open(wifiPath, FILE_READ);
          if (readFile) {
            DeserializationError error = deserializeJson(doc, readFile);
            readFile.close();
            if (error) {
              doc.clear();
            }
          }
        }
        arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc.to<JsonArray>();

        JsonDocument newEntry;
        newEntry["ssid"] = ssid;
        newEntry["password"] = password;
        if (ip.length() > 0) newEntry["ip"] = ip;
        if (gateway.length() > 0) newEntry["gateway"] = gateway;

        JsonDocument newDoc;
        JsonArray newArr = newDoc.to<JsonArray>();
        newArr.add(newEntry);
        for (JsonVariant v : arr) {
          newArr.add(v);
        }

        File file = LittleFS.open(wifiPath, FILE_WRITE);
        if (!file) {
          log::toAll("Failed to open wifi.json for writing");
        } else {
          serializeJsonPretty(newDoc, file);
          file.close();
          log::toAll("WiFi credentials saved to wifi.json (newest first)");
        }
      } else
        log::toAll("must supply ssid and password");
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
    delay(3000);
    ESP.restart();
  }); // end lambda for "/wifimanager"

  server.begin();
}

// WiFi event handlers
void onWiFiConnected() {
  log::toAll("WiFi connected!");
  snprintf(logbuf, LOGBUF_SIZE, "ESP IP Address: http://%s", WiFi.localIP().toString().c_str());
  log::toAll(logbuf);
  wifiConnected = true;

  // Store the successfully connected SSID for next boot priority
  String connectedSSID = WiFi.SSID();
  if (connectedSSID.length() > 0) {
    coonPrefs.putString("lastSSID", connectedSSID);
    snprintf(logbuf, LOGBUF_SIZE, "Stored lastSSID: %s", connectedSSID.c_str());
    log::toAll(logbuf);
  }
  if (connectTimer != NULL) {
    xTimerDelete(connectTimer, 0);
    connectTimer = NULL;
  }

#ifdef NTP
  log::toAll("Setting up time synchronization...");
  setupTime();
  waitForTimeSync(10); // async - returns immediately, logs result when done
#endif

  // Initialize mDNS
  String mdnsHost = host;
  mdnsHost.toLowerCase();
  mdnsHost.replace("_", "-");
  mdnsHost.replace(" ", "-");
  String sanitized = "";
  for (int i = 0; i < (int)mdnsHost.length() && i < 63; i++) {
    char c = mdnsHost.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      sanitized += c;
    }
  }
  if (sanitized.length() > 0) {
    MDNS.end();  // clean up any previous mDNS state
    snprintf(logbuf, LOGBUF_SIZE, "Starting MDNS with hostname: %s", sanitized.c_str());
    log::toAll(logbuf);
    if (!MDNS.begin(sanitized.c_str())) {
      log::toAll("Error starting MDNS responder");
    } else {
      snprintf(logbuf, LOGBUF_SIZE, "MDNS started successfully as %s.local", sanitized.c_str());
      log::toAll(logbuf);
      if (!MDNS.addService("http", "tcp", HTTP_PORT)) {
        log::toAll("MDNS add service failed");
      } else {
        log::toAll("MDNS service added successfully");
      }
    }
  }
  server.begin();
}

static int disConCount = 0;
void onWiFiDisconnected() {
  disConCount++;
  snprintf(logbuf, LOGBUF_SIZE, "WiFi disconnected disconnect count: %d", disConCount);
  log::toAll(logbuf);
  wifiConnected = false;
}

// ─── WiFi health check ─────────────────────────────────────────────────────────
static unsigned long lastGatewayCheck = 0;
static unsigned long lastGatewaySuccess = 0;
static const unsigned long GATEWAY_CHECK_INTERVAL = 60000;   // check every 60s
static const unsigned long GATEWAY_TIMEOUT = 300000;         // 5 min with no response = dead

static unsigned long lastWifiDisconnect = 0;
static int wifiFailCount = 0;

void wifiCheck() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiFailCount = 0;

    unsigned long now_ms = millis();
    if (now_ms - lastGatewayCheck >= GATEWAY_CHECK_INTERVAL) {
      lastGatewayCheck = now_ms;
      IPAddress gw = WiFi.gatewayIP();
      if (gw) {
        WiFiClient testClient;
        testClient.setTimeout(1);
        bool reachable = testClient.connect(gw, 1);
        testClient.stop();
        if (reachable || lastGatewaySuccess == 0) {
          lastGatewaySuccess = now_ms;
        }
      }
    }

    if (lastGatewaySuccess > 0 && (now_ms - lastGatewaySuccess >= GATEWAY_TIMEOUT)) {
      log::toAll("WiFi stack dead (gateway unreachable), forcing reconnect...");
      lastGatewaySuccess = now_ms;
      WiFi.disconnect(true);
      delay(100);
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifi[0].ssid, wifi[0].password);
    }
    return;
  }

  if (wifiFailCount == 0) {
    lastWifiDisconnect = millis();
    wifiFailCount = 1;
    return;
  }

  if (millis() - lastWifiDisconnect < 5000) {
    return;
  }

  if (wifiFailCount < 3) {
    wifiFailCount++;
    snprintf(logbuf, LOGBUF_SIZE, "WiFi disconnected, attempt %d...", wifiFailCount);
    log::toAll(logbuf);
    WiFi.reconnect();
    lastWifiDisconnect = millis();
    return;
  }

  log::toAll("WiFi reconnect failed, doing full reconnect...");
  wifiFailCount = 0;
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi[0].ssid, wifi[0].password);
}

// Task that calls WiFi.begin() with its own stack.
static void doWiFiBeginTask(void* param) {
  int idx = (int)param;
  WiFi.begin(wifi[idx].ssid, wifi[idx].password);
  vTaskDelete(NULL);
}

// Dedicated task for WiFi connection attempts
static void wifiConnectTask(void* param) {
  for (;;) {
    if (wifiConnectPending) {
      wifiConnectPending = false;

      if (wifiConnected) {
        log::toAll("Already connected, stopping connection attempts");
        xTimerStop(connectTimer, 0);
        continue;
      }

      snprintf(logbuf, LOGBUF_SIZE, "connecting to: %s attempt: %d/%d",
        wifi[connectIdx].ssid.c_str(), connectCount + 1, RETRY_PER_SSID);
      log::toAll(logbuf);

      xTaskCreate(doWiFiBeginTask, "wifiBegin", 4096, (void*)connectIdx, 1, NULL);

      if (++connectCount >= RETRY_PER_SSID) {
        connectCount = 0;
        connectIdx++;
        if (connectIdx >= wifiCount) {
          log::toAll("Failed to connect to any WiFi network, starting captive portal");
          xTimerDelete(connectTimer, 0);
          connectTimer = NULL;
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          delay(100);
          startPortal();
          continue;
        }
        snprintf(logbuf, LOGBUF_SIZE, "Moving to next SSID: %s", wifi[connectIdx].ssid.c_str());
        log::toAll(logbuf);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void tryConnect(TimerHandle_t xTimer) {
  wifiConnectPending = true;
}

bool setupWifi() {
  log::toAll("Starting WiFi...");
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    onWiFiConnected();
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    onWiFiDisconnected();
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  if (!gotWifiCreds) {
    log::toAll("No wifi credentials in wifi.json, starting captive portal...");
    startPortal();
    return false;
  } else {
    log::toAll("starting wifi");

    // Prioritize lastSSID from Preferences — move it to front of wifi[] array
    String lastSSID = coonPrefs.getString("lastSSID", "");
    if (lastSSID.length() > 0) {
      int foundIdx = -1;
      for (int i = 0; i < wifiCount; i++) {
        if (wifi[i].ssid == lastSSID) {
          foundIdx = i;
          break;
        }
      }
      if (foundIdx > 0) {
        WiFiCredentials tmp = wifi[0];
        wifi[0] = wifi[foundIdx];
        wifi[foundIdx] = tmp;
        snprintf(logbuf, LOGBUF_SIZE, "Prioritizing lastSSID: %s", lastSSID.c_str());
        log::toAll(logbuf);
      }
    }

    WiFi.mode(WIFI_STA);
    wifiStartTime = now;
    connectTimer = xTimerCreate("connectTimer", 5000/portTICK_PERIOD_MS, pdTRUE, NULL, tryConnect);
    if (connectTimer == NULL) {
      log::toAll("Failed to create wifi connect timer");
      return false;
    }
    xTimerStart(connectTimer,0);
    log::toAll("WiFi connection initiated (async)...");

    xTaskCreate(wifiConnectTask, "wifiConnect", 4096, NULL, 1, NULL);
    log::toAll("WiFi connect task started");

    return false; // Not connected yet, will be set by event handler
  }
}

void resetWifi() {
  log::toAll("Resetting WiFi settings...");
  WiFi.disconnect(true);
  if (LittleFS.exists(wifiPath))
    if (LittleFS.remove(wifiPath))
      log::toAll("removed wifi params");
  delay(3000);
  ESP.restart();
}

// Save new credentials as first entry in wifi.json, preserving existing entries
void saveNewWifiCredentials(const char* ssid, const char* password) {
  JsonDocument doc;
  JsonArray arr;

  if (LittleFS.exists(wifiPath)) {
    File readFile = LittleFS.open(wifiPath, FILE_READ);
    if (readFile) {
      DeserializationError error = deserializeJson(doc, readFile);
      readFile.close();
      if (error) {
        doc.clear();
      }
    }
  }
  arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc.to<JsonArray>();

  JsonDocument newEntry;
  newEntry["ssid"] = ssid;
  newEntry["password"] = password;

  JsonDocument newDoc;
  JsonArray newArr = newDoc.to<JsonArray>();
  newArr.add(newEntry);
  for (JsonVariant v : arr) {
    if (v["ssid"].as<String>() != String(ssid)) {
      newArr.add(v);
    }
  }

  File file = LittleFS.open(wifiPath, FILE_WRITE);
  if (!file) {
    log::toAll("Failed to open wifi.json for writing");
    return;
  }
  serializeJsonPretty(newDoc, file);
  file.close();
  snprintf(logbuf, LOGBUF_SIZE, "Saved new WiFi: %s (first in wifi.json)", ssid);
  log::toAll(logbuf);

  // Reload credentials and reconnect
  readWiFiCredentials();
  gotWifiCreds = true;
  wifiFailCount = 0;
  WiFi.disconnect(true);
  delay(100);
  connectIdx = 0;
  connectCount = 0;

  if (connectTimer != NULL) {
    xTimerDelete(connectTimer, 0);
    connectTimer = NULL;
  }
  connectTimer = xTimerCreate("connectTimer", 5000/portTICK_PERIOD_MS, pdTRUE, NULL, tryConnect);
  if (connectTimer != NULL) {
    xTimerStart(connectTimer, 0);
  }

  wifiConnectPending = true;
  snprintf(logbuf, LOGBUF_SIZE, "Reconnecting to %s...", ssid);
  log::toAll(logbuf);
}

// Delete a WiFi credential by SSID from wifi.json and reload
void deleteWifiCredentials(const char* ssid) {
  if (!LittleFS.exists(wifiPath)) {
    log::toAll("wifi.json not found");
    return;
  }

  JsonDocument doc;
  File readFile = LittleFS.open(wifiPath, FILE_READ);
  if (!readFile) {
    log::toAll("Failed to open wifi.json for reading");
    return;
  }
  DeserializationError error = deserializeJson(doc, readFile);
  readFile.close();

  if (error) {
    snprintf(logbuf, LOGBUF_SIZE, "Failed to parse wifi.json: %s", error.c_str());
    log::toAll(logbuf);
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  bool found = false;

  JsonDocument newDoc;
  JsonArray newArr = newDoc.to<JsonArray>();
  for (JsonObject obj : arr) {
    const char* entrySsid = obj["ssid"] | "";
    if (strcmp(entrySsid, ssid) == 0) {
      found = true;
      continue;
    }
    newArr.add(obj);
  }

  if (!found) {
    snprintf(logbuf, LOGBUF_SIZE, "SSID '%s' not found in wifi.json", ssid);
    log::toAll(logbuf);
    return;
  }

  File writeFile = LittleFS.open(wifiPath, FILE_WRITE);
  if (!writeFile) {
    log::toAll("Failed to open wifi.json for writing");
    return;
  }
  serializeJsonPretty(newDoc, writeFile);
  writeFile.close();

  snprintf(logbuf, LOGBUF_SIZE, "Deleted WiFi: %s", ssid);
  log::toAll(logbuf);

  readWiFiCredentials();
  gotWifiCreds = (wifiCount > 0);
}
#endif // ifdef WIFI
