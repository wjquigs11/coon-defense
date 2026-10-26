#if defined(WIFI)
#include "include-general.h"

// Include AsyncWebServer headers directly for compilation
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(HTTP_PORT);
AsyncEventSource events("/events");

void startWebServer() {
  log::toAll("starting web server");

  server.on("/rain", HTTP_GET, [](AsyncWebServerRequest * request) {
      log::toAll("raintank.html");
      request->send(LittleFS, "/raintank.html", "text/html", false, [](const String& var) -> String { return rain_processor(var); });
  });

  // Update tank calibration (posted from raintank.html): tanktop, tankbottom, capacity
  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        if (p->name() == "tanktop") {
          tanktop = atof(p->value().c_str());
          waterPrefs.putFloat("tanktop", tanktop);
        } else if (p->name() == "tankbottom") {
          tankbottom = atof(p->value().c_str());
          waterPrefs.putFloat("tankbottom", tankbottom);
        } else if (p->name() == "capacity") {
          capacity = atof(p->value().c_str());
          waterPrefs.putFloat("capacity", capacity);
        }
      }
    }
    snprintf(logbuf, LOGBUF_SIZE, "config: top=%.2f bottom=%.2f cap=%.2f", tanktop, tankbottom, capacity);
    log::toAll(logbuf);
    // Return the refreshed tank page with the new values filled in
    request->send(LittleFS, "/raintank.html", "text/html", false, [](const String& var) -> String { return rain_processor(var); });
  });

  // Root always goes to the rain tank page.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/rain");
  });

  // Serve static files with proper MIME types
  server.serveStatic("/", LittleFS, "/");

  // Request latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", getSensorReadings());
    // Add CORS headers
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  server.on("/host", HTTP_GET, [](AsyncWebServerRequest *request) {
    String buf = "hostname: " + host;
    buf += ", ESP local MAC addr: " + String(WiFi.macAddress());
    log::toAll(buf);
    request->send(200, "text/plain", buf.c_str());
    buf = String();
  });

  // WiFi management routes (also registered in captive portal)
  server.on("/wifimanager", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });

  server.on("/wifimanager", HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssid, password;
    int params = request->params();
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        if (p->name() == "ssid") ssid = p->value();
        else if (p->name() == "pass") password = p->value();
        else if (p->name() == "hostname" && p->value().length() > 0) {
          host = p->value();
          prefs.putString("hostname", host);
        }
      }
    }
    if (ssid.length() > 0 && password.length() > 0) {
      saveNewWifiCredentials(ssid.c_str(), password.c_str());
      request->send(200, "text/plain", "WiFi credentials saved. Restart to connect to new network.");
    } else {
      request->send(400, "text/plain", "Must supply ssid and password");
    }
  });

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

#ifdef TEMPLATE
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("config:");
    String response = "none";
    if (request->hasParam("hostname")) {
      Serial.printf("hostname %s", request->getParam("hostname")->value().c_str());
      host = request->getParam("hostname")->value();
      response = "change hostname to " + host;
      log::toAll(response);
      preferences.putString("hostname",host);
      log::toAll("preferences " + preferences.getString("hostname", "unknown"));
    } else if (request->hasParam("webtimer")) {
      timerDelay = atoi(request->getParam("webtimer")->value().c_str());
      if (timerDelay < 0) timerDelay = DEFDELAY;
      if (timerDelay > 10000) timerDelay = 10000;
      response = "change web timer to " + String(timerDelay);
      log::toAll(response);
      preferences.putInt("timerdelay",timerDelay);
    } else if (request->hasParam("value")) {
      // do something with "value" parameter
      response = "value successful";
      log::toAll(response);
    }
    request->send(200, "text/plain", response.c_str());
    response = String();
  });
#endif

  // Always register the /clienttime endpoint to avoid 404 errors
  server.on("/clienttime", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
#ifdef NTP
      // Check if NTP sync was successful
      if (!isNtpSyncSuccessful()) {
        // Only use client time if NTP sync failed
        log::toAll("Using client time as NTP sync failed");
        JsonDocument doc;
        ::deserializeJson(doc, data);
        String clientTime = doc["datetime"];
        Serial.print("Received client time: ");
        Serial.println(clientTime);
        
        int year, month, day, hour, minute, second;
        sscanf(clientTime.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);
        struct tm timeinfo;
        timeinfo.tm_year = year - 1900;   // tm_year: years since 1900
        timeinfo.tm_mon  = month - 1;     // tm_mon: months since January [0,11]
        timeinfo.tm_mday = day;           // tm_mday: day of the month [1,31]
        timeinfo.tm_hour = hour;
        timeinfo.tm_min  = minute;
        timeinfo.tm_sec  = second;
        time_t t = mktime(&timeinfo);  // Convert to time_t (seconds since epoch)
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, NULL);      // Set the ESP32 system clock
        request->send(200, "text/plain", "Time received and set");
      } else {
        // NTP sync was successful, just acknowledge
        log::toAll("NTP sync successful, ignoring client time");
        request->send(200, "text/plain", "Time received but not used (NTP active)");
      }
#else
      // No NTP support - always use client time
      log::toAll("Using client time (no NTP support)");
      JsonDocument doc;
      ::deserializeJson(doc, data);
      String clientTime = doc["datetime"];
      Serial.print("Received client time: ");
      Serial.println(clientTime);
      
      int year, month, day, hour, minute, second;
      sscanf(clientTime.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);
      struct tm timeinfo;
      timeinfo.tm_year = year - 1900;
      timeinfo.tm_mon  = month - 1;
      timeinfo.tm_mday = day;
      timeinfo.tm_hour = hour;
      timeinfo.tm_min  = minute;
      timeinfo.tm_sec  = second;
      time_t t = mktime(&timeinfo);
      struct timeval now = { .tv_sec = t, .tv_usec = 0 };
      settimeofday(&now, NULL);
      request->send(200, "text/plain", "Time received and set");
#endif
    }
  );

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 1000);
  });
  server.addHandler(&events);
  //server.addHandler(&ws);

  // Handle OPTIONS preflight requests for CORS
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      AsyncWebServerResponse *response = request->beginResponse(204);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      response->addHeader("Access-Control-Max-Age", "3600");
      request->send(response);
      return;
    }
    // Handle not found for other requests
    request->send(404);
  });
}
#endif // WIFI