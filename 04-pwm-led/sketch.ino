/*
 * LESSON 4: PWM — Pulse Width Modulation
 * 
 * CONCEPTS:
 * - PWM: faking analog output with fast digital switching
 * - Duty cycle: percentage of time the signal is HIGH
 * - LED dimming, motor speed control, servo positioning
 * - ESP32's LEDC (LED Control) peripheral
 * 
 * WHAT IS PWM?
 * Digital pins can only be HIGH (3.3V) or LOW (0V). There's no 1.5V output.
 * But if you switch between HIGH and LOW really fast (thousands of times 
 * per second), the AVERAGE voltage is somewhere in between.
 * 
 *   100% duty cycle: ████████████  = 3.3V average (always on)
 *    50% duty cycle: ████    ████  = 1.65V average
 *    25% duty cycle: ██      ██    = 0.825V average  
 *     0% duty cycle:              = 0V (always off)
 * 
 * At high enough frequency (>1kHz), an LED appears smoothly dimmed 
 * because your eyes can't see the individual on/off flickers.
 * 
 * ESP32 PWM:
 * ESP32 has a dedicated LEDC peripheral with 16 channels.
 * Each channel can independently control frequency and duty cycle.
 * Resolution: up to 16 bits (0-65535) but 8-bit (0-255) is common.
 */

const int LED_PIN = 2;
const int POT_PIN = 34;

// ESP32 LEDC configuration
const int PWM_CHANNEL = 0;     // Which of the 16 LEDC channels to use
const int PWM_FREQ = 5000;     // 5kHz — fast enough for smooth LED dimming
const int PWM_RESOLUTION = 8;  // 8-bit: duty cycle 0-255

void setup() {
  Serial.begin(115200);
  
  // Configure the LEDC peripheral
  // This is ESP32-specific — Arduino Uno uses analogWrite() which is simpler
  // but ESP32's LEDC gives you more control
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_PIN, PWM_CHANNEL);
  
  pinMode(POT_PIN, INPUT);
  
  Serial.println("Lesson 4: PWM LED Dimming");
  Serial.println("Turn the potentiometer to dim the LED");
}

void loop() {
  // Read potentiometer (12-bit: 0-4095)
  int potValue = analogRead(POT_PIN);
  
  // Map 12-bit ADC value to 8-bit PWM duty cycle
  // map(value, fromLow, fromHigh, toLow, toHigh)
  int dutyCycle = map(potValue, 0, 4095, 0, 255);
  
  // Set PWM duty cycle
  ledcWrite(PWM_CHANNEL, dutyCycle);
  
  // Print every 200ms (not every loop — that would flood serial)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    
    int percentage = map(dutyCycle, 0, 255, 0, 100);
    Serial.print("POT: ");
    Serial.print(potValue);
    Serial.print(" → PWM duty: ");
    Serial.print(dutyCycle);
    Serial.print("/255 (");
    Serial.print(percentage);
    Serial.println("%)");
  }
  
  delay(10);  // Small delay to reduce ADC noise
}

/*
 * WHY PWM MATTERS IN EMBEDDED:
 * 
 * - LED dimming (this lesson)
 * - Motor speed control (DC motors respond to average voltage)
 * - Servo positioning (1-2ms pulse width = angle)
 * - Audio generation (square wave buzzer tones)
 * - Power regulation (switching power supplies)
 * 
 * STATIC LOCAL VARIABLES:
 * Notice "static unsigned long lastPrint" inside loop().
 * 'static' means it persists between function calls — like a global 
 * but scoped to the function. Essential pattern in embedded C/C++.
 */
