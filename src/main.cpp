#include <LoRaWanMinimal_APP.h>
#include <Arduino.h>
#include <AppConfig.hpp>
#include <AppConfig.hpp>

uint16_t userChannelsMask[6] = {0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
TimerEvent_t sleepTimer;
bool sleepTimerExpired;

// static ////////////////////////////////////////////////////////////////////

static TxFrameData txFrame;

static void wakeUp()
{
  sleepTimerExpired = true;
}

static void lowPowerSleep(uint32_t sleeptime)
{
  digitalWrite(Vext, HIGH);
  sleepTimerExpired = false;
  TimerInit(&sleepTimer, &wakeUp);
  TimerSetValue(&sleepTimer, sleeptime);
  TimerStart(&sleepTimer);
  while (!sleepTimerExpired)
    lowPowerHandler();
  TimerStop(&sleepTimer);
  digitalWrite(Vext, LOW);
}

static uint8_t crc8( uint8_t *data, int length)
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

#ifdef DEBUG
  Serial.printf("\n*** Sending packet ***\n");
#endif

  txFrame.preamble = 0x5A;
  txFrame.status = 0x01;

  digitalWrite(Vext, HIGH);
  delay(50);

  txFrame.battery = (getBatteryVoltage() - 2000) / 10;

#ifdef DEBUG
  printf("battery = %d\n", txFrame.battery + 200);
#endif

  txFrame.crc8 = crc8((uint8_t *)&txFrame, sizeof(TxFrameData) - 1);

#ifdef DEBUG
  printf("crc8 = %d\n", txFrame.crc8);
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
