//
// config.h
// Naookie Sato
//
// Created by Naookie Sato on 03/20/2019
// Copyright © 2020 Naookie Sato. All rights reserved.
//

#ifndef _CONFIG_H_
#define _CONFIG_H_

// Wifi Info
const char* ssid      = "";
const char* password  = "";

String server_address = "10.0.0.14";
String sensor_id       = "d1-1";
String server_port    = "5050";
String endpoint       = "update";
String request_string = "http://" + server_address + ":" + server_port + "/" + endpoint;
const char* request   = request_string.c_str();


#endif  // _CONFIG_H_
