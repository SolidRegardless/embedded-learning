/*
 * LESSON 8: STM32 — Timers & PWM
 * Board: STM32 Nucleo C031C6 (Cortex-M0+, 48MHz)
 * 
 * ============================================================
 * STM32 TIMERS: THE MOST POWERFUL PERIPHERAL
 * ============================================================
 * 
 * STM32 timers are extraordinarily capable. The C031C6 has:
 *   - TIM1:  Advanced-control timer (16-bit, complementary outputs, dead-time)
 *   - TIM3:  General-purpose timer (16-bit, 4 channels)
 *   - TIM14: Simple timer (16-bit, 1 channel)
 *   - TIM16, TIM17: General-purpose (16-bit, 1 channel each)
 *   - LPTIM1, LPTIM2: Low-power timers (run in sleep mode!)
 * 
 * Compare to:
 *   - AVR ATmega328P: Timer0 (8-bit), Timer1 (16-bit), Timer2 (8-bit) — 3 total
 *   - ESP32: 4 hardware timers + LEDC (8 channels) + MCPWM
 *   - STM32F4: Can have 12+ timers, some 32-bit!
 * 
 * ============================================================
 * HOW A TIMER GENERATES PWM
 * ============================================================
 * 
 * A timer is just a counter that counts up (or down) at a known rate.
 * Three key registers control PWM:
 * 
 *   PRESCALER (PSC):
 *     Divides the input clock.
 *     Timer clock = System clock / (PSC + 1)
 *     Example: 48MHz / (47 + 1) = 1MHz → counter ticks every 1µs
 * 
 *   AUTO-RELOAD REGISTER (ARR):
 *     The counter counts from 0 up to ARR, then resets to 0.
 *     This sets the PWM PERIOD (frequency).
 *     PWM frequency = Timer clock / (ARR + 1)
 *     Example: 1MHz / (999 + 1) = 1kHz PWM
 * 
 *   CAPTURE/COMPARE REGISTER (CCR):
 *     When counter < CCR → output HIGH
 *     When counter >= CCR → output LOW
 *     This sets the DUTY CYCLE.
 *     Duty = CCR / (ARR + 1) × 100%
 * 
 *   Visual (ARR=9, CCR=3):
 *   Counter: 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5...
 *   Output:  ███████░░░░░░░░░░░████████░░░░░░...
 *                   ^CCR       ^ARR
 * 
 * ============================================================
 * STM32 TIMER MODES (way beyond basic PWM!)
 * ============================================================
 * 
 * 1. PWM Generation (what we're doing here)
 *    - Edge-aligned: count up, reset at ARR
 *    - Center-aligned: count up to ARR then down to 0 (smoother for motors)
 * 
 * 2. Input Capture
 *    - Timer records its counter value when an external pin edges
 *    - Measure pulse widths, frequencies, RPM — no CPU involvement!
 *    - ESP32 has this via PCNT/MCPWM; AVR has ICP1 (limited)
 * 
 * 3. Output Compare
 *    - Toggle/set/clear a pin when counter hits a value
 *    - Generate precise timing signals
 * 
 * 4. Encoder Mode (STM32 exclusive at this level!)
 *    - Connect a rotary encoder directly to two timer channels
 *    - Hardware counts quadrature signals — NO interrupts needed
 *    - AVR/ESP32 need software or interrupt-based encoder reading
 * 
 * 5. Complementary Outputs with Dead Time (Advanced timers like TIM1)
 *    - Two outputs: PWM and inverted PWM with a configurable gap
 *    - ESSENTIAL for H-bridge motor drivers and power inverters
 *    - Prevents shoot-through (both transistors ON = short circuit = fire)
 *    - This is why STM32 dominates in motor control applications
 * 
 * 6. One-Pulse Mode
 *    - Timer fires once and stops — perfect for triggering events
 * 
 * ============================================================
 * COMPARISON: PWM ACROSS PLATFORMS
 * ============================================================
 * 
 * ESP32 LEDC:
 *   - 8 channels, up to 13-bit resolution
 *   - Simple API: ledcSetup(channel, freq, resolution)
 *   - Good for LEDs and basic servos
 *   - No input capture, no encoder mode, no complementary outputs
 * 
 * AVR (ATmega328P):
 *   - 6 PWM outputs tied to 3 timers
 *   - 8-bit resolution (Timer0/2) or up to 16-bit (Timer1)
 *   - Fixed to certain pins (not remappable)
 *   - analogWrite() hides the registers
 * 
 * STM32:
 *   - Each timer channel can map to multiple pins (alternate functions)
 *   - 16-bit resolution standard, 32-bit on some timers
 *   - Capture, compare, encoder, complementary, DMA trigger...
 *   - Industrial-grade motor control features
 * 
 * ============================================================
 * THIS LESSON: Pot controls LED brightness via PWM
 * Using Arduino (STM32duino) framework with HAL explanations
 * ============================================================
 */

const int POT_PIN = PA0;     // Analog input — potentiometer
const int LED_PIN = PA6;     // PWM output — TIM3_CH1 on PA6

