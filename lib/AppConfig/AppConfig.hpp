#pragma once
/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Thorsten Ludewig (t.ludewig@gmail.com)
 */
#include <Arduino.h>
#include <CubeCell_NeoPixel.h>

// Magic number to identify valid EEPROM data
#define EEPROM_MAGIC 0x19660304

// Default sleep time in milliseconds
#define DEFAULT_SLEEPTIME 1200000
#define DEFAULT_SENDDELAY 0

// Structure to hold application configuration
typedef struct 
{
  uint32_t magic;      // Magic number to validate EEPROM data
  uint8_t appEui[8];   // Application EUI
  uint8_t devEui[8];   // Device EUI
  uint8_t appKey[16];  // Application Key
  uint32_t sleeptime;  // Sleep time in milliseconds
  uint32_t senddelay;  // Sleep time in milliseconds
} AppConfig;

// Structure to hold data to be transmitted from BME280 sensor
typedef struct 
{
  uint8_t preamble;   // Preamble byte
  uint8_t status;     // Status byte
  uint8_t battery;    // Battery voltage
  uint8_t crc8;     // CRC8 LE
} TxFrame_Data;

// Global instance of application configuration
extern AppConfig appConfig;
extern CubeCell_NeoPixel pixels;

// Function to initialize application configuration
extern void init_app_config();

// Function to write configuration to EEPROM
extern void write_config();

extern void showBoardLED(uint8_t r, uint8_t g, uint8_t b);
extern void clearBoardLED();
