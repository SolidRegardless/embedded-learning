/*
 * LESSON 17: Pico — ADC & Internal Temperature Sensor
 * Board: Raspberry Pi Pico (RP2040, Dual Cortex-M0+, 125MHz, 264KB RAM)
 * 
 * ============================================================
 * RP2040 ADC OVERVIEW
 * ============================================================
 * 
 * The RP2040 has a 12-bit SAR (Successive Approximation Register) ADC:
 *   - 5 input channels (but only 4 external pins)
 *   - 500 ksps (kilo-samples per second) maximum
 *   - 3.3V reference (internal, tied to power supply)
 *   - 12-bit resolution → values 0 to 4095
 * 
 * ADC Channel Mapping:
 *   Channel 0 → GP26 (ADC0)
 *   Channel 1 → GP27 (ADC1)
 *   Channel 2 → GP28 (ADC2)
 *   Channel 3 → GP29 (ADC3) — used for VSYS voltage divider on Pico board
 *   Channel 4 → Internal temperature sensor (NOT a pin!)
 * 
 * ============================================================
 * ADC COMPARISON ACROSS PLATFORMS
 * ============================================================
 * 
 * Feature          | AVR (Uno)     | ESP32          | STM32 (C031)  | RP2040 (Pico)
 * ─────────────────|───────────────|────────────────|───────────────|──────────────
 * Resolution       | 10-bit (1024) | 12-bit (4096)  | 12-bit (4096) | 12-bit (4096)
 * Channels         | 6 (8 on SMD)  | 18             | 12            | 5 (4 external)
 * Reference        | 5V or 1.1V    | ~1.1V + atten  | 3.3V          | 3.3V (AVDD)
 * Sample rate      | ~15 ksps      | ~100 ksps      | ~1.1 Msps     | ~500 ksps
 * Noise            | Low           | HIGH (noisy!)  | Low           | Moderate
 * Internal temp    | Yes           | Yes            | Yes           | Yes (Ch4)
 * DMA support      | No            | Yes            | Yes           | Yes
 * 
 * ESP32 ADC quirks:
 *   - Non-linear at extremes (0-100mV and >3.1V are unreliable)
 *   - WiFi causes ADC2 to be unusable! Only ADC1 works with WiFi on.
 *   - Needs calibration for accurate readings
 * 
 * RP2040 ADC quirks:
 *   - Reference is AVDD (power supply) — not a precision reference
 *   - For accurate measurements, need external Vref or calibration
 *   - But much cleaner than ESP32 out of the box
 * 
 * ============================================================
 * SAR ADC: HOW IT WORKS
 * ============================================================
 * 
 * Successive Approximation Register ADC (used in RP2040, STM32, AVR):
 * 
 *   1. Sample the input voltage (sample-and-hold capacitor)
 *   2. Compare with Vref/2 → MSB is 1 if input > Vref/2, else 0
 *   3. Compare with Vref/4 (adjusted) → next bit
 *   4. Repeat for each bit (12 comparisons for 12-bit)
 *   5. Result: 12-bit binary representation of voltage
 * 
 * Like a binary search of the voltage range!
 * Takes 12 clock cycles minimum for 12 bits.
 * 
 * ============================================================
 * THIS LESSON:
 * - Read the internal temperature sensor (ADC channel 4)
 * - Read an external potentiometer on ADC0 (GP26)
 * - Convert raw ADC values to meaningful units
 * ============================================================
 */

const int POT_PIN = 26;  // GP26 = ADC0

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Lesson 17: RP2040 ADC & Temperature Sensor");
  Serial.println("Reading internal temp sensor (ADC4) and potentiometer (ADC0)");
  Serial.println();
  
  // Configure ADC pin
  // analogRead() works out of the box on Arduino-Pico framework.
  // The framework handles ADC initialization automatically.
  
  // Set ADC resolution to 12-bit (it's the default on Pico, but let's be explicit)
  analogReadResolution(12);
  
  /*
   * On other platforms:
   *   AVR: analogReadResolution(10) — only 10-bit available natively.
   *         Can call analogReadResolution(12) on some boards but it's
   *         just zero-padded, not real 12-bit resolution.
   *   ESP32: analogReadResolution(12) — default. Can set 9-12 bits.
   *          Also has analogSetAttenuation() for input range:
   *            ADC_0db   → 0-1.1V
   *            ADC_6db   → 0-1.35V
   *            ADC_11db  → 0-2.6V (default)
   *            ADC_12db  → 0-3.3V (added in recent ESP-IDF)
   *   STM32: analogReadResolution(12) — 12-bit native.
   *          HAL: ADC_RESOLUTION_12B, _10B, _8B, _6B
   */
  
  Serial.println("ADC configured: 12-bit resolution, 3.3V reference");
  Serial.println("──────────────────────────────────────────");
}

