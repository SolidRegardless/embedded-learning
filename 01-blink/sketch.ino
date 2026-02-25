/*
 * LESSON 1: Blink — GPIO Output
 * 
 * CONCEPTS:
 * - GPIO (General Purpose Input/Output) pins
 * - pinMode() — configuring a pin as output
 * - digitalWrite() — setting a pin HIGH (3.3V) or LOW (0V)
 * - delay() — blocking wait (bad practice in real firmware, fine for learning)
 * 
 * WHAT'S HAPPENING AT THE HARDWARE LEVEL:
 * When you call digitalWrite(LED_PIN, HIGH), the ESP32 sets a bit in a 
 * hardware register (GPIO_OUT_REG). This physically connects the pin to 
 * the 3.3V rail through a transistor. Current flows through the LED and 
 * resistor to ground, and the LED lights up.
 * 
 * The Arduino abstractions (pinMode, digitalWrite) hide the register 
 * manipulation. Below we show BOTH approaches.
 */

// Arduino way: use a constant for the pin number
const int LED_PIN = 2;  // GPIO2 — built-in LED on many ESP32 boards

void setup() {
  // Tell the hardware: "GPIO2 is an output pin"
  // Under the hood this sets bits in GPIO_ENABLE_REG
  pinMode(LED_PIN, OUTPUT);
  
  // Start serial so we can print debug info
  Serial.begin(115200);
  Serial.println("Lesson 1: Blink starting...");
}

void loop() {
  // --- Arduino abstraction way ---
  digitalWrite(LED_PIN, HIGH);   // LED on — pin goes to 3.3V
  Serial.println("LED ON");
  delay(1000);                   // Wait 1 second (1000ms)
  
  digitalWrite(LED_PIN, LOW);    // LED off — pin goes to 0V
  Serial.println("LED OFF");
  delay(1000);
}

/*
 * BONUS: The raw register way (commented out — try swapping it in!)
 * This is what digitalWrite() does under the hood on ESP32.
 * 
 * #include "soc/gpio_reg.h"
 * 
 * // Turn ON: set bit 2 in the GPIO output set register
 * REG_WRITE(GPIO_OUT_W1TS_REG, 1 << LED_PIN);
 * 
 * // Turn OFF: set bit 2 in the GPIO output clear register  
 * REG_WRITE(GPIO_OUT_W1TC_REG, 1 << LED_PIN);
 * 
 * Two separate registers for set/clear is common in embedded — it's 
 * atomic (no read-modify-write race condition with interrupts).
 */
