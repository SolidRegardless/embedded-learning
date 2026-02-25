/*
 * LESSON 14: AVR — Watchdog Timer
 * Board: Arduino Uno (ATmega328P, 16MHz)
 * 
 * ============================================================
 * WHAT IS A WATCHDOG TIMER (WDT)?
 * ============================================================
 * 
 * A watchdog is a countdown timer that RESETS THE ENTIRE CHIP if
 * your code doesn't periodically "pet" (reset) it.
 * 
 * Think of it as a dead man's switch:
 *   - Your code says "I'm still alive!" every N milliseconds
 *   - If the code crashes, hangs, or enters an infinite loop...
 *   - ...it stops petting the watchdog
 *   - Watchdog expires → HARD RESET → code starts from scratch
 * 
 * ============================================================
 * WHY WATCHDOG TIMERS ARE CRITICAL
 * ============================================================
 * 
 * Hobby projects: "It crashed? I'll just press reset."
 * Production devices: THERE IS NO ONE TO PRESS RESET.
 * 
 * Examples where watchdog is mandatory:
 * 
 *   🏭 Industrial controllers:
 *      A PLC controlling a chemical plant hangs → valves stuck open
 *      → overpressure → explosion. WDT resets and returns to safe state.
 * 
 *   🚗 Automotive ECUs:
 *      Engine control unit crashes → car stalls on highway.
 *      WDT resets within milliseconds. ISO 26262 REQUIRES it.
 *      Some automotive MCUs have MULTIPLE independent watchdogs.
 * 
 *   🏥 Medical devices:
 *      Insulin pump firmware hangs → no insulin delivery → patient dies.
 *      IEC 62304 mandates defensive coding including WDT.
 * 
 *   🛰️ Space/Remote:
 *      Satellite firmware crashes → can't send someone up to reboot it.
 *      WDT with progressive recovery (soft reset → hard reset → safe mode).
 * 
 *   📡 IoT sensors:
 *      Remote weather station in Antarctica. Battery-powered.
 *      If WiFi connection code hangs → WDT resets → try again.
 * 
 * ============================================================
 * ATmega328P WATCHDOG CONFIGURATION
 * ============================================================
 * 
 * The WDT uses an independent 128kHz internal oscillator.
 * It runs even if the main clock fails!
 * 
 * Timeout periods (WDTCSR register, WDP bits):
 *   WDP[3:0] | Timeout
 *   ─────────|──────────
 *   0000     | 16ms
 *   0001     | 32ms
 *   0010     | 64ms
 *   0011     | 125ms
 *   0100     | 250ms
 *   0101     | 500ms
 *   0110     | 1s
 *   0111     | 2s
 *   1000     | 4s
 *   1001     | 8s      ← maximum on ATmega328P
 * 
 * WDTCSR — Watchdog Timer Control and Status Register:
 *   Bit 7: WDIF  — Interrupt flag
 *   Bit 6: WDIE  — Interrupt enable (ISR instead of reset)
 *   Bit 4: WDCE  — Change enable (must be set to modify WDE/prescaler)
 *   Bit 3: WDE   — Watchdog enable (1 = reset mode)
 *   Bit 5,2:0: WDP[3:0] — Prescaler (timeout period)
 * 
 * IMPORTANT: Changing WDTCSR requires a timed sequence:
 *   1. Write WDCE and WDE = 1 simultaneously
 *   2. Within 4 clock cycles, write new configuration
 *   This prevents accidental WDT changes from runaway code!
 * 
 * ============================================================
 * WDT MODES
 * ============================================================
 * 
 * 1. System Reset mode (WDE=1, WDIE=0):
 *    Timeout → chip resets. Most common for safety.
 * 
 * 2. Interrupt mode (WDE=0, WDIE=1):
 *    Timeout → fires an ISR. Used for waking from sleep mode.
 *    After the ISR, WDT is automatically disabled.
 * 
 * 3. Interrupt + Reset mode (WDE=1, WDIE=1):
 *    First timeout → interrupt. Second timeout → reset.
 *    Gives the ISR a chance to save state before reset.
 * 
 * ============================================================
 * THIS LESSON: WDT resets MCU when button simulates a hang
 * ============================================================
 */

#include <avr/wdt.h>

const int BUTTON_PIN = 2;
const int LED_PIN = 13;
const int STATUS_LED = 7;  // Shows normal operation

volatile bool simulateHang = false;

