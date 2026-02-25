/*
 * LESSON 15: Pico — Dual Core
 * Board: Raspberry Pi Pico (RP2040, Dual Cortex-M0+, 125MHz, 264KB RAM)
 * 
 * ============================================================
 * THE RP2040: A DUAL-CORE MICROCONTROLLER
 * ============================================================
 * 
 * The RP2040 has TWO Cortex-M0+ cores running at 125MHz.
 * Both cores can run code SIMULTANEOUSLY — true parallelism.
 * 
 * This is different from ESP32's dual core:
 *   ESP32: Dual Xtensa LX6 @ 240MHz, but one core usually runs WiFi/BT.
 *          FreeRTOS manages task scheduling. You can pin tasks to cores.
 *   RP2040: Both cores are fully available. No WiFi stack eating one core.
 *           No RTOS required (though you can use one).
 * 
 * Single core MCUs (AVR, most STM32):
 *   - One thing at a time, switching via interrupts
 *   - Interrupt latency matters a lot
 *   - Simpler but limited
 * 
 * Dual core MCUs:
 *   - True parallel execution
 *   - Core 0 can run a control loop while Core 1 handles display/comms
 *   - Shared memory means fast communication
 *   - But also means: race conditions, cache coherency, synchronization
 * 
 * ============================================================
 * RP2040 MULTICORE ARCHITECTURE
 * ============================================================
 * 
 *   ┌──────────────┐  ┌──────────────┐
 *   │   Core 0     │  │   Core 1     │
 *   │  Cortex-M0+  │  │  Cortex-M0+  │
 *   │   125MHz     │  │   125MHz     │
 *   └──────┬───────┘  └──────┬───────┘
 *          │                  │
 *          │    ┌────────┐    │
 *          ├────┤  FIFO  ├────┤  ← Hardware mailbox (32-bit, 8 entries each way)
 *          │    └────────┘    │
 *          │                  │
 *   ┌──────┴──────────────────┴──────┐
 *   │         AHB Bus Fabric          │
 *   ├─────────────────────────────────┤
 *   │  264KB SRAM  │  Peripherals     │
 *   │  (6 banks)   │  (GPIO, ADC,     │
 *   │              │   UART, SPI...)  │
 *   └──────────────┴──────────────────┘
 * 
 * Key features:
 *   - SRAM is banked (6 banks) — both cores can access different banks
 *     simultaneously without contention
 *   - Hardware FIFO mailbox: 8-entry queue in each direction for
 *     lightweight core-to-core messaging
 *   - Hardware spinlocks: 32 hardware spinlocks for mutual exclusion
 *   - SIO (Single-cycle I/O): Each core has its own divider, interpolator,
 *     and GPIO access — no sharing conflicts for fast operations
 * 
 * ============================================================
 * ARDUINO-PICO FRAMEWORK DUAL CORE API
 * ============================================================
 * 
 * The Arduino-Pico framework makes dual core dead simple:
 * 
 *   setup()  / loop()  → Run on Core 0 (default)
 *   setup1() / loop1() → Run on Core 1 (add these functions!)
 * 
 * That's it! Just define setup1/loop1 and Core 1 starts automatically.
 * 
 * Communication between cores:
 *   - Shared global variables (with volatile + mutex)
 *   - rp2040.fifo.push() / rp2040.fifo.pop() — hardware FIFO
 *   - mutex_enter_blocking() / mutex_exit() — Pico SDK mutexes
 * 
 * ============================================================
 * THIS LESSON:
 * Core 0: Blinks an LED at 1Hz
 * Core 1: Reads a button and prints to serial
 * Both run simultaneously — true parallelism!
 * ============================================================
 */

const int LED_PIN = 25;     // Built-in LED on Pico (GP25)
const int BUTTON_PIN = 15;  // Button on GP15

// Shared variables between cores — MUST be volatile!
volatile uint32_t core0_counter = 0;
volatile uint32_t core1_counter = 0;
volatile bool buttonPressed = false;

// ═══════════════════════════════════════════
// CORE 0: LED blinker + status printer
// ═══════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial connection
  
  Serial.println("Lesson 15: RP2040 Dual Core");
  Serial.println("Core 0: LED blink at 1Hz");
  Serial.println("Core 1: Button monitoring");
  Serial.println();
  
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Blink LED at 1Hz
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
  
  core0_counter++;
  
  // Print status every 2 seconds
  if (core0_counter % 2 == 0) {
    Serial.print("[Core 0] Blink #");
    Serial.print(core0_counter);
    Serial.print(" | Core 1 loops: ");
    Serial.print(core1_counter);
    Serial.print(" | Button: ");
    Serial.println(buttonPressed ? "PRESSED" : "released");
  }
  
  /*
   * Note: delay() on Core 0 does NOT block Core 1!
   * Both cores have independent execution. While Core 0 is in delay(),
   * Core 1 continues running loop1() at full speed.
   * 
   * This is fundamentally different from cooperative multitasking
   * (where delay blocks everything) or even FreeRTOS on ESP32
   * (where delay() yields to the scheduler, but it's still one
   *  core doing the task switching).
   */
}

