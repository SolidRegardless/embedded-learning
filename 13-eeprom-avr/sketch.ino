/*
 * LESSON 13: AVR — EEPROM Persistence
 * Board: Arduino Uno (ATmega328P, 16MHz)
 * 
 * ============================================================
 * NON-VOLATILE STORAGE ON MICROCONTROLLERS
 * ============================================================
 * 
 * RAM (SRAM): 2KB on ATmega328P
 *   - Fast read/write (single clock cycle)
 *   - Lost on power-off (volatile)
 *   - Used for variables, stack, heap
 * 
 * Flash: 32KB on ATmega328P
 *   - Stores your program code
 *   - Non-volatile (survives power cycles)
 *   - ~10,000 write cycles per page
 *   - Can store constants with PROGMEM
 *   - Self-programming possible but tricky (bootloader territory)
 * 
 * EEPROM: 1KB on ATmega328P  ← THIS LESSON
 *   - Non-volatile (survives power cycles!)
 *   - ~100,000 write cycles per byte
 *   - Byte-addressable (write individual bytes)
 *   - Slow to write (~3.3ms per byte)
 *   - Fast to read (~1 clock cycle with proper setup)
 *   - Perfect for: settings, calibration data, boot counters
 * 
 * ============================================================
 * EEPROM WEAR: THE SILENT KILLER
 * ============================================================
 * 
 * Each EEPROM byte can be written ~100,000 times before it degrades.
 * Sounds like a lot? Let's do the math:
 * 
 *   Writing once per second:
 *   100,000 / 3600 = ~27.8 hours until failure!
 * 
 *   Writing once per minute:
 *   100,000 / 60 / 24 = ~69 days
 * 
 *   Writing once per hour:
 *   100,000 / 24 / 365 = ~11.4 years (more reasonable!)
 * 
 * Rules for EEPROM use:
 *   1. ONLY write when the value CHANGES (check before writing!)
 *   2. Don't write in a loop — accumulate changes, write periodically
 *   3. For frequently-changing data, use wear leveling (see below)
 *   4. Consider saving on power-down using a brown-out detector
 * 
 * ============================================================
 * WEAR LEVELING
 * ============================================================
 * 
 * Instead of always writing to address 0, spread writes across
 * multiple addresses. When address N is "worn out" (or after M writes),
 * move to address N+1.
 * 
 * Simple approach (circular buffer):
 *   - Reserve 10 EEPROM bytes for one value
 *   - Each write goes to the next address
 *   - After 10 addresses, wrap around
 *   - 100,000 × 10 = 1,000,000 total write cycles!
 * 
 * We demonstrate a simple version below.
 * 
 * ============================================================
 * EEPROM ON OTHER PLATFORMS
 * ============================================================
 * 
 * STM32: Most have NO EEPROM. Use flash emulation (write to a flash
 *   page, manage wear in software). STM32L4 has a small "EEPROM-like"
 *   flash area. ST provides a library for flash-based EEPROM emulation.
 * 
 * ESP32: No EEPROM. The EEPROM library is fake — it uses a flash
 *   partition. The Preferences library (NVS) is better: it does
 *   wear leveling automatically using a log-structured filesystem.
 * 
 * RP2040: No EEPROM. Uses flash (last sector). Same situation as ESP32.
 * 
 * AVR: Real, dedicated EEPROM hardware. One of AVR's advantages!
 *   The hardware handles the write timing internally.
 * 
 * ============================================================
 * THIS LESSON: Boot counter + button-press counter with EEPROM
 * ============================================================
 */

#include <EEPROM.h>

// EEPROM addresses
const int ADDR_BOOT_COUNT = 0;       // 4 bytes (uint32_t)
const int ADDR_BUTTON_COUNT = 4;     // 4 bytes (uint32_t)
const int ADDR_WEAR_LEVEL_START = 8; // Wear-leveled area starts here

// Wear leveling for a frequently-updated value
const int WEAR_SLOTS = 10;           // 10 slots × 2 bytes each = 20 bytes
const int ADDR_WEAR_INDEX = 28;      // Which slot we're currently using

const int BUTTON_PIN = 2;  // Button on pin 2

