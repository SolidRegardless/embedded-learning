/*
 * LESSON 19: Cross-Platform Comparison — Same Task, Four Ways
 * Board: ESP32 DevKit C V4 (runs here, but explained for all platforms)
 * 
 * ============================================================
 * THE TASK: Button → Debounce → Toggle LED → Print Count
 * ============================================================
 * 
 * Simple enough to implement on any MCU, complex enough to reveal
 * how each platform approaches the same fundamental operations:
 *   1. Configure GPIO (input + output)
 *   2. Debounce a button
 *   3. Toggle an LED
 *   4. Count presses and print via serial
 * 
 * This code RUNS on ESP32. Comments show exactly how you'd do it
 * on STM32, AVR, and RP2040 (Pico).
 * 
 * ============================================================
 * PLATFORM OVERVIEW
 * ============================================================
 * 
 * Feature         | AVR (Uno)      | ESP32           | STM32 (C031C6)  | RP2040 (Pico)
 * ────────────────|────────────────|─────────────────|─────────────────|──────────────
 * Architecture    | 8-bit AVR      | 32-bit Xtensa   | 32-bit Cortex-M | 32-bit Cortex-M
 * Clock           | 16MHz          | 240MHz          | 48MHz           | 125MHz
 * Flash           | 32KB           | 4MB (external)  | 32KB            | 2MB (external)
 * RAM             | 2KB            | 520KB           | 12KB            | 264KB
 * Cores           | 1              | 2               | 1               | 2
 * GPIO voltage    | 5V             | 3.3V            | 3.3V (some 5V)  | 3.3V
 * Cost            | ~$3            | ~$3             | ~$2             | ~$0.70
 * WiFi/BT         | No             | Yes             | No              | No (Pico W: yes)
 * ────────────────────────────────────────────────────────────────────
 */

// ═══════════════════════════════════════════════════════════
// PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════

const int LED_PIN    = 2;    // Built-in LED on ESP32 DevKit
const int BUTTON_PIN = 4;    // External button

/*
 * PIN DEFINITIONS ON OTHER PLATFORMS:
 * 
 * AVR (Arduino Uno):
 *   const int LED_PIN    = 13;   // PB5, built-in LED
 *   const int BUTTON_PIN = 2;    // PD2 (also INT0 for interrupts)
 *   // Pin numbers map to port/bit: pin 13 = PORTB bit 5
 *   // This mapping is fixed in hardware (no GPIO matrix)
 * 
 * STM32 (Nucleo C031C6):
 *   const int LED_PIN    = PA5;  // Built-in LED (D13 equivalent)
 *   const int BUTTON_PIN = PC13; // Built-in user button (active LOW)
 *   // STM32duino uses PAx, PBx, PCx naming
 *   // Under HAL: GPIOA pin 5, GPIOC pin 13
 * 
 * RP2040 (Pico):
 *   const int LED_PIN    = 25;   // Built-in LED (GP25)
 *   const int BUTTON_PIN = 15;   // Any GPIO, e.g., GP15
 *   // Pico uses GP0-GP29, simple numbering
 */

// ═══════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════

volatile bool ledState = false;
volatile uint32_t pressCount = 0;

// Debounce variables
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 50;
bool lastButtonState = HIGH;
bool buttonState = HIGH;