// ═══════════════════════════════════════════
// CORE 1: Button monitor
// ═══════════════════════════════════════════

void setup1() {
  // setup1 runs on Core 1 — configure button here
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  /*
   * Both cores share the same GPIO hardware.
   * pinMode() is safe to call from either core.
   * But be careful: if both cores try to write to the SAME pin
   * at the same time, you get undefined behavior.
   * 
   * Rule: each pin should be "owned" by one core.
   *   Core 0 owns: LED_PIN (output)
   *   Core 1 owns: BUTTON_PIN (input)
   */
}

void loop1() {
  // Read button (with simple debounce)
  static bool lastState = HIGH;
  static unsigned long lastChange = 0;
  
  bool state = digitalRead(BUTTON_PIN);
  
  if (state != lastState && (millis() - lastChange > 50)) {
    lastChange = millis();
    lastState = state;
    buttonPressed = (state == LOW);
    
    if (buttonPressed) {
      // Send a message to Core 0 via the hardware FIFO
      // This is a 32-bit hardware queue — no software overhead!
      rp2040.fifo.push(core1_counter);
      
      Serial.print("[Core 1] Button pressed! Sent FIFO message: ");
      Serial.println(core1_counter);
      
      /*
       * ══════════════════════════════════════════════════════════
       * HARDWARE FIFO MAILBOX
       * ══════════════════════════════════════════════════════════
       * 
       * The RP2040 has two 32-bit, 8-entry FIFOs:
       *   Core 0 → Core 1: rp2040.fifo.push() on C0, rp2040.fifo.pop() on C1
       *   Core 1 → Core 0: rp2040.fifo.push() on C1, rp2040.fifo.pop() on C0
       * 
       * The push/pop is automatically routed based on which core calls it!
       * 
       * Features:
       *   - Blocking and non-blocking variants
       *   - Available check: rp2040.fifo.available()
       *   - IRQ on data available
       *   - Zero-copy, hardware-managed
       * 
       * Compare to ESP32:
       *   ESP32 uses FreeRTOS queues (software) for inter-task communication.
       *   xQueueSend() / xQueueReceive() — works but has RTOS overhead.
       *   RP2040's hardware FIFO is faster and simpler.
       */
    }
  }
  
  core1_counter++;
  
  // Small delay to not hog the bus
  delay(1);
  
  // Check for FIFO messages from Core 0 (non-blocking)
  if (rp2040.fifo.available()) {
    uint32_t msg = rp2040.fifo.pop();
    Serial.print("[Core 1] Received FIFO from Core 0: ");
    Serial.println(msg);
  }
}

/*
 * ══════════════════════════════════════════════════════════
 * MULTICORE PITFALLS
 * ══════════════════════════════════════════════════════════
 * 
 * 1. RACE CONDITIONS:
 *    Both cores read/write shared data → corrupted values
 *    Solution: use volatile, mutexes, or atomic operations
 * 
 *    // Bad:
 *    sharedCounter++;  // Read-modify-write is NOT atomic!
 *    
 *    // Good:
 *    mutex_enter_blocking(&myMutex);
 *    sharedCounter++;
 *    mutex_exit(&myMutex);
 * 
 * 2. SERIAL PORT CONFLICTS:
 *    Both cores calling Serial.print() can interleave output.
 *    The Arduino-Pico framework has some internal protection,
 *    but for production code, use a mutex around serial writes.
 * 
 * 3. FLASH ACCESS:
 *    The RP2040 runs code from external SPI flash via XIP (execute-in-place).
 *    If one core writes to flash (OTA update, filesystem), the other core
 *    MUST be parked or running from RAM — flash is unavailable during erase/write!
 * 
 * 4. INTERRUPT ASSIGNMENT:
 *    Interrupts fire on the core that enabled them.
 *    Plan which core handles which interrupts.
 * 
 * ══════════════════════════════════════════════════════════
 * WHY DUAL-CORE MCUs ARE BECOMING COMMON
 * ══════════════════════════════════════════════════════════
 * 
 * Modern embedded systems need to do multiple things:
 *   - Run a control loop at fixed rate (motor PID, sensor fusion)
 *   - Handle communication (USB, WiFi, BLE, CAN)
 *   - Update display
 *   - Log data
 * 
 * On a single core, all of this competes for CPU time.
 * With dual core:
 *   Core 0: Time-critical control loop (deterministic)
 *   Core 1: Everything else (communication, display, logging)
 * 
 * The RP2040 at $0.70 makes dual-core cheaper than most single-core STM32s!
 */