void setup() {
  // IMPORTANT: Disable WDT as first thing in setup!
  // After a WDT reset, the WDT stays enabled with the shortest timeout.
  // If setup() takes too long, it would reset again → boot loop!
  wdt_disable();
  
  /*
   * wdt_disable() does:
   *   cli();
   *   wdt_reset();                // Reset the timer
   *   MCUSR &= ~(1 << WDRF);    // Clear WDT reset flag
   *   // Timed sequence to disable:
   *   WDTCSR |= (1 << WDCE) | (1 << WDE);  // Enable changes
   *   WDTCSR = 0;                            // Disable everything
   *   sei();
   */
  
  Serial.begin(115200);
  
  // Check WHY we booted
  Serial.println();
  Serial.println("Lesson 14: AVR Watchdog Timer");
  Serial.println("─────────────────────────────");
  
  // MCUSR tells us the reset source
  // (Note: wdt_disable clears WDRF, so we check a saved copy)
  // In production code, you'd save MCUSR at the very start of setup()
  Serial.print("Reset reason: ");
  /*
   * MCUSR bits:
   *   Bit 0: PORF  — Power-on Reset
   *   Bit 1: EXTRF — External Reset (reset button)
   *   Bit 2: BORF  — Brown-out Reset (voltage dipped)
   *   Bit 3: WDRF  — Watchdog Reset
   * 
   * In production, you'd log this to EEPROM to track field reliability.
   * Frequent watchdog resets = firmware bug to investigate!
   */
  Serial.println("(check MCUSR at very start of main() for accurate source)");
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  
  Serial.println("Normal operation: LED heartbeat + serial output");
  Serial.println("Press button: simulates a firmware hang");
  Serial.println("Watchdog will reset the MCU after 2 seconds!");
  Serial.println();
  
  // ── Enable the Watchdog Timer ──
  // 2 second timeout, system reset mode
  wdt_enable(WDTO_2S);
  
  /*
   * wdt_enable(WDTO_2S) does:
   *   cli();
   *   wdt_reset();
   *   // Timed sequence:
   *   WDTCSR |= (1 << WDCE) | (1 << WDE);
   *   WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
   *   //        ^enable      ^──────── 2s timeout ────────────────^
   *   sei();
   * 
   * Available timeout constants:
   *   WDTO_15MS, WDTO_30MS, WDTO_60MS, WDTO_120MS, WDTO_250MS,
   *   WDTO_500MS, WDTO_1S, WDTO_2S, WDTO_4S, WDTO_8S
   */
  
  Serial.println("Watchdog enabled: 2 second timeout");
  Serial.println("If we don't call wdt_reset() within 2s → HARD RESET");
}

void loop() {
  // Check for button press (simulated hang)
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println();
    Serial.println("!!! BUTTON PRESSED — SIMULATING FIRMWARE HANG !!!");
    Serial.println("!!! Code is stuck in infinite loop...         !!!");
    Serial.println("!!! Watchdog will reset in ~2 seconds...      !!!");
    Serial.println();
    
    // Turn on LED to show we're "hung"
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(STATUS_LED, LOW);
    
    // SIMULATE A HANG — infinite loop!
    // In real life this could be: deadlocked mutex, infinite retry loop,
    // corrupted function pointer, stack overflow, hardware lockup
    while (1) {
      // We're stuck here. Not calling wdt_reset().
      // The watchdog counter is still ticking...
      // In ~2 seconds: BOOM → reset!
      
      // You can see the LED stay on, then suddenly the board restarts.
    }
    // This line is NEVER reached.
  }
  
  // ── Normal operation: pet the watchdog ──
  wdt_reset();  // "I'm still alive!" — resets the countdown
  
  /*
   * wdt_reset() compiles to a single instruction: WDR
   * It resets the watchdog counter to zero.
   * 
   * BEST PRACTICE: Only call wdt_reset() from ONE place in your code.
   * If you sprinkle it everywhere, a partial hang might still pet the WDT.
   * 
   * Pattern for complex firmware:
   *   - Each task/module sets a "I'm healthy" flag each iteration
   *   - One supervisor function checks ALL flags
   *   - Only if ALL tasks are healthy → wdt_reset()
   *   - This catches partial hangs (one task stuck, others running)
   */
  
  // Heartbeat LED
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 250) {
    lastBlink = millis();
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }
  
  // Show we're alive
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    Serial.print("Running OK | Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println("s | WDT: fed ✓");
  }
  
  /*
   * ══════════════════════════════════════════════════════════
   * WATCHDOG ON OTHER PLATFORMS
   * ══════════════════════════════════════════════════════════
   * 
   * STM32:
   *   Two independent watchdogs!
   *   - IWDG (Independent WDT): Clocked from LSI (independent clock)
   *     Cannot be disabled once started! (hardware safety feature)
   *     IWDG->KR = 0x5555;  // Enable register access
   *     IWDG->PR = 4;       // Prescaler
   *     IWDG->RLR = 625;    // Reload value
   *     IWDG->KR = 0xCCCC;  // Start watchdog
   *     IWDG->KR = 0xAAAA;  // Refresh (pet the dog)
   *   
   *   - WWDG (Window WDT): Must be refreshed within a TIME WINDOW
   *     Too early = reset. Too late = reset. 
   *     Catches both hangs AND runaway code that loops too fast!
   * 
   * ESP32:
   *   - Task WDT: Monitors FreeRTOS tasks (esp_task_wdt_add())
   *   - Interrupt WDT: Resets if interrupts are disabled too long
   *   - RTC WDT: Ultra-low-power watchdog
   *   - More complex because of FreeRTOS + dual core
   * 
   * RP2040:
   *   - Single watchdog timer
   *   - Can trigger a soft reset or hard reset
   *   - Watchdog_enable(timeout_ms, pause_on_debug)
   *   - Scratch registers survive watchdog reset (pass data across resets!)
   */
}
