#if defined(WIFI)
#include "include-general.h"

// Include AsyncWebServer headers directly for compilation
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(HTTP_PORT);
AsyncEventSource events("/events");

const char* PARAM_INPUT_1 = "relay";  
const char* PARAM_INPUT_2 = "state";

void startWebServer() {
  log::toAll("starting web server");

  server.on("/rain", HTTP_GET, [](AsyncWebServerRequest * request) {
      log::toAll("raintank.html");
      request->send(LittleFS, "/raintank.html", "text/html", false, [](const String& var) -> String { return rain_processor(var); });
  });

  server.on("/coon", HTTP_GET, [](AsyncWebServerRequest * request) {
      log::toAll("coon.html");
      request->send(LittleFS, "/coon.html", "text/html", false, [](const String& var) -> String { return coon_processor(var); });
  });

  server.on("/schedule", HTTP_GET, [](AsyncWebServerRequest * request) {
      log::toAll("schedule.html");
      request->send(LittleFS, "/schedule.html", "text/html");
  });

  // Get all schedules
  server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest *request) {
    File file = LittleFS.open("/schedule.json", "r");
    if (!file) {
      request->send(500, "application/json", "{\"error\":\"Failed to open schedule file\"}");
      return;
    }
    
    String scheduleJson = file.readString();
    file.close();
    
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", scheduleJson);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  // Save schedules
  server.on("/api/schedules", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index + len == total) {
        // Parse the JSON data
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (error) {
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }
        
        // Save to LittleFS
        File file = LittleFS.open("/schedule.json", "w");
        if (!file) {
          request->send(500, "application/json", "{\"error\":\"Failed to save schedule\"}");
          return;
        }
        
        serializeJson(doc, file);
        file.close();
        
        // Update the global schedule data
        scheduleData.clear();
        scheduleData = doc;
        
        log::toAll("Schedules updated");
        
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"success\":true}");
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
      }
    }
  );

  // Handle OPTIONS preflight for schedule API
  server.on("/api/schedules", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
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

// Send a GET request to <ESP_IP>/relayupdate?relay=<inputMessage>&state=<inputMessage2>
  server.on("/relayupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    String inputMessage2;
    String inputParam2;
    // GET input1 value on <ESP_IP>/relayupdate?relay=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1) & request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      inputParam2 = PARAM_INPUT_2;
      if(RELAY_NO){
        Serial.print("NO ");
        digitalWrite(relayGPIO, !inputMessage2.toInt());
      }
      else{
        Serial.print("NC ");
        digitalWrite(relayGPIO, inputMessage2.toInt());
      }
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage + inputMessage2);
    request->send(200, "text/plain", "OK");
  });

}
#endif