/*
 * VOLATILE KEYWORD — why and when:
 * 
 * 'volatile' tells the compiler: "Don't optimise reads/writes to this
 * variable — its value can change outside normal program flow."
 * 
 * Required when:
 *   - Variable is modified in an ISR and read in loop()
 *   - Variable is shared between cores (RP2040/ESP32)
 *   - Variable is mapped to a hardware register
 * 
 * Without volatile, the compiler might:
 *   - Cache the value in a CPU register and never re-read from RAM
 *   - Reorder reads/writes for "optimisation"
 *   - Eliminate "redundant" reads (it doesn't know the ISR changes it)
 * 
 * Platform nuances:
 *   AVR: volatile is sufficient (8-bit, single core, no cache)
 *   ESP32: volatile alone is NOT enough for multi-core!
 *          Use atomic operations or FreeRTOS mutexes.
 *   STM32: volatile works for ISR↔main, but for DMA buffers
 *          you also need cache management on Cortex-M7 (SCB_CleanDCache).
 *   RP2040: volatile works for ISR↔main on same core.
 *           For cross-core: use hardware spinlocks or mutexes.
 */

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  // ── Serial Initialisation ──
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Lesson 19: Cross-Platform Comparison");
  Serial.println("Press the button to toggle the LED");
  Serial.println();
  
  /*
   * SERIAL INIT ACROSS PLATFORMS:
   * 
   * ESP32 (Arduino):
   *   Serial.begin(115200);
   *   // Uses UART0 by default. ESP32 has 3 UARTs.
   *   // Can remap to any pins: Serial.begin(115200, SERIAL_8N1, RX, TX);
   *   // USB CDC also available on ESP32-S2/S3 (native USB)
   * 
   * AVR (Arduino Uno):
   *   Serial.begin(115200);
   *   // Uses UART0 (pins 0/1). Only 1 UART on ATmega328P.
   *   // 115200 at 16MHz has 2.1% baud rate error (acceptable).
   *   // Max practical: ~1Mbps. No DMA — every byte causes an interrupt.
   * 
   * STM32 (STM32duino):
   *   Serial.begin(115200);
   *   // On Nucleo: Serial = USB CDC (virtual COM via ST-Link)
   *   // Serial1, Serial2 = USART1, USART2 on real pins
   *   // STM32 UART has DMA support — can send/receive without CPU
   * 
   *   // HAL version:
   *   // UART_HandleTypeDef huart2;
   *   // huart2.Instance = USART2;
   *   // huart2.Init.BaudRate = 115200;
   *   // huart2.Init.WordLength = UART_WORDLENGTH_8B;
   *   // huart2.Init.StopBits = UART_STOPBITS_1;
   *   // huart2.Init.Parity = UART_PARITY_NONE;
   *   // HAL_UART_Init(&huart2);
   *   // DON'T FORGET: enable USART2 clock first!
   *   // __HAL_RCC_USART2_CLK_ENABLE();
   * 
   * RP2040 (Arduino-Pico):
   *   Serial.begin(115200);
   *   // Serial = USB CDC (native USB on RP2040)
   *   // Serial1 = UART0 (GP0=TX, GP1=RX)
   *   // Serial2 = UART1 (GP4=TX, GP5=RX)
   *   // Or use PIO for unlimited extra UARTs!
   */
  
  // ── GPIO Configuration ──
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  /*
   * GPIO CONFIG ACROSS PLATFORMS:
   * 
   * ════════════════════════════════════════
   * ESP32 (Arduino):
   * ════════════════════════════════════════
   *   pinMode(LED_PIN, OUTPUT);
   *   pinMode(BUTTON_PIN, INPUT_PULLUP);
   *   // ESP32 has internal pull-up AND pull-down on most pins
   *   // GPIO matrix: any peripheral can be routed to (almost) any pin
   *   // Strapping pins (GPIO0, 2, 5, 12, 15) affect boot — be careful!
   * 
   *   // Register level:
   *   // GPIO_ENABLE_REG |= (1 << LED_PIN);       // Enable output
   *   // GPIO_OUT_W1TS_REG = (1 << LED_PIN);       // Set HIGH (atomic)
   *   // GPIO_OUT_W1TC_REG = (1 << LED_PIN);       // Set LOW (atomic)
   *   // REG_SET_BIT(GPIO_PIN_REG(pin), FUN_PU);  // Enable pull-up
   * 
   * ════════════════════════════════════════
   * AVR (Arduino Uno):
   * ════════════════════════════════════════
   *   pinMode(LED_PIN, OUTPUT);     // DDRB |= (1 << PB5);
   *   pinMode(BUTTON_PIN, INPUT_PULLUP);
   *   // INPUT_PULLUP: DDRD &= ~(1 << PD2); PORTD |= (1 << PD2);
   *   // AVR pull-up: write 1 to PORT when DDR is input
   *   // No pull-DOWN on AVR! External resistor needed.
   *   // No slew rate control, no drive strength selection.
   *   // All pins are 5V tolerant (it IS 5V logic).
   * 
   * ════════════════════════════════════════
   * STM32 (HAL):
   * ════════════════════════════════════════
   *   // MUST enable clock first!
   *   __HAL_RCC_GPIOA_CLK_ENABLE();
   *   __HAL_RCC_GPIOC_CLK_ENABLE();
   *   
   *   GPIO_InitTypeDef gpio = {0};
   *   
   *   // LED output
   *   gpio.Pin = GPIO_PIN_5;
   *   gpio.Mode = GPIO_MODE_OUTPUT_PP;  // Push-pull
   *   gpio.Pull = GPIO_NOPULL;
   *   gpio.Speed = GPIO_SPEED_FREQ_LOW;
   *   HAL_GPIO_Init(GPIOA, &gpio);
   *   
   *   // Button input with pull-up
   *   gpio.Pin = GPIO_PIN_13;
   *   gpio.Mode = GPIO_MODE_INPUT;
   *   gpio.Pull = GPIO_PULLUP;
   *   HAL_GPIO_Init(GPIOC, &gpio);
   *   
   *   // STM32 has: push-pull, open-drain, pull-up, pull-down,
   *   // 4 speed settings, alternate function mux
   * 
   * ════════════════════════════════════════
   * RP2040 (Pico SDK):
   * ════════════════════════════════════════
   *   gpio_init(LED_PIN);
   *   gpio_set_dir(LED_PIN, GPIO_OUT);
   *   
   *   gpio_init(BUTTON_PIN);
   *   gpio_set_dir(BUTTON_PIN, GPIO_IN);
   *   gpio_pull_up(BUTTON_PIN);
   *   
   *   // RP2040 has: pull-up, pull-down, drive strength (2/4/8/12mA),
   *   // slew rate (slow/fast), schmitt trigger (per pin!)
   *   // gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
   */
  
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("GPIO configured. Ready!");
  Serial.println("──────────────────────────────────");
}

