#ifdef WIFI
#include "include-general.h"

// Schedule checking variables
static bool scheduleStates[MAX_SCHEDULES]; // Track last triggered state for each schedule
static bool scheduleStatesInitialized = false;
static unsigned long scheduleStartTime = 0; // Track when schedule checking started

// Helper function to execute the scheduled action
void triggerScheduleEvent(int scheduleIndex, JsonObject schedule) {
    JsonObject switches = schedule["switches"];
    
    for (JsonPair kv : switches) {
        int relayNum = String(kv.key().c_str()).toInt();
        bool relayState = kv.value().as<bool>();
        
        // Apply relay state (adapt this to your relay control logic)
        if (relayNum == 1) { // Assuming you have relay #1
            if (RELAY_NO) {
                digitalWrite(relayGPIO, !relayState);
            } else {
                digitalWrite(relayGPIO, relayState);
            }
            log::toAll("Relay " + String(relayNum) + " set to " + 
                      (relayState ? "ON" : "OFF"));
        }
    }
}

// Main schedule checking function
void checkScheduleEvents() {
    unsigned long now = millis();
    
    // Initialize start time on first call
    if (scheduleStartTime == 0) {
        scheduleStartTime = now;
    }
    
    // Get current time (use your existing NTP time logic)
    time_t currentTime;
    
#ifdef NTP
    if (isNtpSyncSuccessful()) {
        currentTime = getEpochTime();
    } else 
#endif
    if (updateTime > 0) {
        // Use elapsed time since schedule checking started
        currentTime = updateTime + (now - scheduleStartTime) / 1000;
    } else {
        return; // No valid time available
    }
    
    struct tm* timeinfo = localtime(&currentTime);
    if (timeinfo == nullptr) {
        return; // Invalid time
    }
    
    int currentHour = timeinfo->tm_hour;
    int currentMinute = timeinfo->tm_min;
    int currentWeekday = timeinfo->tm_wday; // 0=Sunday, 1=Monday, etc.
    
    // Initialize schedule states array on first run
    if (!scheduleStatesInitialized) {
        size_t maxSchedules = (scheduleData.size() < MAX_SCHEDULES) ? scheduleData.size() : MAX_SCHEDULES;
        for (size_t i = 0; i < maxSchedules; i++) {
            scheduleStates[i] = false;
        }
        scheduleStatesInitialized = true;
    }
    
    // Check each schedule
    size_t maxSchedules = (scheduleData.size() < MAX_SCHEDULES) ? scheduleData.size() : MAX_SCHEDULES;
    for (size_t i = 0; i < maxSchedules; i++) {
        JsonObject schedule = scheduleData[i];
        
        if (!schedule["enabled"].as<bool>()) {
            continue; // Skip disabled schedules
        }
        
        // Parse schedule time
        String timeStr = schedule["time"];
        String ampmStr = schedule["ampm"];
        
        int scheduleHour = timeStr.substring(0, timeStr.indexOf(':')).toInt();
        int scheduleMinute = timeStr.substring(timeStr.indexOf(':') + 1).toInt();
        
        // Convert to 24-hour format
        if (ampmStr == "PM" && scheduleHour != 12) {
            scheduleHour += 12;
        } else if (ampmStr == "AM" && scheduleHour == 12) {
            scheduleHour = 0;
        }
        
        // Check if today is a scheduled day
        JsonArray days = schedule["days"];
        bool isScheduledDay = false;
        for (JsonVariant day : days) {
            if (day.as<int>() == currentWeekday) {
                isScheduledDay = true;
                break;
            }
        }
        
        if (!isScheduledDay) {
            scheduleStates[i] = false; // Reset state for non-scheduled days
            continue;
        }
        
        // Calculate time window (±2 minutes for tolerance)
        const int TOLERANCE_MINUTES = 2;
        int scheduleTimeMinutes = scheduleHour * 60 + scheduleMinute;
        int currentTimeMinutes = currentHour * 60 + currentMinute;
        
        bool inTimeWindow = abs(currentTimeMinutes - scheduleTimeMinutes) <= TOLERANCE_MINUTES;
        
        // Trigger event only once when entering the time window
        if (inTimeWindow && !scheduleStates[i]) {
            scheduleStates[i] = true;
            triggerScheduleEvent(i, schedule);
            log::toAll("Schedule " + String(i) + " triggered at " + 
                      String(currentHour) + ":" + String(currentMinute));
        } else if (!inTimeWindow && scheduleStates[i]) {
            // Reset state when leaving the time window
            scheduleStates[i] = false;
        }
    }
}

#endif // WIFI