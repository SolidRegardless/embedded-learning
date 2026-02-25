/*
 * LESSON 10: STM32 — I2C OLED Display (SSD1306)
 * Board: STM32 Nucleo C031C6 (Cortex-M0+, 48MHz)
 * 
 * ============================================================
 * I2C ON STM32 vs OTHER PLATFORMS
 * ============================================================
 * 
 * I2C (Inter-Integrated Circuit), also called TWI (Two-Wire Interface):
 *   - SDA (data) and SCL (clock) — just two wires for many devices
 *   - Each device has a 7-bit address (0x00-0x7F)
 *   - Master (MCU) generates the clock, slaves respond
 *   - Typical speeds: 100kHz (standard), 400kHz (fast), 1MHz (fast-mode+)
 * 
 * STM32 I2C hardware features:
 *   - Dedicated I2C peripheral (not bit-banged)
 *   - Hardware address matching — can be a slave too
 *   - DMA support for I2C transfers (send display buffer via DMA!)
 *   - Clock stretching support
 *   - SMBus/PMBus protocol support (for power management ICs)
 *   - Configurable digital noise filter
 *   - Analog noise filter
 *   - Wakeup from Stop mode on address match (ultra-low power!)
 * 
 * Compare:
 *   AVR: Basic TWI hardware, 400kHz max, no DMA, no noise filter
 *   ESP32: Flexible I2C with GPIO matrix (any pins), but buggy driver historically
 *   RP2040: Good I2C hardware, DMA capable
 * 
 * ============================================================
 * I2C PROTOCOL REFRESHER
 * ============================================================
 * 
 *   START → [Address + R/W] → ACK → [Data] → ACK → ... → STOP
 *   
 *   SDA: ──┐ ┌─────────┐   ┌─┐ ┌────────┐   ┌─┐      ┌──
 *          └─┤ ADDRESS  ├───┤A├─┤  DATA  ├───┤A├──────┘
 *            └─────────┘   └─┘ └────────┘   └─┘
 *   SCL: ────┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌┐ ┌──
 *            └─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘
 * 
 * SSD1306 OLED address: typically 0x3C (or 0x3D if address pin is HIGH)
 * 
 * ============================================================
 * SSD1306 COMMANDS
 * ============================================================
 * 
 * The SSD1306 is a common 128x64 or 128x32 pixel OLED controller.
 * It communicates via I2C with a simple protocol:
 * 
 *   [0x3C] [control byte] [data byte(s)]
 * 
 * Control byte:
 *   0x00 = following bytes are COMMANDS
 *   0x40 = following bytes are display DATA (pixel data)
 * 
 * Key commands:
 *   0xAE = Display OFF
 *   0xAF = Display ON
 *   0xD5 = Set display clock divide ratio
 *   0x8D = Charge pump setting (0x14 = enable — needed for most modules)
 *   0x20 = Set memory addressing mode
 *   0xB0-0xB7 = Set page start address (for page addressing)
 *   0x00-0x0F = Set lower column start address
 *   0x10-0x1F = Set upper column start address
 * 
 * ============================================================
 * THIS LESSON: Drive SSD1306 OLED with Adafruit library
 * Show I2C address scanning + display text
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1      // No reset pin (shared with MCU reset)
#define OLED_ADDRESS  0x3C    // I2C address

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long bootTime;
int frameCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Lesson 10: STM32 I2C OLED Display");
  
  // ── I2C Address Scanner ──
  // This is incredibly useful for debugging I2C issues!
  // It tries every possible 7-bit address and reports which ones ACK.
  Serial.println("Scanning I2C bus...");
  Wire.begin();  // Initialize I2C as master
  
  /*
   * ══════════════════════════════════════════════════════════
   * WHAT Wire.begin() DOES ON STM32:
   * ══════════════════════════════════════════════════════════
   * 
   * 1. Enable GPIOB clock (SDA=PB7, SCL=PB6 on Nucleo C031C6)
   *    RCC->IOPENR |= RCC_IOPENR_GPIOBEN;
   * 
   * 2. Configure PB6/PB7 as Alternate Function, Open-Drain
   *    (I2C MUST be open-drain — the pull-up resistors pull HIGH,
   *     the device pulls LOW. This allows multiple devices to share the bus.)
   *    GPIOB->MODER  → AF mode
   *    GPIOB->OTYPER → Open-drain
   *    GPIOB->AFR    → AF6 (I2C1)
   * 
   * 3. Enable I2C1 clock
   *    RCC->APBENR1 |= RCC_APBENR1_I2C1EN;
   * 
   * 4. Configure I2C timing register (complex — use CubeMX to calculate!)
   *    I2C1->TIMINGR = 0x00301D28;  // 400kHz at 48MHz PCLK (example)
   *    
   *    The timing register encodes: prescaler, setup time, hold time,
   *    high period, low period. Getting this wrong = bus errors.
   *    STM32CubeMX has a calculator for this.
   * 
   * 5. Enable I2C peripheral
   *    I2C1->CR1 |= I2C_CR1_PE;
   * 
   * HAL equivalent:
   *   hi2c1.Instance             = I2C1;
   *   hi2c1.Init.Timing          = 0x00301D28;
   *   hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
   *   HAL_I2C_Init(&hi2c1);
   */
  
  int devicesFound = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("  Found device at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      
      // Identify common devices by address
      if (address == 0x3C || address == 0x3D) Serial.print(" (SSD1306 OLED)");
      if (address == 0x68) Serial.print(" (MPU6050 IMU / DS3231 RTC)");
      if (address == 0x76 || address == 0x77) Serial.print(" (BME280/BMP280)");
      if (address == 0x48) Serial.print(" (ADS1115 ADC / TMP102)");
      if (address == 0x27 || address == 0x3F) Serial.print(" (PCF8574 I/O expander)");
      Serial.println();
      devicesFound++;
    }
  }
  Serial.print("Found ");
  Serial.print(devicesFound);
  Serial.println(" device(s).");
  Serial.println();
  
  // ── Initialize OLED ──
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 init FAILED!");
    while (1); // Halt
  }
  
  /*
   * SSD1306_SWITCHCAPVCC tells the library to use the internal charge pump.
   * The SSD1306 needs ~7-8V to drive the OLED pixels, but we only have 3.3V.
   * The charge pump is a voltage doubler circuit built into the SSD1306 chip.
   * Without enabling it, the display stays dark.
   */
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("STM32 + SSD1306");
  display.println("Lesson 10: I2C OLED");
  display.println();
  display.println("Nucleo C031C6");
  display.println("Cortex-M0+ @ 48MHz");
  display.display();  // Push buffer to screen
  
  /*
   * display.display() sends the ENTIRE 128x64 framebuffer over I2C.
   * That's 128 * 64 / 8 = 1024 bytes of pixel data.
   * At 400kHz I2C, that takes ~20ms.
   * 
   * With HAL + DMA, you'd do:
   *   HAL_I2C_Mem_Write_DMA(&hi2c1, 0x3C<<1, 0x40, 1, buffer, 1024);
   * The CPU queues the transfer and returns immediately.
   * DMA handles sending all 1024 bytes. CPU is free for 20ms!
   * 
   * Without DMA (Arduino Wire library), the CPU waits for each byte.
   * At 30fps, that's 30 * 20ms = 600ms/sec spent on display updates!
   * With DMA: ~0ms of CPU time.
   */
  
  delay(2000);
  bootTime = millis();
}

