//
// soil_sensor_http.ino
// Naookie Sato
//
// Created by Naookie Sato on 04/14/2020
// Copyright © 2020 Naookie Sato. All rights reserved.
//

// ESP Includes
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <stdlib.h>
#include "config.h"
#include "utils.h"
// This might need to be included when using some esp8266 arduino cores.
//#include "pins_arduino.h"

// Persistent storage between deep sleep cycles
#include <RTCVars.h>
RTCVars state;
int cold_boot;

#include <NTPClient.h>
#include <WiFiUdp.h>
#define TIME_ZONE_OFFSET -25200
#define SLEEP_MINUTE_INTERVAL 15
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
int minute;
int nextMinute;
int second;
int nextSecond;
int sleepTime;

// for DHT11 temp/humidity
#include <DHT.h>
#define DHT_TYPE DHT11

// Pin definitions
#define DHT_PIN D5
#define LED_PIN BUILTIN_LED
#define MOISTURE_PIN A0

// use temperature unit F if true, else C
bool temperatureF = true;

// Definte soil moisture sensor type
//#define RESISTIVE_SENSOR
#define CAPACITIVE_SENSOR

// Define different calibration settings depending on soil sensor type
#ifdef RESISTIVE_SENSOR
int dryThresh = 700;
int wetThresh = 400;
#elif defined(CAPACITIVE_SENSOR)
int dryThresh = 700;
int wetThresh = 550;
#endif

DHT dht(DHT_PIN, DHT_TYPE);

String ip;
int moisture;
String waterLevel;
float temperature;
float humidity;

void readMoisture() {
  moisture = analogRead(MOISTURE_PIN);
  Serial.print("Moisture: ");
  Serial.println(moisture);
  if (moisture > dryThresh) {
    waterLevel = "DRY";
  } else if (moisture < wetThresh) {
    waterLevel = "WET";
  } else {
    waterLevel = "OK";
  }
}

void readDHT() {
  temperature = dht.readTemperature(temperatureF);
  humidity = dht.readHumidity();
}

void readNTP() {
  while (!timeClient.update()) {
    blinkLed(250, LED_PIN);
  }
  minute = timeClient.getMinutes();
  second = timeClient.getSeconds();
  nextMinute = minute + SLEEP_MINUTE_INTERVAL - (minute % SLEEP_MINUTE_INTERVAL);
  nextSecond = 60;
  Serial.print("Current NTP time: ");
  Serial.println(timeClient.getFormattedTime());
}

bool wokeUpEarly() {
  if (cold_boot) {
    return false;
  }
  int minutesLeft = nextMinute - minute;
  return !(minutesLeft == SLEEP_MINUTE_INTERVAL || SLEEP_MINUTE_INTERVAL == 1);
}

void setSleepTime() {
  int sleepMinutes = nextMinute - minute - 1;
  int sleepSeconds = nextSecond - second;

  Serial.print("sleep for minutes: ");
  Serial.println(sleepMinutes);
  Serial.print("sleep for seconds: ");
  Serial.println(sleepSeconds);
  sleepTime = (sleepMinutes * 60 + sleepSeconds) * 1000000;
}

String createSensorYaml() {
  String yml = "---\n";
  addKeyVal(yml, "location", location);
  addKeyVal(yml, "ip-address", ip);
  addKeyVal(yml, "moisture", moisture);
  addKeyVal(yml, "temperature", temperature);
  addKeyVal(yml, "temperature-unit", temperatureF ? "F" : "C");
  addKeyVal(yml, "humidity", humidity);
  addKeyVal(yml, "water-level", waterLevel);
  addKeyVal(yml, "date", timeClient.getFormattedDate());
  addKeyVal(yml, "time", timeClient.getFormattedTime());
  return yml;
}

void sendYaml() {
  auto yml = createSensorYaml();
  Serial.println("sending yml:");
  Serial.println(yml);
  HTTPClient http;
  http.begin(request);
  http.addHeader("Content-Type", "text/plain");
  int retCode = http.POST(yml);
  Serial.print("http request complete, return code: ");
  Serial.println(retCode);
}

void setup() {
  setupSerial(115200, 2000);

  state.registerVar(&cold_boot);
  cold_boot = !state.loadFromRTC();
  state.saveToRTC();

  timeClient.begin();
  timeClient.setTimeOffset(TIME_ZONE_OFFSET);
 
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);             // Connect to the network

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  ip = waitForConnection([]() { blinkLed(500, LED_PIN, true); });
  readNTP();
  setSleepTime();
  // For some reason my esp's internal clock during deep sleep seems to be slightly slow
  // See https://forum.arduino.cc/index.php?topic=595499 post #7
  if (wokeUpEarly()) {
    Serial.println("Woke up early!");
    ESP.deepSleep(sleepTime);
  }

  readMoisture();
  readDHT();
  
  sendYaml();

  ESP.deepSleep(sleepTime);
}

void loop() {
}
