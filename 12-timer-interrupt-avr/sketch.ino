/*
 * LESSON 12: AVR — Timer Interrupts
 * Board: Arduino Uno (ATmega328P, 16MHz)
 * 
 * ============================================================
 * WHY TIMER INTERRUPTS INSTEAD OF delay() OR millis()?
 * ============================================================
 * 
 * delay() blocks the CPU — nothing else can run.
 * millis() is better (non-blocking), but it DRIFTS:
 *   - millis() itself uses Timer0 overflow interrupt
 *   - If your code takes variable time, your timing varies
 *   - Long ISRs can delay the millis() update
 * 
 * Hardware timer interrupts are EXACT:
 *   - The timer counts independently of CPU code
 *   - When it hits the compare value, it fires an interrupt
 *   - Jitter is only the interrupt latency (~4-8 cycles on AVR)
 *   - Perfect for: precise timing, audio sampling, control loops
 * 
 * ============================================================
 * ATmega328P TIMERS
 * ============================================================
 * 
 * Timer0 (8-bit):
 *   - Used by Arduino for millis(), micros(), delay()
 *   - DON'T TOUCH IT or Arduino timing breaks!
 *   - Connected to OC0A (pin 6) and OC0B (pin 5) for analogWrite()
 * 
 * Timer1 (16-bit):  ← WE USE THIS ONE
 *   - The most capable timer on ATmega328P
 *   - 16-bit counter (counts 0 to 65535)
 *   - Input capture (measure external pulse widths)
 *   - Connected to OC1A (pin 9) and OC1B (pin 10)
 *   - Free to use (Arduino doesn't depend on it*)
 *   *Servo library uses Timer1! Can't use both.
 * 
 * Timer2 (8-bit):
 *   - Can use external 32.768kHz crystal for RTC
 *   - Connected to OC2A (pin 11) and OC2B (pin 3)
 *   - Used by tone() function
 * 
 * ============================================================
 * TIMER MODES
 * ============================================================
 * 
 * 1. NORMAL MODE:
 *    Counter counts from 0 to MAX (255 or 65535), then overflows to 0.
 *    Overflow sets a flag / triggers interrupt.
 *    Simple but imprecise for timing (can't set an arbitrary period).
 * 
 * 2. CTC MODE (Clear Timer on Compare match):  ← WE USE THIS
 *    Counter counts from 0 up to OCRnA, then resets to 0.
 *    Fires interrupt when counter == OCRnA.
 *    PERFECT for periodic interrupts at exact intervals.
 *    
 *    Frequency = F_CPU / (prescaler * (OCRnA + 1))
 *    For 1Hz: OCR1A = 16,000,000 / (256 * 1) - 1 = 62499
 * 
 * 3. FAST PWM:
 *    Counter counts 0 to TOP (ICR1 or OCRnA).
 *    Output set at BOTTOM, cleared at compare match.
 *    Single-slope: higher frequency, but slightly asymmetric.
 *    Good for: LED dimming, motor speed control.
 * 
 * 4. PHASE CORRECT PWM:
 *    Counter counts UP to TOP, then DOWN to 0 (dual-slope).
 *    Output toggled on compare match in both directions.
 *    Half the frequency of Fast PWM, but symmetric.
 *    Good for: motor control (less audible whine).
 * 
 * ============================================================
 * TIMER1 REGISTERS
 * ============================================================
 * 
 * TCCR1A — Timer/Counter Control Register A
 *   Bits 7:6 = COM1A1:0 — Compare Match Output A mode
 *   Bits 5:4 = COM1B1:0 — Compare Match Output B mode
 *   Bits 1:0 = WGM11:10 — Waveform Generation Mode (lower 2 bits)
 * 
 * TCCR1B — Timer/Counter Control Register B
 *   Bit 7 = ICNC1 — Input Capture Noise Canceler
 *   Bit 6 = ICES1 — Input Capture Edge Select
 *   Bits 4:3 = WGM13:12 — Waveform Generation Mode (upper 2 bits)
 *   Bits 2:0 = CS12:10 — Clock Select (prescaler)
 *     000 = Timer stopped
 *     001 = No prescaling (F_CPU)
 *     010 = F_CPU/8
 *     011 = F_CPU/64
 *     100 = F_CPU/256    ← We use this
 *     101 = F_CPU/1024
 *     110 = External clock on T1, falling edge
 *     111 = External clock on T1, rising edge
 * 
 * TCNT1 — The actual 16-bit counter value (read/write)
 * 
 * OCR1A — Output Compare Register A (the compare value)
 * OCR1B — Output Compare Register B (second compare value)
 * 
 * ICR1 — Input Capture Register (stores counter value on external event)
 * 
 * TIMSK1 — Timer Interrupt Mask
 *   Bit 5 = ICIE1  — Input Capture Interrupt Enable
 *   Bit 2 = OCIE1B — Output Compare B Interrupt Enable
 *   Bit 1 = OCIE1A — Output Compare A Interrupt Enable
 *   Bit 0 = TOIE1  — Overflow Interrupt Enable
 * 
 * WGM modes for Timer1 (combined from TCCR1A and TCCR1B):
 *   WGM13:10 = 0100 → CTC, TOP = OCR1A    ← We use this
 *   WGM13:10 = 1110 → Fast PWM, TOP = ICR1
 *   WGM13:10 = 1010 → Phase Correct PWM, TOP = ICR1
 *   (There are 16 modes total — see datasheet Table 16-4)
 * 
 * ============================================================
 * THIS LESSON: Toggle LED at exactly 1Hz using Timer1 CTC
 * ============================================================
 */

