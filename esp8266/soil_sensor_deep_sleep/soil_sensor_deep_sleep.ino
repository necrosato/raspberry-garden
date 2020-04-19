//
// soil_sensor.ino
// Naookie Sato
//
// Created by Naookie Sato on 04/14/2020
// Copyright © 2020 Naookie Sato. All rights reserved.
//

// ESP Inclused
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <stdlib.h>
// This might need to be included when using some esp8266 arduino cores.
//#include "pins_arduino.h"

// for 1306 oled display
#include "config.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
// Definte display reset pin, -1 for RST
#define OLED_RESET -1
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

// NOTE: TO ACTUALLY CONNECT 1306 128x64 LCD Display I2C - 
/*
    arduino I2C standards define the following:
        SDA = GPIO4
        SCL = GPIO5

    so to connect an I2C Display to NodeMCU: 
        SDA = GPIO4 = D2 (NodeMCU Pin - see ./pins_arduino.h)
        SCL = GPIO5 = D1 (NodeMCU Pin - see ./pins_arduino.h)
    
*/

Adafruit_SSD1306 display(OLED_RESET);
DHT dht(DHT_PIN, DHT_TYPE);

int wifi_status;

String ip;
int moisture;
float temperature;
float humidity;

void readMoisture() {
  moisture = analogRead(MOISTURE_PIN);
  Serial.print("Moisture: ");
  Serial.println(moisture);
}

void drawState(int x, int y, int size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print("STATE: ");
  if (moisture > dryThresh) {
    display.println("DRY");
  } else if (moisture < wetThresh) {
    display.println("WET");
  } else {
    display.println("OK");
  }
}

void drawMoisture(int x, int y, int size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  auto moistureStr = String(moisture);
  display.print("MOISTURE: ");
  display.println(moistureStr.c_str());
}

void drawTemperature(int x, int y, int size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  auto temperatureStr = String(temperature);
  display.print("TEMP: ");
  display.print(temperatureStr.c_str());
  display.println(temperatureF ? " F" : " C");
}

void drawHumidity(int x, int y, int size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  auto humidityStr = String(humidity);
  display.print("HUMID: ");
  display.print(humidityStr.c_str());
  display.println(" %");
}

void drawIp(int x, int y, int size) { display.setTextSize(size);
  display.setCursor(x, y);
  display.print("IP: ");
  display.println(ip.c_str());
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

void setup() {
  delay(1000);
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(1000);
  Serial.println('\n');

  state.registerVar(&cold_boot);
  cold_boot = !state.loadFromRTC();
  state.saveToRTC();
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  display.setTextColor(WHITE);

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
  timeClient.begin();
  timeClient.setTimeOffset(TIME_ZONE_OFFSET);

  readMoisture();
  readDHT();

  display.clearDisplay();
  drawIp(0, 0, 1);
  drawMoisture(0, 20, 1);
  drawTemperature(0, 30, 1);
  drawHumidity(0, 40, 1);
  drawState(0, 50, 2);
  display.display();
  
  readNTP();
  ESP.deepSleep(sleepTime);
}

void loop() {
}
