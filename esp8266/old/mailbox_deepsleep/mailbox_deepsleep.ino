//
// mailbox_deepsleep.ino
// Naookie Sato
//
// Created by Naookie Sato on 04/18/2020
// Copyright © 2020 Naookie Sato. All rights reserved.
//

// ESP Includes
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <stdlib.h>
#include "config.h"
#include "utils.h"

#define ESP01

#ifdef ESP01
// This might need to be included when using some esp8266 arduino cores.
#include "pins_arduino.h"
#define LED_PIN 1
#else
#define LED_PIN BUILTIN_LED
#endif

#include <NTPClient.h>
#include <WiFiUdp.h>
#define TIME_ZONE_OFFSET -25200
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");


String ip;

void readNTP() {
  while (!timeClient.update()) {
    delay(500);
  }
}

String createSensorYaml() {
  String yml = "---\n";
  addKeyVal(yml, "location", location);
  addKeyVal(yml, "ip-address", ip);
  addKeyVal(yml, "mailbox-opened", true);
  addKeyVal(yml, "sensor-id", "esp01-1");
  addKeyVal(yml, "date", timeClient.getFormattedDate());
  addKeyVal(yml, "time", timeClient.getFormattedTime());
  return yml;
}

void setup() {
  setupSerial(115200, 2000);

  pinMode(LED_PIN, OUTPUT);
  blinkLed(1000, LED_PIN);

  timeClient.begin();
  timeClient.setTimeOffset(TIME_ZONE_OFFSET);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);             // Connect to the network

  ip = waitForConnection([]() { blinkLed(500, LED_PIN); });

  readNTP();

  auto yml = createSensorYaml();
  Serial.println("sending yml:");
  Serial.println(yml);
  HTTPClient http;
  http.begin(request);
  http.addHeader("Content-Type", "text/plain");
  int retCode = http.POST(yml);
  Serial.print("http request complete, return code: ");
  Serial.println(retCode);

  ESP.deepSleep(0);
}

void loop() {
}