const int LED_PIN = 13;  // Built-in LED on PB5
volatile bool ledState = false;
volatile uint32_t interruptCount = 0;

// Timer1 Compare Match A interrupt service routine
ISR(TIMER1_COMPA_vect) {
  // This fires EXACTLY every 0.5 seconds (2Hz interrupt → 1Hz toggle)
  ledState = !ledState;
  // Direct port manipulation in ISR for speed
  if (ledState) {
    PORTB |= (1 << 5);   // PB5 (pin 13) HIGH
  } else {
    PORTB &= ~(1 << 5);  // PB5 (pin 13) LOW
  }
  interruptCount++;
  
  /*
   * IMPORTANT ISR RULES:
   * 1. Keep ISRs SHORT — other interrupts are blocked while running
   * 2. Use 'volatile' for any variable shared between ISR and main code
   * 3. Don't use Serial.print() in ISR (it uses interrupts internally!)
   * 4. Don't use delay() in ISR (it depends on Timer0 interrupt)
   * 5. Avoid floating-point math (slow, uses lots of stack)
   * 6. Set a flag in ISR, do the work in loop() — common pattern
   */
}

void setup() {
  Serial.begin(115200);
  Serial.println("Lesson 12: AVR Timer Interrupts");
  Serial.println("LED toggles at exactly 1Hz via Timer1 CTC mode");
  Serial.println();
  
  // Configure LED pin as output (using direct register for consistency)
  DDRB |= (1 << 5);   // PB5 = output
  
  // ══ Configure Timer1 for CTC mode, 2Hz interrupt ══
  
  // Disable interrupts during configuration
  cli();  // Clear Interrupt flag (same as noInterrupts())
  
  // Reset Timer1 control registers
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;  // Reset counter
  
  // Set CTC mode (WGM12 bit in TCCR1B)
  // WGM13:10 = 0100 → CTC with OCR1A as TOP
  TCCR1B |= (1 << WGM12);
  
  // Set prescaler to 256
  // CS12:10 = 100
  TCCR1B |= (1 << CS12);
  
  // Calculate OCR1A for 2Hz interrupt (toggle at 2Hz = blink at 1Hz):
  // Timer clock = 16,000,000 / 256 = 62,500 Hz
  // For 2Hz: OCR1A = 62,500 / 2 - 1 = 31249
  OCR1A = 31249;
  
  // Enable Timer1 Compare Match A interrupt
  TIMSK1 |= (1 << OCIE1A);
  
  // Re-enable interrupts
  sei();  // Set Interrupt flag (same as interrupts())
  
  Serial.println("Timer1 configured:");
  Serial.print("  Prescaler: 256 → Timer clock: ");
  Serial.print(F_CPU / 256);
  Serial.println(" Hz");
  Serial.print("  OCR1A: ");
  Serial.print(OCR1A);
  Serial.print(" → Interrupt rate: ");
  Serial.print((float)F_CPU / 256 / (OCR1A + 1), 4);
  Serial.println(" Hz");
  Serial.println("  LED toggle rate: 1Hz (exact)");
  Serial.println();
  
  /*
   * COMPARISON: Timer interrupts across platforms
   * 
   * AVR: Direct register access (TCCR, OCR, TIMSK)
   *   + Simple, well-documented, predictable
   *   - Limited timers (3), some claimed by Arduino
   *   - 8-bit timers limit the range
   * 
   * STM32: HAL_TIM_Base_Start_IT() or direct TIM registers
   *   + Many timers (10+), 16/32-bit
   *   + Advanced features (DMA trigger, encoder, complementary)
   *   - Must enable timer clock via RCC first
   *   - More complex setup
   * 
   * ESP32: timerBegin(), timerAttachInterrupt(), timerAlarmWrite()
   *   + Simple Arduino-style API
   *   + 64-bit timers with 80MHz base clock
   *   - Only 4 hardware timers
   *   - FreeRTOS can introduce jitter
   * 
   * RP2040: Uses alarm pool or hardware timer peripheral
   *   + Single 64-bit timer with 4 alarm registers
   *   + Very precise (1µs resolution)
   *   - Different programming model than traditional timers
   */
}

void loop() {
  // The LED is toggled entirely by the ISR — the main loop is FREE!
  // This is the whole point: the CPU can do other work.
  
  static uint32_t lastPrint = 0;
  uint32_t now = millis();
  
  if (now - lastPrint >= 2000) {
    lastPrint = now;
    
    // Read shared variable with interrupts disabled (atomic access)
    cli();
    uint32_t count = interruptCount;
    sei();
    
    /*
     * WHY cli()/sei() around the read?
     * 
     * interruptCount is a uint32_t (4 bytes). On 8-bit AVR, reading it
     * takes 4 instructions. If the ISR fires between reading byte 2 and
     * byte 3, you get a TORN READ — half old value, half new value.
     * 
     * cli() disables interrupts → guaranteed atomic read.
     * sei() re-enables them.
     * 
     * For single-byte variables (uint8_t), this isn't needed — 
     * 8-bit reads are inherently atomic on 8-bit AVR.
     * 
     * On 32-bit platforms (STM32, ESP32, RP2040), reading a uint32_t
     * IS atomic (single instruction), so you don't need to disable
     * interrupts for 32-bit reads.
     */
    
    Serial.print("Interrupts fired: ");
    Serial.print(count);
    Serial.print(" | LED state: ");
    Serial.print(ledState ? "ON" : "OFF");
    Serial.print(" | Expected toggles: ");
    Serial.println((now / 500));
  }
  
  // You could do other work here — read sensors, run PID control, etc.
  // The timing-critical LED blink happens in hardware, not in this loop.
}
