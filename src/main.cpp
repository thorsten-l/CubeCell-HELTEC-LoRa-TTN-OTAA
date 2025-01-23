#include <LoRaWanMinimal_APP.h>
#include <Arduino.h>
#include <AppConfig.hpp>
#include <AppConfig.hpp>

// Set these OTAA parameters to match your app/node in TTN
//  everything msb

uint16_t userChannelsMask[6] = {0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
static TxFrame_Data txFrame;

///////////////////////////////////////////////////
// Some utilities for going into low power mode
TimerEvent_t sleepTimer;
// Records whether our sleep/low power timer expired
bool sleepTimerExpired;

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
  // Low power handler also gets interrupted by other timers
  // So wait until our timer had expired
  while (!sleepTimerExpired)
    lowPowerHandler();
  TimerStop(&sleepTimer);
  digitalWrite(Vext, LOW);
}

uint8_t crc8( uint8_t *data, int length)
{
  uint8_t crc = 0x00; // Initialwert
  while (length--)
  {
    crc ^= *data++;
    // 8 Bits verarbeiten
    for (uint8_t i = 0; i < 8; i++)
    {
      // Prüfen, ob das MSB gesetzt ist
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
  return crc; // kein Final-XOR
}

///////////////////////////////////////////////////
void setup()
{
  init_app_config();
  LoRaWAN.begin(CLASS_A, LORAMAC_REGION_EU868);

  // Enable ADR
  LoRaWAN.setAdaptiveDR(true);

  while (1)
  {
    showBoardLED(0, 0, 50);

    Serial.print("Joining... ");
    LoRaWAN.joinOTAA(appConfig.appEui, appConfig.appKey, appConfig.devEui);
    if (!LoRaWAN.isJoined())
    {
      // In this example we just loop until we're joined, but you could
      // also go and start doing other things and try again later

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

///////////////////////////////////////////////////
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

  txFrame.crc8 = crc8((uint8_t *)&txFrame, sizeof(TxFrame_Data) - 1);

#ifdef DEBUG
  printf("crc8 = %d\n", txFrame.crc8);
#endif

  bool success = LoRaWAN.send(sizeof(TxFrame_Data), (uint8_t *)&txFrame, 1, false);

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

///////////////////////////////////////////////////
// Example of handling downlink data
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
      write_config(); // store sleep time in eeprom
    }
  }
}