void loop() {
  frameCount++;
  unsigned long uptime = (millis() - bootTime) / 1000;
  
  display.clearDisplay();
  
  // Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("=== STM32 OLED ===");
  
  // Uptime
  display.setCursor(0, 16);
  display.print("Uptime: ");
  display.print(uptime);
  display.println("s");
  
  // Frame counter
  display.setCursor(0, 28);
  display.print("Frame: ");
  display.println(frameCount);
  
  // Simulated temperature (in real code, read from a sensor)
  float temp = 22.5 + (sin(millis() / 5000.0) * 3.0);
  display.setCursor(0, 40);
  display.print("Temp: ");
  display.print(temp, 1);
  display.println(" C");
  
  // Draw a simple bar graph
  display.setCursor(0, 54);
  display.print("Level:");
  int barWidth = map((int)(temp * 10), 195, 255, 0, 80);
  display.fillRect(44, 54, barWidth, 8, SSD1306_WHITE);
  
  display.display();
  
  delay(100);
  
  /*
   * ══════════════════════════════════════════════════════════
   * I2C PULL-UP RESISTORS
   * ══════════════════════════════════════════════════════════
   * 
   * I2C uses open-drain outputs — the device can only pull LOW.
   * Pull-up resistors (typically 4.7kΩ to VCC) pull the line HIGH.
   * 
   * Most breakout boards include pull-ups. If you connect multiple
   * I2C devices, you might have too many pull-ups in parallel
   * (combined resistance too low → signal integrity issues).
   * 
   * Rule of thumb: total pull-up should be 2kΩ-10kΩ.
   * If you have 3 boards each with 4.7kΩ, combined = 1.6kΩ — too low!
   * Solution: desolder excess pull-ups, or use only one module's pull-ups.
   * 
   * STM32 has a DIGITAL noise filter (configurable number of clock cycles)
   * and an ANALOG noise filter built into the I2C peripheral.
   * These help in electrically noisy environments (near motors, switching
   * power supplies, etc.) — a feature AVR and ESP32 lack in hardware.
   */
}
