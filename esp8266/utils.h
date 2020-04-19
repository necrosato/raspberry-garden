//
// utils.h
// Naookie Sato
//
// Created by Naookie Sato on 04/19/2020
// Copyright © 2020 Naookie Sato. All rights reserved.
//

#ifndef _RASPBERRY_GARDEN_ESP8266_UTILS_H_
#define _RASPBERRY_GARDEN_ESP8266_UTILS_H_

void blinkLed(int half_time, int led_pin, bool swap = false) {
    // Low and high swapped for onboard led
    digitalWrite(led_pin, swap ? HIGH : LOW);
    delay(half_time);
    digitalWrite(led_pin, swap ? LOW : HIGH);
    delay(half_time);
}

// template argument is function callback every time ip cannot be obtained
template <class T>
String waitForConnection(T fail_func) {
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  String ip = "";
  while (ip == "") {
    if (WiFi.status() == WL_CONNECTED){
      Serial.println("Your ESP is connected!");
      Serial.println("Your IP address is: ");
      Serial.println(WiFi.localIP());
      Serial.println("");
      ip = WiFi.localIP().toString();
    } else {
      fail_func();
      Serial.println("WiFi not connected");
      Serial.println("");
    }
  }
  return ip;
}

// Adds comma separated 'key: val' to string x
template <class T>
void addKeyVal(String & x, String key, T val) {
  x += key + ": ";
  x += String(val);
  x += "\n";
}

void setupSerial(int baud, int timeout) {
  Serial.begin(baud);
  Serial.setTimeout(timeout);
  // Wait for serial to initialize.
  while(!Serial) { }
  Serial.println('\n');
}

#endif  // _RASPBERRY_GARDEN_ESP8266_UTILS_H_
