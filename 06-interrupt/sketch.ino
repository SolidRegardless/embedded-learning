/*
 * LESSON 6: Hardware Interrupts & the volatile Keyword
 * 
 * CONCEPTS:
 * - Hardware interrupts: the CPU drops everything to handle an event
 * - ISR (Interrupt Service Routine): the function that runs on interrupt
 * - volatile: tells the compiler "this variable changes outside normal flow"
 * - Critical sections: protecting shared data between ISR and main code
 * - IRAM_ATTR: ISR code must live in RAM (not flash) on ESP32
 * 
 * WHAT IS AN INTERRUPT?
 * In lessons 1-5, we used polling: checking a pin's state every loop().
 * Problem: if something happens between checks, you miss it.
 * 
 * Interrupts flip this around. You tell the hardware: "when THIS pin 
 * changes, STOP whatever you're doing and run THIS function immediately."
 * 
 * It's like the difference between:
 *   - Polling: checking your phone every 5 minutes for messages
 *   - Interrupt: your phone buzzes when a message arrives
 * 
 * WHY volatile?
 * The compiler optimizes code. If it sees a variable that only gets 
 * read in loop() and never written there, it might cache the value 
 * in a CPU register and never re-read it from RAM.
 * 
 * But ISRs modify variables OUTSIDE the normal program flow. The 
 * compiler doesn't know about them. 'volatile' forces the compiler 
 * to always read from RAM, never cache.
 * 
 *   Without volatile: compiler reads pressCount once, caches it forever
 *   With volatile: compiler reads pressCount from RAM every time
 */

const int BUTTON_PIN = 4;
const int LED_PIN = 2;

// VOLATILE: this variable is modified inside an ISR
// Without volatile, the compiler might optimize away reads in loop()
volatile int pressCount = 0;
volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;

// ISR — Interrupt Service Routine
// IRAM_ATTR: on ESP32, ISR code must be in IRAM (not flash/PSRAM)
// because flash access is too slow and might be busy during interrupt
void IRAM_ATTR handleButtonPress() {
  // Debounce inside ISR: ignore if < 200ms since last interrupt
  unsigned long now = millis();
  if (now - lastInterruptTime > 200) {
    lastInterruptTime = now;
    pressCount++;
    buttonPressed = true;
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // ATTACH THE INTERRUPT
  // FALLING = trigger when pin goes from HIGH to LOW (button press)
  // Other modes: RISING, CHANGE, LOW, HIGH
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);
  
  Serial.println("========================================");
  Serial.println("  Lesson 6: Hardware Interrupts");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Press the button — the interrupt catches it INSTANTLY");
  Serial.println("Even while the main loop is doing other work!");
  Serial.println();
}

void loop() {
  // Simulate "busy work" — in real firmware this could be 
  // complex calculations, communication, etc.
  // The interrupt still fires even during this delay!
  static unsigned long lastWork = 0;
  if (millis() - lastWork > 100) {
    lastWork = millis();
    // Pretend we're doing something CPU-intensive
    volatile int dummy = 0;
    for (int i = 0; i < 10000; i++) {
      dummy += i;
    }
  }
  
  // Check the flag set by ISR
  if (buttonPressed) {
    // CRITICAL SECTION: temporarily disable interrupts while 
    // reading shared variables to prevent data corruption
    noInterrupts();  // portDISABLE_INTERRUPTS on ESP32
    int count = pressCount;
    buttonPressed = false;
    interrupts();    // portENABLE_INTERRUPTS
    
    // Toggle LED
    static bool ledState = false;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    
    Serial.print("INTERRUPT! Press count: ");
    Serial.print(count);
    Serial.print(" | LED: ");
    Serial.println(ledState ? "ON" : "OFF");
  }
}

/*
 * ISR RULES — CRITICAL FOR EMBEDDED:
 * 
 * 1. KEEP ISRs SHORT: No Serial.print, no delay(), no I2C/SPI.
 *    Set a flag, increment a counter, that's it. Do heavy work in loop().
 * 
 * 2. NO HEAP ALLOCATION: No new, no malloc, no String operations.
 *    ISRs must be deterministic and fast.
 * 
 * 3. VOLATILE ALL SHARED VARIABLES: Anything read in loop() and 
 *    written in ISR (or vice versa) must be volatile.
 * 
 * 4. ATOMIC ACCESS: On 32-bit MCUs, reading a 32-bit int is atomic.
 *    Reading a 64-bit value or a struct is NOT — use noInterrupts()/
 *    interrupts() to protect multi-byte reads.
 * 
 * 5. IRAM_ATTR (ESP32-specific): ISR code and any function it calls 
 *    must be in IRAM. Flash might be mid-operation when interrupt fires.
 * 
 * THIS IS THE BIGGEST DIFFERENCE FROM DESKTOP C++:
 * On desktop, the OS handles interrupts. You never think about them.
 * In embedded, YOU manage interrupts, YOU protect shared state,
 * YOU ensure your ISR doesn't take too long and starve other interrupts.
 * It's real concurrent programming with no OS safety net.
 */
