function decodeUplink(input) {
  var data = {};
  
  data.preamble = input.bytes[0];
  data.status = input.bytes[1];

  // BME280 data
  // Temperature is at index 2
  var value = ((input.bytes[3] << 8) | input.bytes[2]);
  if (value & 0x8000) {
    value -= 0x10000; // 0x10000 entspricht 65536
  }
  data.temperature = value / 100.0;
  
  // Humidity is at index 4
  data.humidity = ((input.bytes[5] << 8) | input.bytes[4]) / 100.0;
  // Pressure is at index 6
  data.pressure = (((input.bytes[7] << 8) | input.bytes[6]) + 80000) / 100.0;

  // Battery voltage is at index 8
  data.batteryVoltage = (200.0 + input.bytes[8]) / 100.0;
  data.crc8le = input.bytes[9];

  // 100% battery is 4.1V
  // 0% battery is 2.5V
  var battery = data.batteryVoltage;
  if ( battery > 4.1 ) {
    battery = 4.1;
  }
  data.batteryPercentage =  Math.round((battery - 2.5) / (4.1 - 2.5) * 100.0);
  if ( data.batteryPercentage < 0 )
  {
    data.batteryPercentage = 0;
  }

  return {
    data: data,
    warnings: [],
    errors: []
  };
}
