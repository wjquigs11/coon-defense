/*
*/

#include "include-general.h"

// variables for ultrasonic distance sensor
float lastValue=0;
const int trigPin = 17; // green
const int echoPin = 19; // orange
//define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701
#define TANKTOP 5 // inches from sensor to water level when tank is full (top of tank)
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
  //log::toAll("single reading duration = " + String(duration));
  // Calculate the distance
  float distance = (duration * SOUND_SPEED/2) * CM_TO_INCH;
  //log::toAll("single reading distance = " + String(distance));
  return distance;
}

// Calculates the average of collected samples
float getWaterLevel() {
  // Calculate average of collected samples
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
  /* Log the average calculation periodically
  static unsigned long lastLogTime = 0;
  unsigned long now = millis();
  if (now - lastLogTime > 30000 || lastLogTime == 0) { // Log every 30 seconds
    lastLogTime = now;
    log::toAll("getWaterLevel: Avg of " + String(validSamples) + " samples = " + String(distanceInch) + " inches");
  }
  */
  // Convert to tank level
  // Arduino's map function only works with long integers, so we need to create our own float mapping
  float tankLevel = (distanceInch - tankbottom) * (capacity) / (tanktop - tankbottom);
  if (tankLevel < 0) tankLevel = -1;
  if (tankLevel > capacity) tankLevel = capacity;
  
  return tankLevel;
}

Preferences waterPrefs;

void setupWater() {
  waterPrefs.begin("raintank", false);

  tanktop = waterPrefs.getFloat("tanktop", TANKTOP);
  tankbottom = waterPrefs.getFloat("tankbottom", TANKBOTTOM);
  capacity = waterPrefs.getFloat("capacity", CAPACITY);
  sprintf(prbuf,"tank top: %.2f tank bottom: %.2f capacity: %.2f", tanktop, tankbottom, capacity);
  log::toAll(prbuf);
  
  //Wire.begin();
  log::toAll("ultrasonic water level");
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  
  // Initialize sample buffer
  for (int i = 0; i < MAX_SAMPLES; i++) {
    sampleBuffer[i] = 0;
  }
  
  log::toAll("water level: " + String(getWaterLevel()));
  consLog.flush();
}

void loopWater() {
  // Take a new sample every 60 seconds
  if (now - lastSampleTime >= 60000) {
    lastSampleTime = now;
    // Add new sample to the buffer
    float reading = takeSingleReading();
    sampleBuffer[sampleCount % MAX_SAMPLES] = reading;
    sampleCount++;
    if (sampleCount >= 1000) {
      sampleCount = MAX_SAMPLES;
    }
    readings["readings"] = getSensorReadings();
  }
}
