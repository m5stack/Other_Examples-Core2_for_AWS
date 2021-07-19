/*
 * Core2 for AWS IoT EduKit
 * Arduino Basic Blinking LED Example v1.0.0
 * Blink.cpp
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
#include "Wire.h"
#include "WiFi.h"

#include "FastLED.h"

void setup()
{
  // initialize the M5Stack Core2 for AWS IoT EduKit
  bool LCDEnable = true;
  bool SDEnable = true; 
  bool SerialEnable = true;
  bool I2CEnable = false;
  mbus_mode_t MBUSmode = kMBusModeOutput;
  M5.begin(LCDEnable, SDEnable, SerialEnable, I2CEnable, MBUSmode);
}

void loop()
{
  // turn the LED on (HIGH is the voltage level)
  M5.Axp.SetLed(1);
      
  // wait for a second
  delay(1000);

  // turn the LED off by making the voltage LOW
  M5.Axp.SetLed(0);
  
  // wait for a second
  delay(1000);
}
