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

// Determine reset reason
#include <user_interface.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#define TIME_ZONE_OFFSET -25200
#define SLEEP_MINUTE_INTERVAL 30
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
int minute;
int nextMinute;
int second;
int nextSecond;
int sleepTime;

// HC4051 multiplexer pin definitions
#define HC4051_A D5
#define HC4051_B D6
#define HC4051_C D7

// for DHT11 temp/humidity
#include <DHT.h>
#define DHT_TYPE DHT11

// Pin definitions
#define DHT_PIN D2
#define LED_PIN BUILTIN_LED

// use temperature unit F if true, else C
bool temperatureF = true;

// These settings need to be set depending on the sensor type and sensitivity
const char * soilSensorType = "Capacitive Soil 2.0";
int dryThresh = 650;
int wetThresh = 500;

DHT dht(DHT_PIN, DHT_TYPE);

String ip;
int moisture;
int light;
int batteryRaw;
float batteryPercent;
float batteryVoltage;
String waterLevel;
float temperature;
float humidity;

void readMoisture() {
  digitalWrite(HC4051_A, LOW);
  digitalWrite(HC4051_B, LOW);
  digitalWrite(HC4051_C, LOW);
  delay(10);
  moisture = analogRead(A0);
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

void readLight() {
  digitalWrite(HC4051_A, LOW);
  digitalWrite(HC4051_B, HIGH);
  digitalWrite(HC4051_C, HIGH);
  delay(10);
  light = analogRead(A0);
  Serial.print("Light: ");
  Serial.println(light);
}

void readBattery() {
  digitalWrite(HC4051_A, HIGH);
  digitalWrite(HC4051_B, LOW);
  digitalWrite(HC4051_C, LOW);
  delay(10);
  batteryRaw = analogRead(A0);
  Serial.print("Battery Raw: ");
  Serial.println(batteryRaw);
  
  int voltage = map(batteryRaw, 0, 1024, 0, 420);
  batteryVoltage = voltage / 100.0;
  Serial.print("Battery Voltage: ");
  Serial.println(batteryVoltage);

  int min_voltage = 330;
  batteryPercent = voltage < min_voltage ? 0 : map(voltage, min_voltage, 420, 0, 1000) / 10.0;
  Serial.print("Battery Percent: ");
  Serial.println(batteryPercent);
}

void readDHT() {
  temperature = dht.readTemperature(temperatureF);
  humidity = dht.readHumidity();
}

void readNTP() {
  while (!timeClient.update()) {
    blinkLed(250, LED_PIN, false);
  }
  minute = timeClient.getMinutes();
  second = timeClient.getSeconds();
  nextMinute = minute + SLEEP_MINUTE_INTERVAL - (minute % SLEEP_MINUTE_INTERVAL);
  nextSecond = 60;
  Serial.print("Current NTP time: ");
  Serial.println(timeClient.getFormattedTime());
}

bool wokeUpEarly() {
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
  addKeyVal(yml, "soil-sensor-type", soilSensorType);
  addKeyVal(yml, "light", light);
  addKeyVal(yml, "temperature", temperature);
  addKeyVal(yml, "temperature-unit", temperatureF ? "F" : "C");
  addKeyVal(yml, "humidity", humidity);
  addKeyVal(yml, "water-level", waterLevel);
  addKeyVal(yml, "date", timeClient.getFormattedDate());
  addKeyVal(yml, "time", timeClient.getFormattedTime());
  addKeyVal(yml, "battery-raw", batteryRaw);
  addKeyVal(yml, "battery-voltage", batteryVoltage);
  addKeyVal(yml, "battery-percent", batteryPercent);
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

  auto resetInfo = ESP.getResetInfoPtr();
  Serial.print("RESET REASON: ");
  Serial.println(ESP.getResetReason());

  timeClient.begin();
  timeClient.setTimeOffset(TIME_ZONE_OFFSET);
 
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);             // Connect to the network

  pinMode(HC4051_A, OUTPUT);
  pinMode(HC4051_B, OUTPUT);
  pinMode(HC4051_C, OUTPUT);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  ip = waitForConnection([]() { blinkLed(500, LED_PIN, false); });
  readNTP();
  setSleepTime();
  // For some reason my esp's internal clock during deep sleep seems to be slightly slow
  // See https://forum.arduino.cc/index.php?topic=595499 post #7
  if (resetInfo->reason == REASON_DEEP_SLEEP_AWAKE && wokeUpEarly()) {
    Serial.println("Woke up early!");
    ESP.deepSleep(sleepTime);
  }

  readMoisture();
  readLight();
  readDHT();
  readBattery();
  
  sendYaml();

  ESP.deepSleep(sleepTime);
}

void loop() {
}