// ═══════════════════════════════════════════════════════════
// MAIN LOOP — DEBOUNCE + TOGGLE + PRINT
// ═══════════════════════════════════════════════════════════

void loop() {
  // ── Read button with debounce ──
  bool reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != buttonState) {
      buttonState = reading;
      
      // Toggle on press (LOW because INPUT_PULLUP)
      if (buttonState == LOW) {
        ledState = !ledState;
        pressCount++;
        
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        
        Serial.print("Press #");
        Serial.print(pressCount);
        Serial.print(" — LED is ");
        Serial.println(ledState ? "ON" : "OFF");
      }
    }
  }
  
  lastButtonState = reading;
  
  /*
   * ══════════════════════════════════════════════════════════
   * DEBOUNCE STRATEGIES ACROSS PLATFORMS
   * ══════════════════════════════════════════════════════════
   * 
   * SOFTWARE DEBOUNCE (used above — works on all platforms):
   *   Simple millis()-based delay. Portable, easy, reliable.
   *   Downside: polling in loop() — can miss short presses if loop is slow.
   * 
   * INTERRUPT + DEBOUNCE (better for responsive systems):
   * 
   * ESP32:
   *   attachInterrupt(BUTTON_PIN, isr, FALLING);
   *   // ISR runs on the core that called attachInterrupt()
   *   // Use portENTER_CRITICAL() for shared variable access
   *   // Can also use FreeRTOS task notification from ISR:
   *   //   xTaskNotifyFromISR(taskHandle, 0, eNoAction, &woken);
   * 
   * AVR:
   *   attachInterrupt(digitalPinToInterrupt(2), isr, FALLING);
   *   // Only pins 2 and 3 support external interrupts on Uno!
   *   // Or use Pin Change Interrupts (PCINT) for any pin
   *   //   PCICR |= (1 << PCIE0);         // Enable PCINT group 0
   *   //   PCMSK0 |= (1 << PCINT0);       // Enable pin
   *   // PCINT fires on ANY change — you must check direction in ISR
   * 
   * STM32:
   *   // EXTI (External Interrupt) — any pin can be an interrupt source!
   *   // But only one pin per number across ports (PA0 OR PB0, not both)
   *   
   *   // HAL way:
   *   gpio.Mode = GPIO_MODE_IT_FALLING;  // Interrupt on falling edge
   *   HAL_GPIO_Init(GPIOC, &gpio);
   *   HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
   *   HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
   *   // Then implement: void HAL_GPIO_EXTI_Callback(uint16_t pin)
   *   
   *   // STM32duino way:
   *   attachInterrupt(PC13, isr, FALLING);  // Same as Arduino API
   * 
   * RP2040:
   *   attachInterrupt(BUTTON_PIN, isr, FALLING);
   *   // Interrupt fires on the core that set it up!
   *   // All GPIO interrupts share one IRQ (IO_IRQ_BANK0)
   *   // The framework dispatches to the correct handler
   *   
   *   // Pico SDK way:
   *   gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_FALL, true, &callback);
   * 
   * HARDWARE DEBOUNCE (best for noisy environments):
   *   RC filter: 10kΩ resistor + 100nF capacitor on the button line
   *   Time constant = R × C = 10kΩ × 100nF = 1ms
   *   Schmitt trigger input cleans up the signal further
   *   RP2040 has per-pin Schmitt trigger enable!
   *   STM32 inputs are always Schmitt-triggered.
   *   AVR inputs have Schmitt triggers too.
   * 
   * ══════════════════════════════════════════════════════════
   * TIMING FUNCTIONS
   * ══════════════════════════════════════════════════════════
   * 
   * millis() / micros() — available on all Arduino platforms, but
   * implemented differently:
   * 
   * AVR: Timer0 overflow interrupt, incrementing a counter.
   *      Resolution: 1ms (millis), 4µs (micros).
   *      millis() wraps at ~49.7 days (32-bit unsigned).
   * 
   * ESP32: Uses the FreeRTOS tick counter or esp_timer (64-bit, µs).
   *        esp_timer_get_time() gives microseconds since boot.
   *        Higher resolution than AVR.
   * 
   * STM32: SysTick timer (24-bit down-counter, fires every 1ms).
   *        HAL_GetTick() returns ms since boot.
   *        For µs: use DWT cycle counter or a hardware timer.
   *        DWT->CYCCNT / (SystemCoreClock / 1000000) = µs
   * 
   * RP2040: Uses the 64-bit hardware timer (1µs resolution).
   *         time_us_64() in Pico SDK — won't overflow for 584,942 years!
   *         Best timer of the lot.
   * 
   * ══════════════════════════════════════════════════════════
   * MEMORY MODEL DIFFERENCES
   * ══════════════════════════════════════════════════════════
   * 
   * AVR:
   *   - Harvard architecture: separate program (Flash) and data (SRAM) buses
   *   - Strings in code go to Flash AND RAM unless you use PROGMEM/F()
   *   - Serial.println(F("text")) keeps string in Flash only — saves RAM!
   *   - Only 2KB RAM — every byte matters
   * 
   * ESP32:
   *   - Modified Harvard with unified address space
   *   - Code runs from external SPI Flash via cache (XIP)
   *   - IRAM_ATTR: put function in internal RAM (required for ISRs!)
   *   - DRAM_ATTR: put data in internal RAM
   *   - PSRAM: some boards have 4-8MB external RAM
   * 
   * STM32:
   *   - Modified Harvard (Cortex-M)
   *   - Code in internal Flash (fast, no external SPI needed)
   *   - Data in SRAM (12KB on C031C6, up to 1MB on H7!)
   *   - CCM RAM on some families (tightly coupled, fastest)
   *   - Flash latency: may need wait states at high clock speeds
   *     (C031C6 at 48MHz: 1 wait state for Flash access)
   * 
   * RP2040:
   *   - Code in external QSPI Flash via XIP cache (like ESP32)
   *   - 264KB SRAM in 6 banks (striped for dual-core access)
   *   - Can copy functions to RAM: __not_in_flash_func(myFunc)
   *   - No Flash wait states (XIP cache handles it)
   * 
   * ══════════════════════════════════════════════════════════
   * CLOCK SETUP DIFFERENCES
   * ══════════════════════════════════════════════════════════
   * 
   * AVR: No setup needed. ATmega328P runs at 16MHz from external
   *       crystal. Fuse bits set clock source at programming time.
   *       Can prescale: CLKPR register (divide by 1/2/4/8/16/32/64/128/256)
   * 
   * ESP32: Configured by the framework/bootloader.
   *        setCpuFrequencyMhz(80/160/240) to change at runtime.
   *        Lower clock = less power but slower WiFi/BT.
   * 
   * STM32: Most complex! RCC (Reset & Clock Control) configures:
   *        - Clock source (HSI, HSE, PLL)
   *        - PLL multiplier/divider
   *        - AHB/APB bus prescalers
   *        - Flash wait states (must match clock speed!)
   *        SystemClock_Config() is typically 50+ lines of HAL code.
   *        CubeMX generates this — don't write it by hand.
   * 
   * RP2040: Simpler than STM32. Boot from 12MHz crystal, PLL to 125MHz.
   *         set_sys_clock_khz(133000, true) to change.
   *         Can overclock to 250MHz+ (no guarantees though).
   */
}

