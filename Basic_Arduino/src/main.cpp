/*
 * Core2 for AWS IoT EduKit
 * Arduino Basic Connectivity Example v1.0.1
 * main.cpp
 * 
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>
#include <M5Core2.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <WiFi.h>
#include <FastLED.h>
#include <time.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include "arduino_secrets.h"

// NTP server details.
//
// NOTE: GMT offset is in seconds, so multiply hours by
// 3600 (e.g. Pacific Time would be -8 * 3600)
const char* ntp_server = "pool.ntp.org";
const long  gmt_offset_sec = 0 * 3600;
const int   daylight_offset_sec = 3600;

// 
const char wifi_ssid[]      = WIFI_SSID;
const char wifi_password[]  = WIFI_PASS;
const char endpoint_address[]      = AWS_IOT_ENDPOINT_ADDRESS;
const char* certificate  = THING_CERTIFICATE;

// Clients for Wi-Fi, SSL, and MQTT libraries
WiFiClient    wifi_client;            
BearSSLClient ssl_client(wifi_client);
MqttClient    mqtt_client(ssl_client);

// The MQTT client Id used in the connection to 
// AWS IoT Core. AWS IoT Core expects a unique client Id
// and the policy restricts which client Id's can connect
// to your broker endpoint address.
//
// NOTE: client_Id is set after the ATECC608 is initialized 
// to the value of the unique chip serial number.
String client_Id = "";

// Used to track how much time has elapsed since last MQTT 
// message publish.
unsigned long last_publish_millis = 0;

// Retrieves stored time_t object and returns seconds since
// Unix Epoch time.
unsigned long get_stored_time() {
  time_t seconds_since_epoch;
  struct tm time_info;
  
  if (!getLocalTime(&time_info)) {
    Serial.println("Failed to retrieve stored time.");
    return(0);
  }

  time(&seconds_since_epoch);
  return seconds_since_epoch;
}

// Retrieves the current time from the defined NTP server.
//
// NOTE: Time is stored in the ESP32, not the RTC using configTime.
void get_NTP_time() {
  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
}

// Connects to the specified Wi-Fi network using the defined
// SSID and password. A failed connection retries every 5 seconds.
void connect_wifi() {
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(wifi_ssid);

  while (WiFi.begin(wifi_ssid, wifi_password) != WL_CONNECTED) {
    Serial.println("Failed to connect. Retrying...");
    delay(5000);
  }
  Serial.println("Successfully connected to the Wi-Fi network!");
}

// Connects to the MQTT message broker, AWS IoT Core using
// the defined endpoint address at the default port 8883.
// A failed connection retries every 5 seconds.
// On a successful connection, it will then subscribe to a
// default MQTT topic, that is listening to everything
// on starting with a topic filter of the device name/.
// Changing the topic filter can cause the broker to disconnect
// the client session after successfully connecting if the thing policy 
// doesn't have sufficient authorization.
//
// NOTE: You must use the ATS endpoint address.
void connect_AWS_IoT() {
#define PORT 8883
  Serial.print("Attempting to AWS IoT Core message broker at mqtt:\\\\");
  Serial.print(endpoint_address);
  Serial.print(":");
  Serial.println(PORT);

  while (!mqtt_client.connect(endpoint_address, PORT)) {
    Serial.println("Failed to connect to AWS IoT Core. Retrying...");
    delay(5000);
  }

  Serial.println("Connected to AWS IoT Core!\n");

  // Subscribe to an MQTT topic.

  // NOTE: "#" is a wildcard, meaning that it will subscribe to all 
  // messages that start with the topic 'client_Id/' with the client_Id
  // being your device serial number.
  Serial.println(client_Id + "/#");
  mqtt_client.subscribe(client_Id + "/#");
}

// Publishes the MQTT message string to the MQTT broker. The thing must
// have authorization to publish to the topic, otherwise the connection
// to AWS IoT Core will disconnect.
// 
// NOTE: Use the "print" interface to send messages.
void publish_MQTT_message() {
  Serial.println("Publishing:");

  mqtt_client.beginMessage(client_Id + "/");
  mqtt_client.print("hello ");
  mqtt_client.print(millis());
  mqtt_client.endMessage();
}

// Callback for messages received on the subscribed MQTT
// topics. Use the Stream interface to loop until all contents
// are read.
void message_received_callback(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqtt_client.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  while (mqtt_client.available()) {
    Serial.print((char)mqtt_client.read());
  }
  Serial.println("\n");
}

// Initialize the ATECC608 secure element to use the stored private
// key in establishing TLS and securing network messages.
//
// NOTE: The definitions for I2C are in the platformio.ini file and
// not meant to be changed for the M5Stack Core2 for AWS IoT EduKit.
void atecc608_init(){
  Wire.begin(ACTA_I2C_SDA_PIN, ACTA_I2C_SCL_PIN, ACTA_I2C_BAUD);

  if (!ECCX08.begin(0x35)) {
    Serial.println("ATECC608 Secure Element initialization error!");
    while (1);
  }
  Serial.print("Device serial number: ");
  Serial.println(ECCX08.serialNumber());
}

void setup() {
  // Initialize the M5Stack Core2 for AWS IoT EduKit
  bool LCDEnable = true;
  bool SDEnable = true; 
  bool SerialEnable = true;
  bool I2CEnable = true;
  mbus_mode_t MBUSmode = kMBusModeOutput;
  M5.begin(LCDEnable, SDEnable, SerialEnable, I2CEnable, MBUSmode);
  
  // Initialize the secure element, connect to Wi-Fi, sync time
  atecc608_init();
  connect_wifi();
  get_NTP_time();
  
  // Set a callback to get the current time
  // used to validate the servers certificate
  ArduinoBearSSL.onGetTime(get_stored_time);

  // Uses the private key slot from the secure element and the
  // certificate you pasted into arduino_secrets.h,
  ssl_client.setEccSlot(ACTA_SLOT_PRIVATE_KEY, certificate);

  // The client Id for the MQTT client. Uses the ATECC608 serial number
  // as the unique client Id, as registered in AWS IoT, and set in the
  // thing policy.
  client_Id = ECCX08.serialNumber();
  client_Id.toLowerCase();
  mqtt_client.setId(client_Id);

  // The MQTT message callback, this function is called when 
  // the MQTT client receives a message on the subscribed topic
  mqtt_client.onMessage(message_received_callback);
}

void loop() {
  // Attempt to reconnect to Wi-Fi if disconnected.
  if (WiFi.status() != WL_CONNECTED) {
    connect_wifi();
  }

  // Attempt to reconnect to AWS IoT Core if disconnected.
  if (!mqtt_client.connected()) {
    connect_AWS_IoT();
  }

  // Poll for new MQTT messages and send keep alive pings.
  mqtt_client.poll();

  // Publish a message every 3 seconds or more.
  if (millis() - last_publish_millis > 3000) {
    last_publish_millis = millis();
    publish_MQTT_message();
  }
}