uint32_t bootCount = 0;
uint32_t buttonCount = 0;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Lesson 13: AVR EEPROM Persistence");
  Serial.println();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  
  // ── Read boot counter from EEPROM ──
  EEPROM.get(ADDR_BOOT_COUNT, bootCount);
  
  /*
   * EEPROM.get() reads multiple bytes into a variable.
   * Under the hood for uint32_t, it reads 4 bytes:
   *   byte0 = EEPROM.read(0);
   *   byte1 = EEPROM.read(1);
   *   byte2 = EEPROM.read(2);
   *   byte3 = EEPROM.read(3);
   * 
   * On first boot (new chip), EEPROM contains 0xFF in every byte.
   * So bootCount would read as 0xFFFFFFFF = 4,294,967,295!
   * We detect this and initialize to 0.
   */
  
  if (bootCount == 0xFFFFFFFF) {
    // First boot ever — EEPROM is fresh (all 0xFF)
    bootCount = 0;
    buttonCount = 0;
    Serial.println("*** FIRST BOOT — Initializing EEPROM ***");
  } else {
    EEPROM.get(ADDR_BUTTON_COUNT, buttonCount);
  }
  
  // Increment and save boot counter
  bootCount++;
  EEPROM.put(ADDR_BOOT_COUNT, bootCount);
  
  /*
   * EEPROM.put() is SMART — it reads the current value first and
   * ONLY writes bytes that have changed. This is called "write-if-different"
   * and it reduces wear automatically.
   * 
   * EEPROM.write() always writes, even if the value is the same.
   * ALWAYS prefer EEPROM.put() and EEPROM.update() over EEPROM.write()!
   * 
   * EEPROM.update(addr, byte) = write only if different (single byte)
   * EEPROM.put(addr, data)    = update for any data type
   */
  
  Serial.print("Boot count: ");
  Serial.println(bootCount);
  Serial.print("Button presses (from last session): ");
  Serial.println(buttonCount);
  Serial.println();
  
  // ── Show EEPROM contents ──
  Serial.println("EEPROM first 32 bytes:");
  for (int i = 0; i < 32; i++) {
    byte val = EEPROM.read(i);
    if (val < 16) Serial.print("0");
    Serial.print(val, HEX);
    Serial.print(" ");
    if ((i + 1) % 16 == 0) Serial.println();
  }
  Serial.println();
  
  /*
   * ══════════════════════════════════════════════════════════
   * RAW REGISTER ACCESS (what EEPROM library does):
   * ══════════════════════════════════════════════════════════
   * 
   * Reading a byte from EEPROM:
   *   while (EECR & (1 << EEPE));     // Wait for previous write to finish
   *   EEAR = address;                   // Set address register
   *   EECR |= (1 << EERE);            // Trigger read
   *   return EEDR;                      // Read data register
   * 
   * Writing a byte to EEPROM:
   *   while (EECR & (1 << EEPE));     // Wait for previous write
   *   EEAR = address;                   // Set address
   *   EEDR = data;                      // Set data
   *   cli();                            // Disable interrupts (CRITICAL!)
   *   EECR |= (1 << EEMPE);           // Master write enable
   *   EECR |= (1 << EEPE);            // Start write (must be within 4 cycles of EEMPE!)
   *   sei();                            // Re-enable interrupts
   *   // Write takes ~3.3ms — CPU continues, just can't access EEPROM
   * 
   * The timed write sequence (EEMPE then EEPE within 4 cycles) is a
   * SAFETY FEATURE — prevents accidental writes from code bugs or
   * brown-out conditions corrupting your saved data.
   */
  
  Serial.println("Press the button to increment the counter.");
  Serial.println("Reset the board to see boot count increase.");
  Serial.println("EEPROM data survives power cycles!");
}

void loop() {
  // ── Debounced button reading ──
  bool reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 50) {
    static bool buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) {  // Button pressed (active LOW with pull-up)
        buttonCount++;
        
        // Save to EEPROM — but check first!
        EEPROM.put(ADDR_BUTTON_COUNT, buttonCount);
        
        // Visual feedback
        digitalWrite(LED_BUILTIN, HIGH);
        
        Serial.print("Button pressed! Count: ");
        Serial.println(buttonCount);
        
        // ── Demonstrate wear-leveled write ──
        wearLeveledWrite((uint16_t)(buttonCount & 0xFFFF));
      } else {
        digitalWrite(LED_BUILTIN, LOW);
      }
    }
  }
  
  lastButtonState = reading;
}

/*
 * ══════════════════════════════════════════════════════════
 * SIMPLE WEAR LEVELING IMPLEMENTATION
 * ══════════════════════════════════════════════════════════
 * 
 * We have WEAR_SLOTS positions, each holding a uint16_t.
 * We rotate through them round-robin.
 * This gives us WEAR_SLOTS × 100,000 = 1,000,000 write cycles.
 */
void wearLeveledWrite(uint16_t value) {
  // Read current slot index
  byte slotIndex = EEPROM.read(ADDR_WEAR_INDEX);
  if (slotIndex >= WEAR_SLOTS || slotIndex == 0xFF) {
    slotIndex = 0;  // Initialize or wrap around
  }
  
  // Calculate address for this slot
  int slotAddr = ADDR_WEAR_LEVEL_START + (slotIndex * 2);  // 2 bytes per slot
  
  // Write value to current slot
  EEPROM.put(slotAddr, value);
  
  // Advance to next slot
  slotIndex = (slotIndex + 1) % WEAR_SLOTS;
  EEPROM.update(ADDR_WEAR_INDEX, slotIndex);
  
  /*
   * MORE ADVANCED WEAR LEVELING APPROACHES:
   * 
   * 1. Sequence-number based: Each slot has a sequence number.
   *    On boot, find the highest sequence number = newest data.
   *    No separate index byte needed (it's implicit).
   * 
   * 2. Log-structured: Write-ahead log, compact when full.
   *    This is what ESP32's NVS (Non-Volatile Storage) does.
   *    Much more complex but handles arbitrary key-value pairs.
   * 
   * 3. EEPROM emulation in flash (STM32): Use two flash pages.
   *    Write new entries to current page. When full, compact valid
   *    entries to the other page, erase the first. ST provides an
   *    application note (AN2594/AN4894) and library for this.
   */
}