void setup() {
  Serial.begin(115200);
  Serial.println("Lesson 8: STM32 Timers & PWM");
  
  // Arduino way: analogWrite just works (STM32duino uses HAL underneath)
  // PWM frequency defaults to 1kHz, 8-bit resolution (0-255)
  pinMode(LED_PIN, OUTPUT);
  
  /*
   * ══════════════════════════════════════════════════════════
   * WHAT HAL DOES TO SET UP TIM3 CH1 ON PA6:
   * ══════════════════════════════════════════════════════════
   * 
   *   // 1. Enable clocks
   *   __HAL_RCC_TIM3_CLK_ENABLE();
   *   __HAL_RCC_GPIOA_CLK_ENABLE();
   *   
   *   // 2. Configure PA6 as Alternate Function (TIM3_CH1)
   *   GPIO_InitTypeDef gpio = {0};
   *   gpio.Pin       = GPIO_PIN_6;
   *   gpio.Mode      = GPIO_MODE_AF_PP;        // Alternate function, push-pull
   *   gpio.Alternate = GPIO_AF1_TIM3;          // AF1 = TIM3 on this pin
   *   gpio.Speed     = GPIO_SPEED_FREQ_LOW;
   *   HAL_GPIO_Init(GPIOA, &gpio);
   *   
   *   // 3. Configure Timer3
   *   TIM_HandleTypeDef htim3 = {0};
   *   htim3.Instance               = TIM3;
   *   htim3.Init.Prescaler         = 47;       // 48MHz / 48 = 1MHz
   *   htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
   *   htim3.Init.Period            = 999;      // ARR = 999 → 1kHz PWM
   *   htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
   *   HAL_TIM_PWM_Init(&htim3);
   *   
   *   // 4. Configure PWM channel
   *   TIM_OC_InitTypeDef sConfig = {0};
   *   sConfig.OCMode     = TIM_OCMODE_PWM1;    // High when counter < CCR
   *   sConfig.Pulse      = 500;                // CCR = 500 → 50% duty
   *   sConfig.OCPolarity  = TIM_OCPOLARITY_HIGH;
   *   HAL_TIM_PWM_ConfigChannel(&htim3, &sConfig, TIM_CHANNEL_1);
   *   
   *   // 5. Start PWM
   *   HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
   *   
   *   // To change duty cycle at runtime:
   *   __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, newValue);
   * 
   * ══════════════════════════════════════════════════════════
   * RAW REGISTER EQUIVALENT:
   * ══════════════════════════════════════════════════════════
   * 
   *   RCC->APBENR1 |= RCC_APBENR1_TIM3EN;   // Enable TIM3 clock
   *   RCC->IOPENR  |= RCC_IOPENR_GPIOAEN;   // Enable GPIOA clock
   *   
   *   // PA6 → AF1 (TIM3_CH1)
   *   GPIOA->MODER  &= ~(3 << (6*2));
   *   GPIOA->MODER  |=  (2 << (6*2));   // Mode = Alternate Function
   *   GPIOA->AFR[0] &= ~(0xF << (6*4));
   *   GPIOA->AFR[0] |=  (1 << (6*4));   // AF1
   *   
   *   TIM3->PSC  = 47;         // Prescaler
   *   TIM3->ARR  = 999;        // Auto-reload
   *   TIM3->CCR1 = 500;        // Compare value (duty cycle)
   *   TIM3->CCMR1 = (6 << 4); // PWM mode 1 on channel 1
   *   TIM3->CCER  = 1;         // Enable channel 1 output
   *   TIM3->CR1   = 1;         // Enable counter
   */
}

void loop() {
  // Read potentiometer (12-bit ADC on STM32: 0-4095)
  int potValue = analogRead(POT_PIN);
  
  // Map 12-bit ADC to 8-bit PWM (STM32duino analogWrite is 8-bit by default)
  int pwmValue = map(potValue, 0, 4095, 0, 255);
  
  // Set LED brightness
  analogWrite(LED_PIN, pwmValue);
  
  // Print values
  Serial.print("Pot: ");
  Serial.print(potValue);
  Serial.print(" → PWM: ");
  Serial.print(pwmValue);
  Serial.print(" → Duty: ");
  Serial.print((pwmValue * 100) / 255);
  Serial.println("%");
  
  delay(100);
  
  /*
   * NOTE ON RESOLUTION:
   * 
   * STM32duino analogWrite defaults to 8-bit (0-255) for Arduino compat.
   * But the hardware supports 16-bit! You can configure this:
   *   analogWriteResolution(16);  // Now analogWrite takes 0-65535
   * 
   * Compare:
   *   AVR:   8-bit PWM (256 steps) — can see LED flicker at low duty
   *   ESP32: up to 13-bit (8192 steps) via LEDC
   *   STM32: up to 16-bit (65536 steps) — buttery smooth dimming
   * 
   * More bits = smoother dimming, especially noticeable at low brightness.
   * At 1% duty cycle: 8-bit = 2-3 ticks (visible flicker), 16-bit = 655 ticks (smooth).
   */
}
