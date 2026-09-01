/*
  Ultrasonic water level sensor (non-blocking).
  Samples are collected one-at-a-time in loopWater() (every 60s) to avoid
  blocking the main loop, which also services relays, schedules, and INA219
  spike detection. getWaterLevel() averages the collected sample buffer.
*/

#include "include-general.h"

// variables for ultrasonic distance sensor
float lastValue = 0;
const int trigPin = 17; // green
const int echoPin = 19; // orange
// define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701
#define TANKTOP 5     // inches from sensor to water level when tank is full (top of tank)
#define TANKBOTTOM 46 // inches
#define CAPACITY 776
// tanks are empty if distance >= TANKBOTTOM", assuming sensor is at exact level of top of tank
// total capacity = 333+333+55+55 = 776 gallons (not linear since 55 gallon drums don't go to same level)
float tanktop, tankbottom, capacity;
float distanceInch;

// Variables for non-blocking sensor reading
const int MAX_SAMPLES = 10;
int sampleCount = 0;
float sampleBuffer[MAX_SAMPLES];
unsigned long lastSampleTime = 0;

// Cached level so getSensorReadings() never has to trigger a blocking read
static int cachedWaterLevel = 0;

// Takes a single ultrasonic sensor reading without delays
float takeSingleReading() {
  long duration;

  // Clears the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Set the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Read echoPin, return the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // Calculate the distance
  float distance = (duration * SOUND_SPEED / 2) * CM_TO_INCH;
  return distance;
}

// Calculates the average of collected samples and converts to gallons
float getWaterLevel() {
  float totalDistance = 0;
  int validSamples = 0;
  for (int i = 0; i < MAX_SAMPLES; i++) {
    if (sampleCount >= MAX_SAMPLES || i < sampleCount) {
      if (sampleBuffer[i] > 0) {
        totalDistance += sampleBuffer[i];
        validSamples++;
      }
    }
  }
  if (validSamples == 0) {
    log::toAll("getWaterLevel: No valid samples available yet");
    return -1; // No valid samples
  }
  // Calculate average distance
  distanceInch = totalDistance / validSamples;

  // Convert to tank level (float mapping; Arduino map() is integer-only)
  float tankLevel = (distanceInch - tankbottom) * (capacity) / (tanktop - tankbottom);
  if (tankLevel < 0) tankLevel = -1;
  if (tankLevel > capacity) tankLevel = capacity;

  return tankLevel;
}

// Refresh the cached level. Called from the periodic timer in coon-main loop.
void updateWaterReading() {
  cachedWaterLevel = (int)getWaterLevel();
}

int getCachedWaterLevel() {
  return cachedWaterLevel;
}

Preferences waterPrefs;

void setupWater() {
  waterPrefs.begin("raintank", false);

  tanktop = waterPrefs.getFloat("tanktop", TANKTOP);
  tankbottom = waterPrefs.getFloat("tankbottom", TANKBOTTOM);
  capacity = waterPrefs.getFloat("capacity", CAPACITY);
  snprintf(logbuf, LOGBUF_SIZE, "tank top: %.2f tank bottom: %.2f capacity: %.2f", tanktop, tankbottom, capacity);
  log::toAll(logbuf);

  log::toAll("ultrasonic water level");
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input

  // Initialize sample buffer
  for (int i = 0; i < MAX_SAMPLES; i++) {
    sampleBuffer[i] = 0;
  }

  snprintf(logbuf, LOGBUF_SIZE, "water level: %d", (int)getWaterLevel());
  log::toAll(logbuf);
  log::flush();
}

void loopWater() {
  // Take a new sample every 60 seconds (non-blocking)
  if (now - lastSampleTime >= 60000) {
    lastSampleTime = now;
    float reading = takeSingleReading();
    sampleBuffer[sampleCount % MAX_SAMPLES] = reading;
    sampleCount++;
    if (sampleCount >= 1000) {
      sampleCount = MAX_SAMPLES;
    }
    updateWaterReading();
  }
}