void loop() {
  // ═══════════════════════════════════════════
  // READ POTENTIOMETER (ADC Channel 0, GP26)
  // ═══════════════════════════════════════════
  
  int potRaw = analogRead(POT_PIN);  // 0-4095 for 12-bit
  
  // Convert to voltage: (raw / max) * Vref
  // Vref on Pico = 3.3V (AVDD)
  float potVoltage = potRaw * 3.3 / 4095.0;
  
  // Convert to percentage (useful for UI)
  float potPercent = potRaw * 100.0 / 4095.0;
  
  /*
   * ADC voltage calculation:
   *   voltage = (raw_value / (2^resolution - 1)) × Vref
   * 
   * Platform-specific:
   *   AVR:  voltage = raw / 1023.0 * 5.0    (10-bit, 5V ref)
   *   ESP32: voltage = raw / 4095.0 * 3.3   (12-bit, ~3.3V with 11dB atten)
   *   STM32: voltage = raw / 4095.0 * 3.3   (12-bit, 3.3V ref)
   *   Pico:  voltage = raw / 4095.0 * 3.3   (12-bit, 3.3V ref)
   * 
   * Note: ESP32 needs calibration for accuracy! The eFuse stores
   * calibration data that the esp_adc_cal library uses.
   */
  
  // ═══════════════════════════════════════════
  // READ INTERNAL TEMPERATURE SENSOR (ADC Channel 4)
  // ═══════════════════════════════════════════
  
  // The internal temperature sensor is on ADC channel 4.
  // Arduino-Pico provides analogReadTemp() for this!
  float temperatureC = analogReadTemp();
  
  /*
   * ══════════════════════════════════════════════════════════
   * HOW THE INTERNAL TEMP SENSOR WORKS (RP2040)
   * ══════════════════════════════════════════════════════════
   * 
   * The RP2040 datasheet gives this formula:
   *   T (°C) = 27 - (ADC_voltage - 0.706) / 0.001721
   * 
   * Where ADC_voltage is the voltage read on channel 4.
   * The sensor outputs ~706mV at 27°C, decreasing ~1.721mV per °C.
   * 
   * Under the hood, analogReadTemp() does:
   *   1. Select ADC channel 4 (temperature sensor)
   *   2. Read raw ADC value
   *   3. Convert to voltage: V = raw * 3.3 / 4095
   *   4. Apply formula: T = 27 - (V - 0.706) / 0.001721
   * 
   * Accuracy: ±2-3°C typical (it's measuring die temperature,
   * not ambient — will read higher under CPU load).
   * 
   * Other platforms' internal temp sensors:
   *   AVR: No internal temp sensor on ATmega328P (Uno).
   *        ATmega32U4 (Leonardo) has one, but ±10°C accuracy.
   *   ESP32: Built-in, ~±1°C, reads die temperature.
   *          temperatureRead() in Arduino framework.
   *   STM32: Built-in on all STM32. Calibration values stored in
   *          factory OTP memory. ±1.5°C with calibration.
   *          Requires ADC channel 16 or 18 (varies by family).
   */
  
  float temperatureF = temperatureC * 9.0 / 5.0 + 32.0;
  
  // ═══════════════════════════════════════════
  // DISPLAY RESULTS
  // ═══════════════════════════════════════════
  
  Serial.print("Pot: ");
  Serial.print(potRaw);
  Serial.print(" raw | ");
  Serial.print(potVoltage, 2);
  Serial.print("V | ");
  Serial.print(potPercent, 1);
  Serial.print("%    Temp: ");
  Serial.print(temperatureC, 1);
  Serial.print("°C (");
  Serial.print(temperatureF, 1);
  Serial.println("°F)");
  
  delay(500);
  
  /*
   * ══════════════════════════════════════════════════════════
   * IMPROVING ADC ACCURACY
   * ══════════════════════════════════════════════════════════
   * 
   * 1. OVERSAMPLING: Read multiple times and average.
   *    Reading 16 samples and averaging gives ~14-bit effective resolution
   *    (each 4× oversampling adds ~1 bit of resolution).
   * 
   *    int sum = 0;
   *    for (int i = 0; i < 16; i++) sum += analogRead(pin);
   *    int averaged = sum / 16;
   * 
   * 2. DECOUPLING: Add a 100nF capacitor close to the ADC pin.
   *    Reduces high-frequency noise from digital switching.
   * 
   * 3. EXTERNAL REFERENCE: For precision, use an external voltage
   *    reference IC (e.g., MCP1541 = 4.096V, REF3030 = 3.0V).
   *    RP2040 doesn't have a dedicated Vref pin, so you'd need
   *    an external ADC (like ADS1115 via I2C) for real precision.
   * 
   * 4. FILTERING: Apply a digital low-pass filter in software.
   *    Simple exponential moving average:
   *    filtered = alpha * newSample + (1 - alpha) * filtered;
   *    Where alpha = 0.1 for heavy smoothing, 0.5 for light.
   */
}
