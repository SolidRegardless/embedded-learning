/*
 * LESSON 7: STM32 — GPIO & HAL vs Registers
 * Board: STM32 Nucleo C031C6 (Cortex-M0+, 48MHz, 32KB Flash, 12KB RAM)
 * 
 * ============================================================
 * THE BIG DIFFERENCE: CLOCK GATING
 * ============================================================
 * 
 * On AVR (Arduino Uno) and ESP32, peripherals are always clocked — you just
 * use them. STM32 is different: EVERY peripheral starts with its clock OFF.
 * You MUST enable the clock before touching any peripheral register.
 * 
 * Why? Power. An STM32L4 in sleep with all clocks gated can draw <1µA.
 * An ESP32 with everything running draws ~240mA. Clock gating is how 
 * battery-powered devices last years on a coin cell.
 * 
 * ============================================================
 * STM32 CLOCK TREE (simplified)
 * ============================================================
 * 
 *   HSI (16MHz internal RC)  ──┐
 *   HSE (external crystal)  ───┤──> PLL ──> SYSCLK (up to 48MHz on C031C6)
 *   LSI (32kHz internal)    ───┘              │
 *   LSE (32.768kHz ext)                       ├──> AHB bus ──> Core, DMA, memory
 *                                             │     │
 *                                             │     └──> AHB peripherals (GPIO, CRC)
 *                                             │
 *                                             └──> APB bus ──> UART, SPI, I2C, TIM, ADC
 * 
 * The RCC (Reset and Clock Control) peripheral controls all of this.
 * Each bus has an "enable register" — you set a bit to clock a peripheral.
 * 
 * ============================================================
 * AHB vs APB BUSES
 * ============================================================
 * 
 * AHB (Advanced High-performance Bus):
 *   - Full system clock speed
 *   - Used for: GPIO, DMA, memory controllers
 *   - Wide data bus (32-bit), single-cycle access
 * 
 * APB (Advanced Peripheral Bus):
 *   - Can run at a divided clock speed (saves power)
 *   - Used for: UART, SPI, I2C, Timers, ADC
 *   - Simpler protocol, lower power
 * 
 * GPIO is on AHB because you want the fastest possible pin toggling.
 * UART is on APB because serial at 115200 baud doesn't need 48MHz access.
 * 
 * ============================================================
 * GPIO REGISTERS ON STM32
 * ============================================================
 * 
 * Each GPIO port (A, B, C, D, F) has these registers:
 * 
 *   MODER   — Mode Register (2 bits per pin)
 *             00 = Input, 01 = Output, 10 = Alternate Function, 11 = Analog
 *   OTYPER  — Output Type (1 bit per pin)
 *             0 = Push-pull, 1 = Open-drain
 *   OSPEEDR — Output Speed (2 bits per pin)
 *             Controls slew rate. Faster = more EMI noise but less rise time
 *   PUPDR   — Pull-Up/Pull-Down (2 bits per pin)
 *             00 = None, 01 = Pull-up, 10 = Pull-down
 *   IDR     — Input Data Register (read pin state)
 *   ODR     — Output Data Register (read/write output state)
 *   BSRR    — Bit Set/Reset Register (atomic set/clear — like ESP32!)
 * 
 * Compare to AVR: DDRx (direction), PORTx (output), PINx (input) — 3 registers.
 * STM32 has 7+ registers per port. More complex, but more flexible.
 * 
 * ============================================================
 * THIS LESSON: Blink an LED using Arduino (STM32duino) framework
 * with comments showing what the HAL and raw registers would do.
 * ============================================================
 * 
 * On Wokwi, the Nucleo C031C6 uses the STM32duino (Arduino) framework.
 * The built-in LED is on PA5 (Arduino pin D13 / LED_BUILTIN).
 * We'll also connect an external LED to PA0 (Arduino pin A0).
 */

const int EXT_LED_PIN = PA0;  // External LED on PA0

