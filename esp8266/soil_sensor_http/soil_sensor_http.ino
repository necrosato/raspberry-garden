//
// soil_sensor.ino
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
// This might need to be included when using some esp8266 arduino cores.
//#include "pins_arduino.h"

// Persistent storage between deep sleep cycles
#include <RTCVars.h>
RTCVars state;
int cold_boot;

#include <NTPClient.h>
#include <WiFiUdp.h>
#define TIME_ZONE_OFFSET -25200
#define SLEEP_MINUTE_INTERVAL 2
#define SLEEP_SECOND_INTERVAL 60
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
int minute;
int second;
int sleepTime;

// for DHT11 temp/humidity
#include <DHT.h>
#define DHT_TYPE DHT11

// Pin definitions
#define DHT_PIN D4
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

// NOTE: TO ACTUALLY CONNECT 1306 128x64 LCD Display I2C - 
/*
    arduino I2C standards define the following:
        SDA = GPIO4
        SCL = GPIO5

    so to connect an I2C Display to NodeMCU: 
        SDA = GPIO4 = D2 (NodeMCU Pin - see ./pins_arduino.h)
        SCL = GPIO5 = D1 (NodeMCU Pin - see ./pins_arduino.h)
    
*/

DHT dht(DHT_PIN, DHT_TYPE);

int wifi_status;

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

bool checkConnection() {
  int wifi_status_last = wifi_status;
  wifi_status = WiFi.status();
  if (wifi_status != wifi_status_last) {
    if(wifi_status == WL_CONNECTED){
      Serial.println("");
      Serial.println("Your ESP is connected!");
      Serial.println("Your IP address is: ");
      Serial.println(WiFi.localIP());
      ip = WiFi.localIP().toString();
    } else {
      Serial.println("");
      Serial.println("WiFi not connected");
      ip = "Connecting...";
    }
  }
  return wifi_status == WL_CONNECTED;
}

void readDHT() {
  temperature = dht.readTemperature(temperatureF);
  humidity = dht.readHumidity();
}

void readNTP() {
  while (!timeClient.update()) {
    delay(500);
  }
  minute = timeClient.getMinutes();
  second = timeClient.getSeconds();
  int minutesLeft = SLEEP_MINUTE_INTERVAL - (minute % SLEEP_MINUTE_INTERVAL) - 1;
  // woke up before minute mark, skip to next interval
  if (!cold_boot && minutesLeft == 0 && SLEEP_MINUTE_INTERVAL != 1) {
    minutesLeft += SLEEP_MINUTE_INTERVAL;
  }
  int secondsLeft = SLEEP_SECOND_INTERVAL - (second % SLEEP_SECOND_INTERVAL);
  Serial.println(timeClient.getFormattedTime());
  Serial.print("sleep for minutes: ");
  Serial.println(minutesLeft);
  Serial.print("sleep for seconds: ");
  Serial.println(secondsLeft);
  sleepTime = (minutesLeft * 60 + secondsLeft) * 1000000;
}

template <class T>
void addVal(String & x, String key, T val) {
  x += key + ": ";
  x += String(val);
  x += "\n";
}

String createSensorYaml() {
  String yml = "---\n";
  addVal(yml, "location", location);
  addVal(yml, "ip-address", ip);
  addVal(yml, "moisture", moisture);
  addVal(yml, "temperature", temperature);
  addVal(yml, "temperature-unit", temperatureF ? "F" : "C");
  addVal(yml, "humidity", humidity);
  addVal(yml, "water-level", waterLevel);
  return yml;
}

void setup() {
  delay(1000);
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(1000);
  Serial.println('\n');

  state.registerVar(&cold_boot);
  cold_boot = !state.loadFromRTC();
  state.saveToRTC();
  
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);             // Connect to the network

  pinMode(LED_PIN, OUTPUT);

  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");
  while (!checkConnection()) {
    delay(1000);
  }

  readMoisture();
  readDHT();

  auto yml = createSensorYaml();
  Serial.println("sending yml:");
  Serial.println(yml);
  HTTPClient http;
  http.begin(request);
  http.addHeader("Content-Type", "text/plain");
  int retCode = http.POST(yml);
  Serial.print("http request complete, return code: ");
  Serial.println(retCode);

  timeClient.begin();
  timeClient.setTimeOffset(TIME_ZONE_OFFSET);
  readNTP();
  ESP.deepSleep(sleepTime);
}

void loop() {
}
