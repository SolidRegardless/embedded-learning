/*
 * LESSON 2: Button + LED — GPIO Input
 * 
 * CONCEPTS:
 * - GPIO input: reading the state of a physical pin
 * - Pull-up resistors: why floating pins are dangerous
 * - Debouncing: mechanical switches bounce (rapidly toggle) for ~10-50ms
 * - Active LOW: button pressed = LOW (because pull-up connects to 3.3V)
 * 
 * WHAT'S A PULL-UP RESISTOR?
 * When a switch is OPEN, the GPIO pin is "floating" — not connected to 
 * anything. It picks up electrical noise and reads random HIGH/LOW values.
 * A pull-up resistor weakly connects the pin to 3.3V, so:
 *   - Switch open → pin reads HIGH (pulled up to 3.3V)
 *   - Switch closed → pin reads LOW (connected directly to GND, overpowers the weak pull-up)
 * 
 * ESP32 has INTERNAL pull-ups, so we don't need an external resistor.
 * That's what INPUT_PULLUP does.
 * 
 * DEBOUNCING:
 * When you press a mechanical button, the metal contacts physically 
 * bounce for a few milliseconds. The MCU is fast enough to see each 
 * bounce as a separate press. We add a small delay to ignore the bounces.
 */

const int BUTTON_PIN = 4;   // GPIO4 — button input
const int LED_PIN = 2;      // GPIO2 — LED output

// Debounce state
bool lastButtonState = HIGH;       // Pull-up means unpressed = HIGH
bool ledState = LOW;               // LED starts off
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 50;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Enable internal pull-up resistor
  
  Serial.begin(115200);
  Serial.println("Lesson 2: Button + LED");
  Serial.println("Press the button to toggle the LED");
}

void loop() {
  bool reading = digitalRead(BUTTON_PIN);
  
  // If the reading changed, reset the debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // Only act if the state has been stable for DEBOUNCE_MS
  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    // Detect the transition from HIGH to LOW (button press)
    static bool stableState = HIGH;
    if (reading != stableState) {
      stableState = reading;
      
      if (stableState == LOW) {  // Button pressed (active LOW)
        ledState = !ledState;    // Toggle
        digitalWrite(LED_PIN, ledState);
        Serial.print("Button pressed! LED is now: ");
        Serial.println(ledState ? "ON" : "OFF");
      }
    }
  }
  
  lastButtonState = reading;
}

/*
 * KEY EMBEDDED CONCEPT: millis() vs delay()
 * 
 * delay(1000) BLOCKS — the CPU does literally nothing for 1 second.
 * In real firmware this is unacceptable. You'd miss button presses,
 * sensor readings, communication, everything.
 * 
 * millis() returns the number of milliseconds since boot. By comparing
 * timestamps, you can do non-blocking timing — the CPU keeps running
 * and checks "has enough time passed?" each loop iteration.
 * 
 * This pattern (check elapsed time instead of blocking) is fundamental
 * to ALL embedded programming.
 */
