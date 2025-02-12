/*
The MIT License (MIT)

Copyright (c) 2016 Seeed Technology Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include "BME280.h"

bool BME280::init(int i2c_addr)
{
  uint8_t retry = 0;
  uint8_t chip_id = 0;

  _devAddr = i2c_addr;
  Wire.begin();

  while ((retry++ < 5) && (chip_id != 0x60))
  {
    chip_id = BME280Read8(BME280_REG_CHIPID);
#ifdef BMP280_DEBUG_PRINT
    Serial.print("Read chip ID: ");
    Serial.println(chip_id);
#endif
    delay(100);
  }

  if (chip_id != 0x60)
  {
    Serial.println("Read Chip ID fail!");
    return false;
  }

  dig_T1 = BME280Read16LE(BME280_REG_DIG_T1);
  dig_T2 = BME280ReadS16LE(BME280_REG_DIG_T2);
  dig_T3 = BME280ReadS16LE(BME280_REG_DIG_T3);

  dig_P1 = BME280Read16LE(BME280_REG_DIG_P1);
  dig_P2 = BME280ReadS16LE(BME280_REG_DIG_P2);
  dig_P3 = BME280ReadS16LE(BME280_REG_DIG_P3);
  dig_P4 = BME280ReadS16LE(BME280_REG_DIG_P4);
  dig_P5 = BME280ReadS16LE(BME280_REG_DIG_P5);
  dig_P6 = BME280ReadS16LE(BME280_REG_DIG_P6);
  dig_P7 = BME280ReadS16LE(BME280_REG_DIG_P7);
  dig_P8 = BME280ReadS16LE(BME280_REG_DIG_P8);
  dig_P9 = BME280ReadS16LE(BME280_REG_DIG_P9);

  dig_H1 = BME280Read8(BME280_REG_DIG_H1);
  dig_H2 = BME280Read16LE(BME280_REG_DIG_H2);
  dig_H3 = BME280Read8(BME280_REG_DIG_H3);
  dig_H4 = (BME280Read8(BME280_REG_DIG_H4) << 4) | (0x0F & BME280Read8(BME280_REG_DIG_H4 + 1));
  dig_H5 = (BME280Read8(BME280_REG_DIG_H5 + 1) << 4) | (0x0F & BME280Read8(BME280_REG_DIG_H5) >> 4);
  dig_H6 = (int8_t)BME280Read8(BME280_REG_DIG_H6);

  writeRegister(BME280_REG_CONTROLHUMID, 0x05); //Choose 16X oversampling
  writeRegister(BME280_REG_CONTROL, 0xB7);      //Choose 16X oversampling

  delay(250);

  return true;
}

int32_t BME280::getTemperature(void)
{
  int32_t var1, var2;

  int32_t adc_T = BME280Read24(BME280_REG_TEMPDATA);

  // Check if the last transport successed
  if (!isTransport_OK)
  {
    return 0;
  }

  adc_T >>= 4;
  var1 = (((adc_T >> 3) - ((int32_t)(dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
         * ((int32_t)dig_T3)) >> 14;

  t_fine = var1 + var2;

  return (t_fine * 50 + 1280) >> 8;
}

uint32_t BME280::getPressure(void)
{
  int64_t var1, var2, var3, var4;

  // getTemperature(); // must be done first to get t_fine

  int32_t adc_P = BME280Read24(BME280_REG_PRESSUREDATA);
  if (adc_P == 0x800000) // value in case pressure measurement was disabled
    return NAN;
  adc_P >>= 4;

  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) * 131072);
  var2 = var2 + (((int64_t)dig_P4) * 34359738368);
  var1 = ((var1 * var1 * (int64_t)dig_P3) / 256) +
         ((var1 * ((int64_t)dig_P2) * 4096));
  var3 = ((int64_t)1) * 140737488355328;
  var1 = (var3 + var1) * ((int64_t)dig_P1) / 8589934592;

  if (var1 == 0) {
    return 0; // avoid exception caused by division by zero
  }

  var4 = 1048576 - adc_P;
  var4 = (((var4 * 2147483648) - var2) * 3125) / var1;
  var1 = (((int64_t)dig_P9) * (var4 / 8192) * (var4 / 8192)) /
         33554432;
  var2 = (((int64_t)dig_P8) * var4) / 524288;
  var4 = ((var4 + var1 + var2) / 256) + (((int64_t)dig_P7) * 16);

  return ( var4 * 10 ) / 256.0;
}

uint32_t BME280::getHumidity(void)
{
  int32_t v_x1_u32r, adc_H;

  // Call getTemperature to get t_fine
  // getTemperature();
  // Check if the last transport successed

  if (!isTransport_OK)
  {
    return 0;
  }

  adc_H = BME280Read16(BME280_REG_HUMIDITYDATA);

  v_x1_u32r = (t_fine - ((int32_t)76800));
  v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) + ((
                                                                                                     int32_t)16384)) >>
                15) *
               (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((
                                                                                                             int32_t)32768))) >>
                   10) +
                  ((int32_t)2097152)) *
                     ((int32_t)dig_H2) +
                 8192) >>
                14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

  uint64_t v = v_x1_u32r;
  v *= 1000;
  v /= 4194304;

  //  return (uint32_t)(v_x1_u32r >> 12) / 1024.0;
  return v;
}

uint8_t BME280::BME280Read8(uint8_t reg)
{
  Wire.beginTransmission(_devAddr);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom(_devAddr, 1);
  // return 0 if slave didn't response
  if (Wire.available() < 1)
  {
    isTransport_OK = false;
    return 0;
  }
  else
  {
    isTransport_OK = true;
  }

  return Wire.read();
}

uint16_t BME280::BME280Read16(uint8_t reg)
{
  uint8_t msb, lsb;

  Wire.beginTransmission(_devAddr);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom(_devAddr, 2);
  // return 0 if slave didn't response
  if (Wire.available() < 2)
  {
    isTransport_OK = false;
    return 0;
  }
  else
  {
    isTransport_OK = true;
  }
  msb = Wire.read();
  lsb = Wire.read();

  return (uint16_t)msb << 8 | lsb;
}

uint16_t BME280::BME280Read16LE(uint8_t reg)
{
  uint16_t data = BME280Read16(reg);
  return (data >> 8) | (data << 8);
}

int16_t BME280::BME280ReadS16(uint8_t reg)
{
  return (int16_t)BME280Read16(reg);
}

int16_t BME280::BME280ReadS16LE(uint8_t reg)
{
  return (int16_t)BME280Read16LE(reg);
}

uint32_t BME280::BME280Read24(uint8_t reg)
{
  uint32_t data;

  Wire.beginTransmission(_devAddr);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom(_devAddr, 3);
  // return 0 if slave didn't response
  if (Wire.available() < 3)
  {
    isTransport_OK = false;
    return 0;
  }
  else if (isTransport_OK == false)
  {
    isTransport_OK = true;
    if (!init(_devAddr))
    {
#ifdef BMP280_DEBUG_PRINT
      Serial.println("Device not connected or broken!");
#endif
    }
  }
  data = Wire.read();
  data <<= 8;
  data |= Wire.read();
  data <<= 8;
  data |= Wire.read();

  return data;
}

void BME280::writeRegister(uint8_t reg, uint8_t val)
{
  Wire.beginTransmission(_devAddr); // start transmission to device
  Wire.write(reg);                  // send register address
  Wire.write(val);                  // send value to write
  Wire.endTransmission();           // end transmission
}
