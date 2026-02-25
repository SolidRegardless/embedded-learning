/*
 * LESSON 5: I2C — Reading a Temperature Sensor
 * 
 * CONCEPTS:
 * - I2C (Inter-Integrated Circuit, pronounced "I-squared-C")
 * - Two-wire protocol: SDA (data) + SCL (clock)
 * - Device addresses: each I2C device has a 7-bit address
 * - Master/slave architecture: MCU is master, sensors are slaves
 * - Wire library: Arduino's I2C abstraction
 * 
 * HOW I2C WORKS:
 * Unlike UART (point-to-point), I2C is a BUS — multiple devices share 
 * the same two wires. Each device has a unique address (like an IP address).
 * 
 *   MCU (Master)
 *    |  |
 *   SDA SCL ← two wires, shared
 *    |  |
 *    +--+--- Sensor 1 (addr 0x48)
 *    |  |
 *    +--+--- Display  (addr 0x3C)
 *    |  |
 *    +--+--- EEPROM   (addr 0x50)
 * 
 * Communication flow:
 * 1. Master sends START condition
 * 2. Master sends device address + read/write bit
 * 3. Addressed slave responds with ACK
 * 4. Data transfer happens
 * 5. Master sends STOP condition
 * 
 * We're using the DHT22 temperature/humidity sensor on Wokwi.
 * (It's actually a one-wire protocol, not I2C — but Wokwi also 
 * supports I2C devices. We'll use both to show the difference.)
 */

#include "DHTesp.h"  // Wokwi includes this library

const int DHT_PIN = 15;   // GPIO15 — DHT22 data pin
DHTesp dht;

void setup() {
  Serial.begin(115200);
  
  // Initialize DHT22 sensor
  dht.setup(DHT_PIN, DHTesp::DHT22);
  
  Serial.println("========================================");
  Serial.println("  Lesson 5: Temperature Sensor");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Reading DHT22 every 2 seconds...");
  Serial.println();
}

void loop() {
  // DHT22 needs ~2 seconds between reads (hardware limitation)
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 2000) return;
  lastRead = millis();
  
  // Read the sensor
  TempAndHumidity data = dht.getTempAndHumidity();
  
  // Check for read errors
  if (dht.getStatus() != 0) {
    Serial.print("DHT22 error: ");
    Serial.println(dht.getStatusString());
    return;
  }
  
  // Display results
  float tempC = data.temperature;
  float tempF = tempC * 9.0 / 5.0 + 32.0;
  float humidity = data.humidity;
  
  // Heat index — "feels like" temperature accounting for humidity
  float heatIndex = dht.computeHeatIndex(tempC, humidity, false);
  
  Serial.print("Temp: ");
  Serial.print(tempC, 1);
  Serial.print("°C (");
  Serial.print(tempF, 1);
  Serial.print("°F) | Humidity: ");
  Serial.print(humidity, 1);
  Serial.print("% | Feels like: ");
  Serial.print(heatIndex, 1);
  Serial.println("°C");
}

/*
 * I2C SCANNING — BONUS TECHNIQUE:
 * 
 * When you connect an unknown I2C device, you can scan for it:
 * 
 *   #include <Wire.h>
 *   
 *   Wire.begin();  // Join I2C bus as master
 *   for (byte addr = 1; addr < 127; addr++) {
 *     Wire.beginTransmission(addr);
 *     if (Wire.endTransmission() == 0) {
 *       Serial.print("Found device at 0x");
 *       Serial.println(addr, HEX);
 *     }
 *   }
 * 
 * Common I2C addresses:
 *   0x3C/0x3D — SSD1306 OLED display
 *   0x48      — TMP102 temperature sensor
 *   0x68      — MPU6050 accelerometer/gyro
 *   0x76/0x77 — BME280 pressure/temp/humidity
 * 
 * KEY DIFFERENCE FROM DESKTOP:
 * On desktop, sensors are abstracted behind OS drivers.
 * In embedded, YOU are the driver. You send raw bytes over 
 * the I2C bus and interpret the response according to the 
 * sensor's datasheet. Libraries like DHTesp do this for you,
 * but understanding the protocol matters for debugging.
 */