/*
 * ══════════════════════════════════════════════════════════
 * FINAL SUMMARY: CHOOSING A PLATFORM
 * ══════════════════════════════════════════════════════════
 * 
 * Choose AVR when:
 *   - Learning the fundamentals (simplest architecture)
 *   - 5V logic is needed
 *   - Very low power sleep (ATmega328PB: ~0.1µA in power-down)
 *   - Massive community, libraries, tutorials
 *   - Simple projects with few peripherals
 * 
 * Choose ESP32 when:
 *   - WiFi or Bluetooth needed
 *   - Lots of RAM/Flash needed
 *   - Rapid prototyping (great Arduino support)
 *   - IoT applications
 *   - Cost-sensitive high-volume (WiFi MCU under $3)
 * 
 * Choose STM32 when:
 *   - Professional/commercial product development
 *   - Maximum peripheral flexibility (ADC, DAC, timers, CAN, USB)
 *   - Motor control (advanced timers with dead-time, break)
 *   - Ultra-low-power applications (STM32L series: <100nA shutdown)
 *   - Safety-critical systems (automotive, medical)
 *   - Wide range: 8¢ (STM32C0) to $15 (STM32H7 with 1MB RAM)
 * 
 * Choose RP2040 when:
 *   - PIO needed (custom protocols, unusual I/O requirements)
 *   - Dual-core without RTOS complexity
 *   - Cost is paramount ($0.70!)
 *   - USB host/device (native USB)
 *   - Education (well-documented, beginner-friendly)
 *   - Deterministic I/O timing (PIO)
 * 
 * No platform is "best" — each has its sweet spot.
 * Understanding all four makes you a versatile embedded developer.
 */
