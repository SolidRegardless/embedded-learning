/*
 * LESSON 11: AVR — Direct Port Manipulation
 * Board: Arduino Uno (ATmega328P, 16MHz, 32KB Flash, 2KB RAM)
 * 
 * ============================================================
 * WHY DIRECT PORT MANIPULATION?
 * ============================================================
 * 
 * Arduino's digitalWrite() is SLOW. On ATmega328P:
 *   - digitalWrite(): ~60 clock cycles (~3.75µs at 16MHz)
 *   - Direct port write: 1-2 clock cycles (~62-125ns)
 *   - That's 30-60x FASTER!
 * 
 * Why is digitalWrite() so slow?
 *   1. Looks up the pin in a table (progmem read)
 *   2. Checks if the pin has a PWM timer attached (turns it off)
 *   3. Disables interrupts (to make the read-modify-write atomic)
 *   4. Reads the port register
 *   5. Modifies the bit
 *   6. Writes the port register
 *   7. Re-enables interrupts
 * 
 * Direct port access skips ALL of that — you write the register directly.
 * The compiler turns it into a single SBI/CBI instruction (Set/Clear Bit in I/O).
 * 
 * ============================================================
 * THE AVR REGISTER MODEL
 * ============================================================
 * 
 * Each I/O port (B, C, D) has exactly 3 registers:
 * 
 *   DDRx  — Data Direction Register
 *           Bit = 0 → Input
 *           Bit = 1 → Output
 *           Example: DDRD = 0b11110000 → PD7-PD4 output, PD3-PD0 input
 * 
 *   PORTx — Output/Pull-up Register
 *           When pin is OUTPUT: 0 = LOW, 1 = HIGH
 *           When pin is INPUT:  0 = floating, 1 = internal pull-up enabled
 *           (Dual purpose! This confuses beginners.)
 * 
 *   PINx  — Input Register (read-only... mostly)
 *           Reading PINx gives the current state of the pins
 *           TRICK: Writing 1 to PINx TOGGLES the corresponding pin!
 *           (This is an AVR-specific feature — not in the original spec)
 * 
 * ============================================================
 * PORT MAPPING ON ATMEGA328P
 * ============================================================
 * 
 *   PORTD: Arduino pins 0-7
 *     PD0 = pin 0 (RX — serial receive, avoid using!)
 *     PD1 = pin 1 (TX — serial transmit, avoid using!)
 *     PD2 = pin 2
 *     PD3 = pin 3  (OC2B — Timer2 PWM)
 *     PD4 = pin 4
 *     PD5 = pin 5  (OC0B — Timer0 PWM)
 *     PD6 = pin 6  (OC0A — Timer0 PWM)
 *     PD7 = pin 7
 * 
 *   PORTB: Arduino pins 8-13
 *     PB0 = pin 8
 *     PB1 = pin 9  (OC1A — Timer1 PWM)
 *     PB2 = pin 10 (OC1B — Timer1 PWM, also SPI SS)
 *     PB3 = pin 11 (OC2A — Timer2 PWM, also SPI MOSI)
 *     PB4 = pin 12 (SPI MISO)
 *     PB5 = pin 13 (SPI SCK, built-in LED)
 *     PB6, PB7 = crystal oscillator (not available)
 * 
 *   PORTC: Arduino A0-A5
 *     PC0 = A0 (also digital pin 14)
 *     PC1 = A1
 *     PC2 = A2
 *     PC3 = A3
 *     PC4 = A4 (SDA — I2C data)
 *     PC5 = A5 (SCL — I2C clock)
 *     PC6 = RESET (not available as I/O)
 * 
 * ============================================================
 * THIS LESSON: Blink 4 LEDs on PORTD using bitwise operations
 * LEDs on pins 4, 5, 6, 7 (PD4-PD7)
 * ============================================================
 */

void setup() {
  Serial.begin(115200);
  Serial.println("Lesson 11: AVR Direct Port Manipulation");
  
  // ── Arduino way (what we're replacing) ──
  // pinMode(4, OUTPUT);
  // pinMode(5, OUTPUT);
  // pinMode(6, OUTPUT);
  // pinMode(7, OUTPUT);
  
  // ── Direct register way ──
  // Set PD4-PD7 as outputs using DDRD
  // We must NOT touch PD0-PD1 (serial!) so we use |= to only set our bits
  DDRD |= 0b11110000;  // Set bits 7,6,5,4 = output; leave 3,2,1,0 unchanged
  
  // Start with all LEDs off
  PORTD &= 0b00001111;  // Clear bits 7,6,5,4; leave 3,2,1,0 unchanged
  
  /*
   * IMPORTANT: Why |= and &= instead of =?
   * 
   * If we wrote: DDRD = 0b11110000;
   * We'd set PD0-PD3 as INPUTS, breaking serial (PD0=RX, PD1=TX)!
   * 
   * Always use bitwise OR (|=) to SET bits and AND (&=) to CLEAR bits
   * when other bits in the register are in use.
   * 
   * This is a read-modify-write operation:
   *   1. Read DDRD
   *   2. OR with mask
   *   3. Write back
   * 
   * On AVR, the compiler optimizes DDRD |= (1<<4) into a single SBI
   * instruction — but only if the bit position is a compile-time constant
   * and the register is in the I/O space (address 0x00-0x1F).
   */
  
  Serial.println("Setup complete. PD4-PD7 configured as outputs.");
}

