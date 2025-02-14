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
#include <Arduino.h>
#include <AppConfig.hpp>
#include <AppConfig.hpp>

#ifdef HAS_BME280
#include <BME280.h>
BME280 bme; // use I2C interface
#endif

uint16_t userChannelsMask[6] = {0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
TimerEvent_t sleepTimer;
bool sleepTimerExpired;

// static ////////////////////////////////////////////////////////////////////

static TxFrameData txFrame;
static uint32_t loopStart;

static void wakeUp()
{
  sleepTimerExpired = true;
}

static void lowPowerSleep(uint32_t sleeptime)
{
  digitalWrite(Vext, HIGH);
  sleepTimerExpired = false;
  TimerInit(&sleepTimer, &wakeUp);
  TimerSetValue(&sleepTimer, sleeptime - (millis() - loopStart));
  TimerStart(&sleepTimer);
  while (!sleepTimerExpired)
    lowPowerHandler();
  TimerStop(&sleepTimer);
  digitalWrite(Vext, LOW);
  delay(100);
}

static uint8_t crc8(uint8_t *data, int length)
{
  uint8_t crc = 0x00;
  while (length--)
  {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++)
    {
      if (crc & 0x80)
      {
        crc = (uint8_t)((crc << 1) ^ 0x07);
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc;
}

//////////////////////////////////////////////////////////////////////////////

void setup()
{
  init_app_config();
  LoRaWAN.begin(CLASS_A, LORAMAC_REGION_EU868);
  LoRaWAN.setAdaptiveDR(true);

  while (1)
  {
    showBoardLED(0, 0, 50);

    Serial.print("Joining... ");
    LoRaWAN.joinOTAA(appConfig.appEui, appConfig.appKey, appConfig.devEui);
    if (!LoRaWAN.isJoined())
    {
      Serial.println("JOIN FAILED! Sleeping for 30 seconds");
      lowPowerSleep(30000);
    }
    else
    {
      Serial.println("JOINED");
      showBoardLED(0, 50, 0);
      delay(2000);
      break;
    }
  }
}

void loop()
{
  loopStart = millis();

#ifdef DEBUG
  Serial.printf("\n*** Sending packet ***\n");
#endif

  txFrame.preamble = 0x5A;
  txFrame.status = 0x01;

#ifdef HAS_BME280
  if (!bme.init())
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring, "
                   "address, sensor ID!");
  }

  for (int i = 0; i < SENSOR_READ_ITERATIONS; i++)
  {
    if (i > 0)
    {
      delay(SENSOR_READ_ITERATION_DELAY);
    }
    txFrame.temperature = (bme.getTemperature() / 10) & 0xFFFF;
    txFrame.humidity = (bme.getHumidity() / 10) & 0xFFFF;
    txFrame.pressure = (((bme.getPressure() / 10) - 80000) & 0xFFFF);
#ifdef DEBUG
    Serial.print(".");
    Serial.flush();
#endif
  }

  Wire.end();
  Serial.println();

#ifdef DEBUG
  printf("temperature = %.02f°C [%d]\n", txFrame.temperature / 100.0, txFrame.temperature);
  printf("humidity = %.02f%% [%d]\n", txFrame.humidity / 100.0, txFrame.humidity);
  printf("pressure = %.02fhPa [%d]\n", (txFrame.pressure + 80000) / 100.0, txFrame.pressure);
#endif
#endif

  digitalWrite(Vext, HIGH);
  delay(50);

  txFrame.battery = (getBatteryVoltage() - 2000) / 10;
  txFrame.crc8 = crc8((uint8_t *)&txFrame, sizeof(TxFrameData) - 1);

#ifdef DEBUG
  printf("battery = %0.2fV\n", (txFrame.battery + 200) / 100.0);
  printf("crc8 = %d\n", txFrame.crc8);
  printf("TxFrameData size = %d\n", sizeof(TxFrameData));
#endif

  bool success = LoRaWAN.send(sizeof(TxFrameData), (uint8_t *)&txFrame, 1, false);

#ifdef DEBUG
  if (success)
  {
    Serial.println("Send OK");
  }
  else
  {
    Serial.println("Send FAILED");
  }
  delay(50);
#endif

  lowPowerSleep(appConfig.sleeptime);
}

void downLinkDataHandle(McpsIndication_t *mcpsIndication)
{
#ifdef DEBUG
  Serial.printf("Received downlink: %s, RXSIZE %d, PORT %d, DATA: ", mcpsIndication->RxSlot ? "RXWIN2" : "RXWIN1", mcpsIndication->BufferSize, mcpsIndication->Port);
  for (uint8_t i = 0; i < mcpsIndication->BufferSize; i++)
  {
    Serial.printf("%02X", mcpsIndication->Buffer[i]);
  }
  Serial.println();
#endif

  uint8_t *msg = mcpsIndication->Buffer;

  if (mcpsIndication->BufferSize == 6 && msg[0] == 0xa5 && msg[1] == 0x01)
  {
    uint32_t setSleeptime =
        ((msg[2] << 24) + (msg[3] << 16) + (msg[4] << 8) + msg[5]);
#ifdef DEBUG
    printf("set sleeptime = %d\n", setSleeptime);
#endif

    if (setSleeptime <= 86400000) // one day
    {
      appConfig.sleeptime = setSleeptime;
      write_config();
    }
  }
}