void setup() {
  Serial.begin(115200);
  Serial.println("Lesson 7: STM32 GPIO — HAL vs Registers");
  
  // ── Arduino way (STM32duino) ──
  // Just like ESP32/AVR — abstractions hide the complexity
  pinMode(LED_BUILTIN, OUTPUT);  // PA5 — onboard LED
  pinMode(EXT_LED_PIN, OUTPUT);  // PA0 — external LED
  
  /*
   * ══════════════════════════════════════════════════════════
   * WHAT THE HAL (Hardware Abstraction Layer) DOES BEHIND THE SCENES:
   * ══════════════════════════════════════════════════════════
   * 
   * STM32 HAL is ST's official C library. When you use it directly
   * (outside Arduino), initializing a GPIO pin looks like this:
   * 
   *   // 1. Enable the clock for GPIOA (CRITICAL — skip this and it won't work!)
   *   __HAL_RCC_GPIOA_CLK_ENABLE();
   *   
   *   // 2. Configure the pin
   *   GPIO_InitTypeDef gpio = {0};
   *   gpio.Pin   = GPIO_PIN_5;           // PA5
   *   gpio.Mode  = GPIO_MODE_OUTPUT_PP;  // Push-pull output
   *   gpio.Pull  = GPIO_NOPULL;          // No internal pull-up/down
   *   gpio.Speed = GPIO_SPEED_FREQ_LOW;  // Low slew rate (saves EMI)
   *   HAL_GPIO_Init(GPIOA, &gpio);
   *   
   *   // 3. Control the pin
   *   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // HIGH
   *   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // LOW
   *   HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);                // Toggle
   * 
   * The HAL is verbose but portable across ALL STM32 families
   * (F0, F1, F4, H7, L4, G4, C0, U5...). Same API, different chips.
   * 
   * ══════════════════════════════════════════════════════════
   * WHAT THE RAW REGISTERS LOOK LIKE:
   * ══════════════════════════════════════════════════════════
   * 
   * On the STM32C031C6 (Cortex-M0+):
   * 
   *   // 1. Enable GPIOA clock via RCC
   *   //    RCC->IOPENR bit 0 = GPIOAEN
   *   RCC->IOPENR |= RCC_IOPENR_GPIOAEN;  // (1 << 0)
   *   
   *   // 2. Set PA5 as output
   *   //    MODER has 2 bits per pin. PA5 = bits [11:10]
   *   //    Clear both bits first, then set to 01 (output)
   *   GPIOA->MODER &= ~(0x3 << (5 * 2));  // Clear bits 10-11
   *   GPIOA->MODER |=  (0x1 << (5 * 2));  // Set to 01 = output
   *   
   *   // 3. Set PA5 HIGH using BSRR (atomic, no read-modify-write!)
   *   GPIOA->BSRR = (1 << 5);       // Set bit 5 → pin HIGH
   *   
   *   // 4. Set PA5 LOW using BSRR upper half
   *   GPIOA->BSRR = (1 << (5 + 16)); // Reset bit 5 → pin LOW
   *   
   *   // Alternative: toggle via ODR (NOT atomic — beware in ISRs!)
   *   GPIOA->ODR ^= (1 << 5);
   * 
   * ══════════════════════════════════════════════════════════
   * BSRR: WHY TWO HALVES?
   * ══════════════════════════════════════════════════════════
   * 
   * BSRR is a 32-bit register:
   *   Bits [15:0]  — SET:   writing 1 sets the corresponding pin HIGH
   *   Bits [31:16] — RESET: writing 1 sets the corresponding pin LOW
   * 
   * This is WRITE-ONLY and ATOMIC. You don't need to read-modify-write.
   * No race condition if an interrupt fires between read and write.
   * 
   * AVR has a similar trick: writing 1 to PINx toggles the pin.
   * ESP32 has GPIO_OUT_W1TS_REG (set) and GPIO_OUT_W1TC_REG (clear).
   * Same concept, different names.
   */
}

void loop() {
  // Blink both LEDs in alternating pattern
  
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(EXT_LED_PIN, LOW);
  Serial.println("Built-in ON, External OFF");
  delay(500);
  
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(EXT_LED_PIN, HIGH);
  Serial.println("Built-in OFF, External ON");
  delay(500);
  
  /*
   * PERFORMANCE NOTE:
   * 
   * digitalWrite() on STM32duino is faster than on AVR Arduino:
   *   - AVR Arduino: ~60 clock cycles (table lookup, disable interrupts, etc.)
   *   - STM32duino: ~10-15 cycles (more direct register access)
   *   - Raw BSRR write: 1-2 cycles
   *   - ESP32 Arduino: ~30-40 cycles
   * 
   * For LED blinking, it doesn't matter. For bit-banging a protocol
   * at MHz speeds, it matters enormously.
   * 
   * ══════════════════════════════════════════════════════════
   * STM32 vs ESP32 vs AVR: GPIO COMPARISON
   * ══════════════════════════════════════════════════════════
   * 
   * Feature         | STM32          | ESP32           | AVR
   * ────────────────|────────────────|─────────────────|──────────
   * Clock gating    | Required!      | Automatic       | Automatic
   * Output modes    | Push-pull/OD   | Push-pull/OD    | Push-pull/OD
   * Slew rate ctrl  | Yes (4 speeds) | No              | No
   * Alternate func  | Per-pin mux    | GPIO matrix     | Fixed
   * Atomic set/clr  | BSRR           | W1TS/W1TC       | PINx toggle
   * Registers/port  | 7+             | Spread across   | 3
   * Pin voltage     | 3.3V (5V tol*) | 3.3V only       | 5V
   * 
   * *Some STM32 pins are 5V tolerant on input — check the datasheet!
   *  The C031C6 has 5V-tolerant pins marked "FT" in the datasheet.
   *  This is a HUGE advantage when interfacing with 5V sensors.
   */
}