void loop() {
  // ── Pattern 1: Sequential on ──
  Serial.println("Pattern: Sequential");
  for (int i = 4; i <= 7; i++) {
    PORTD |= (1 << i);   // Set bit i HIGH
    delay(200);
  }
  delay(500);
  
  // All off at once — set all 4 bits LOW in ONE instruction
  PORTD &= ~(0b11110000);  // ~0b11110000 = 0b00001111
  delay(300);
  
  // ── Pattern 2: All on, all off (one instruction!) ──
  Serial.println("Pattern: Flash all");
  for (int j = 0; j < 5; j++) {
    PORTD |= 0b11110000;   // All 4 LEDs ON — single write!
    delay(100);
    PORTD &= 0b00001111;   // All 4 LEDs OFF — single write!
    delay(100);
  }
  
  /*
   * Compare to Arduino way:
   *   digitalWrite(4, HIGH);  // ~60 cycles
   *   digitalWrite(5, HIGH);  // ~60 cycles
   *   digitalWrite(6, HIGH);  // ~60 cycles
   *   digitalWrite(7, HIGH);  // ~60 cycles
   *   Total: ~240 cycles, 4 separate operations
   * 
   * Direct port way:
   *   PORTD |= 0b11110000;   // ~2 cycles, ALL pins change simultaneously!
   * 
   * The simultaneous change matters for things like:
   *   - Driving parallel buses (8-bit data bus)
   *   - Bit-banging protocols where timing is critical
   *   - Multiplexed LED displays (all segments must change at once)
   */
  
  // ── Pattern 3: Toggle using PINx write trick ──
  Serial.println("Pattern: PINx toggle trick");
  PORTD |= 0b11110000;  // Start with all ON
  for (int j = 0; j < 10; j++) {
    // Writing 1 to PINx TOGGLES the pin — AVR magic!
    PIND = 0b11110000;   // Toggle PD4-PD7 — no read-modify-write needed!
    delay(150);
  }
  
  /*
   * The PINx toggle trick:
   *   - Normally PINx is read-only (you read input state)
   *   - BUT on ATmega328P (and most modern AVR), writing 1 toggles the output
   *   - This is ATOMIC — no read-modify-write, no interrupt race condition
   *   - Similar to STM32's BSRR or ESP32's W1TS/W1TC registers
   *   - Single clock cycle!
   */
  
  // ── Pattern 4: Binary counter ──
  Serial.println("Pattern: Binary counter on PD4-PD7");
  for (int count = 0; count < 16; count++) {
    // We want to put 'count' onto PD7:PD4
    // Clear PD7-PD4, then set our value shifted to the right position
    PORTD = (PORTD & 0b00001111) | (count << 4);
    
    Serial.print("  Count: ");
    Serial.print(count);
    Serial.print(" Binary: ");
    Serial.println(count, BIN);
    delay(400);
  }
  
  PORTD &= 0b00001111;  // All off
  delay(1000);
  
  /*
   * ══════════════════════════════════════════════════════════
   * COMPARISON: GPIO ACROSS PLATFORMS
   * ══════════════════════════════════════════════════════════
   * 
   * AVR:
   *   - 3 registers per port (DDR, PORT, PIN) — simple and elegant
   *   - 8 pins per port, direct bit manipulation
   *   - Single-cycle I/O for constant-address constant-bit operations
   *   - No clock gating needed
   * 
   * STM32:
   *   - 7+ registers per port (MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR...)
   *   - Must enable clock first (RCC)
   *   - BSRR for atomic set/clear
   *   - 2 bits per pin for mode (input/output/AF/analog)
   * 
   * ESP32:
   *   - GPIO matrix — any peripheral can map to (almost) any pin
   *   - Registers spread across memory-mapped space
   *   - W1TS/W1TC for atomic set/clear
   *   - More pins (34+) but some are input-only
   * 
   * RP2040:
   *   - SIO (Single-cycle I/O) port for fast GPIO
   *   - GPIO_SET, GPIO_CLR, GPIO_XOR registers (like BSRR)
   *   - PIO can control GPIO at system clock speed independently
   */
}
