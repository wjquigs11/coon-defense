#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

#ifdef DEEPSLEEP
// ESP32 deep sleep includes
#include "esp_system.h"
#include "esp_sleep.h"
#endif

#ifdef WIFI
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
extern AsyncWebServer server;
extern AsyncEventSource events;
extern bool serverStarted;
extern bool wifiConnected;
extern String host;
extern unsigned long wifiStartTime;
extern TimerHandle_t connectTimer;
extern JsonDocument readings;
extern int timerDelay;
#define HTTP_PORT 80
#define DRD_TIMEOUT 10
void checkLittleFS();
bool readWiFiCredentials();
extern bool gotWifiCreds;
// for captive portal
#include <DNSServer.h>
extern DNSServer dnsServer;
#define DNS_INTERVAL 300
bool setupWifi();
void resetWifi();
void saveNewWifiCredentials(const char* ssid, const char* password);
void deleteWifiCredentials(const char* ssid);
void wifiCheck();
extern String newWifiSsid;
extern String newWifiPass;
// WiFi credentials
struct WiFiCredentials {
  String ssid;
  String password;
  String ip;
  String gateway;
};
#define MAX_WIFI_NETWORKS 5
extern WiFiCredentials wifi[];
extern int wifiCount;
void startWebServer();
void startPortal();
void onWiFiConnected();
void onWiFiDisconnected();
String getSensorReadings();
String coon_processor(const String& var);
String rain_processor(const String& var);

// Schedule management
#define MAX_SCHEDULES 8
extern JsonDocument scheduleData;
void checkScheduleEvents();
void triggerScheduleEvent(int scheduleIndex, JsonObject schedule);
bool loadSchedules();
#endif

#ifdef WEBSERIAL
#include <WebSerialPro.h>
void WebSerialonMessage(uint8_t *data, size_t len);
void pollSerialConsole();
extern String appCommandList[];
extern String appToggleList[];
using Handler = void(*)(String*, int);
extern Handler appHandler;
extern Handler togHandler;
#endif
#ifdef ELEGANTOTA
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <ElegantOTA.h>
#endif

#include "logto.h"

extern Preferences coonPrefs;
extern Preferences waterPrefs;
extern File consLog;

// Timer variables
#include <time.h>
#define DEFDELAY 1000
extern unsigned long lastTime;
// timerDelay defines how long we wait to send an update to connected web clients
extern int timerDelay;
// loopDelay defines how long we delay at the end of each iteration of loop();
// if we're not doing captive portal (for dns) it can be 0, although spinning on CPU for most of my projects seems a bit silly
extern int loopDelay;
extern int minReadRate;
// store last update based on clock time from client browser
extern time_t lastUpdate, updateTime;
extern struct tm *ptm;
#define PRBUF 128
extern char prbuf[];

#ifdef NTP
// Time synchronization functions
void setupTime();
bool waitForTimeSync(int timeoutSeconds = 10);
String getFormattedTime();
unsigned long getEpochTime();
bool isNtpSyncSuccessful();
void resyncNTP();
#endif

#ifdef DEEPSLEEP
// Deep sleep variables
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60       /* Time ESP32 will go to sleep (in seconds) */
extern int awakeTimer;          /* Time to stay awake before going to sleep (in seconds) */
extern unsigned long startTime; /* Time when the device started */

// Function to print the wakeup reason
void print_wakeup_reason();
#endif

extern unsigned long now;

#define NUM_RELAYS 1
#define relayGPIO 16
#define RELAY_NO    false
extern bool relayState;

#include <Adafruit_INA219.h>
#include <movingAvg.h>
extern Adafruit_INA219 ina219;
extern movingAvg shuntAvg;
extern bool ina219Found;
void logIna219Diagnostics(const char* context);

// Custom panic handler setup
void setup_custom_panic_handler();

void setupWater();
void loopWater();
void updateWaterReading();
int getCachedWaterLevel();
extern int minReadRate;

float getWaterLevel();
extern float tanktop, tankbottom, capacity;
extern float distanceInch;


