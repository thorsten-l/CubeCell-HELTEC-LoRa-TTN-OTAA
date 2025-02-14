/*
 * Copyright 2025 Thorsten Ludewig (t.ludewig@gmail.com)
 *
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
 */

#include <LoRaWanMinimal_APP.h>
#include <EEPROM.h>
#include "AppConfig.hpp"

AppConfig appConfig;

CubeCell_NeoPixel pixels(1, RGB, NEO_GRB + NEO_KHZ800);

static void initBoardLED()
{
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  pixels.begin();
  pixels.clear();
}

static uint8_t *generateDevEUIByChipID()
{
  uint8_t *devEui = new uint8_t[8];
  uint64_t chipID = getID();
  for (int i = 7; i >= 0; i--)
  {
    devEui[i] = (chipID >> (8 * (7 - i))) & 0xFF;
  }
  return devEui;
}

static void printHex(char *label, uint8_t *buffer, int length)
{
  Serial.print(label);
  Serial.print(": ");
  for (int i = 0; i < length; i++)
  {
    Serial.printf("%02X", buffer[i]);
  }
  Serial.println();
}

static void read_config()
{
  EEPROM.begin(512);

  for (int i = 0; i < sizeof(AppConfig); i++)
  {
    *((uint8_t *)&appConfig + i) = EEPROM.read(i);
  }

  EEPROM.end();
}

void write_config()
{
  EEPROM.begin(512);
  for (int i = 0; i < sizeof(AppConfig); i++)
  {
    EEPROM.write(i, *((uint8_t *)&appConfig + i));
  }
  EEPROM.commit();
  EEPROM.end();
}

void init_app_config()
{
  initBoardLED();
  showBoardLED(50, 0, 0);

  Serial.begin(115200);
  pinMode(GPIO7, INPUT_PULLUP);
  delay(6000);

  Serial.println("\n\nLoRaWAN TTN OTAA, Version: " APP_VERSION " (c)2025 Thorsten Ludewig (t.ludewig@gmail.com)");
  Serial.println("Build timestamp: " __DATE__ " " __TIME__);

  uint64_t chipID = getID();
  Serial.printf("ChipID: %04X%08X\n\n", (uint32_t)(chipID >> 32), (uint32_t)chipID);

  bool reconfigure = digitalRead(GPIO7) == LOW;

  Serial.printf("GPIO0: %s\n\n", reconfigure ? "LOW" : "HIGH");
  read_config();
  Serial.println("\nEEPROM read done.\n");

  if (appConfig.magic == EEPROM_MAGIC && !reconfigure)
  {
    Serial.println("*** valid eeprom magic found!");
    Serial.println("*** using config from eeprom.");
  }
  else
  {
    showBoardLED(25, 25, 0);
    Serial.println("*** NO valid eeprom magic found!");
    Serial.println("*** writing random config to eeprom.");
    appConfig.magic = EEPROM_MAGIC;

#ifdef DEVELOPMENT_SLEEPTIME_VALUE
    appConfig.sleeptime = DEVELOPMENT_SLEEPTIME_VALUE;
#else
    appConfig.sleeptime = DEFAULT_SLEEPTIME;
#endif

    appConfig.senddelay = DEFAULT_SENDDELAY;
    uint8_t *d = generateDevEUIByChipID();

    for (int i = 0; i < 8; i++)
    {
#ifdef CREATE_DEV_EUI_CHIPID
      appConfig.devEui[i] = *(d + i);
#endif
#ifdef CREATE_DEV_EUI_RANDOM
      appConfig.devEui[i] = cubecell_random(256) & 0xFF;
#endif
      appConfig.appEui[i] = cubecell_random(256) & 0xFF;
    }
    appConfig.devEui[0] = (appConfig.devEui[0] & ~1) | 2;
    appConfig.appEui[0] = (appConfig.appEui[0] & ~1) | 2;

    for (int i = 0; i < 16; i++)
    {
      appConfig.appKey[i] = cubecell_random(256) & 0xFF;
    }

    write_config();
    delay(2000);
  }

  printf("\nMagic    : %08x\n", appConfig.magic);
  printf("Sleeptime: %dms\n", appConfig.sleeptime);
  printf("Senddelay: %dms\n\n", appConfig.senddelay);
  printHex("AppEUI", appConfig.appEui, 8);
  printHex("DevEUI", appConfig.devEui, 8);
  printHex("AppKey", appConfig.appKey, 16);
}


void showBoardLED(uint8_t r, uint8_t g, uint8_t b)
{
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  delay(50);
  pixels.show();
}

void clearBoardLED()
{
  pixels.clear();
  delay(50);
  pixels.show();
}
