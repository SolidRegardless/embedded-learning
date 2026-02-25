/*
 * LESSON 18: Pico — PWM with Hardware Slices
 * Board: Raspberry Pi Pico (RP2040, Dual Cortex-M0+, 125MHz, 264KB RAM)
 * 
 * ============================================================
 * RP2040 PWM ARCHITECTURE
 * ============================================================
 * 
 * The RP2040 has 8 PWM "slices", each with 2 output channels (A and B):
 * 
 *   Slice 0: GP0 (0A), GP1 (0B)
 *   Slice 1: GP2 (1A), GP3 (1B)
 *   Slice 2: GP4 (2A), GP5 (2B)
 *   Slice 3: GP6 (3A), GP7 (3B)
 *   Slice 4: GP8 (4A), GP9 (4B)
 *   Slice 5: GP10 (5A), GP11 (5B)
 *   Slice 6: GP12 (6A), GP13 (6B)
 *   Slice 7: GP14 (7A), GP15 (7B)
 * 
 * Total: 16 PWM outputs! (8 slices × 2 channels)
 * 
 * Key detail: both channels in a slice share the SAME frequency
 * (same counter/wrap value), but can have DIFFERENT duty cycles.
 * 
 *   ┌─────────────────────────────────┐
 *   │         PWM Slice N             │
 *   │                                 │
 *   │  ┌───────────┐                  │
 *   │  │  Counter   │ ← Counts 0 to wrap value, then resets
 *   │  │  (16-bit)  │                 │
 *   │  └─────┬─────┘                  │
 *   │        │                        │
 *   │   ┌────┴────┐  ┌────┴────┐     │
 *   │   │ Compare │  │ Compare │     │
 *   │   │   A     │  │   B     │     │
 *   │   └────┬────┘  └────┬────┘     │
 *   │        │             │          │
 *   │      Out A         Out B        │
 *   └────────┴─────────────┴──────────┘
 * 
 * ============================================================
 * PWM COMPARISON ACROSS PLATFORMS
 * ============================================================
 * 
 * RP2040 PWM:
 *   - 8 slices × 2 channels = 16 outputs
 *   - 16-bit counter (0-65535)
 *   - Frequency = 125MHz / (wrap + 1) / divider
 *   - Independent duty per channel, shared frequency per slice
 *   - Clock divider: integer + 4-bit fractional (e.g., 1.0625)
 * 
 * ESP32 LEDC (LED Control):
 *   - 16 channels (8 high-speed, 8 low-speed)
 *   - Each channel independently configurable
 *   - Up to 20-bit resolution (but lower resolution = higher frequency)
 *   - Hardware fade support (automatic duty cycle ramping!)
 *   - ledcSetup(channel, freq, resolution) + ledcAttachPin(pin, channel)
 * 
 * STM32 TIM (Timer-based PWM):
 *   - Multiple timers, each with 1-4 channels
 *   - 16-bit or 32-bit counters (TIM2/TIM5 are 32-bit on many STM32)
 *   - Advanced timers (TIM1/TIM8): complementary outputs, dead-time
 *     insertion, break input — designed for motor control
 *   - Most flexible but most complex to configure
 * 
 * AVR (Arduino Uno):
 *   - 6 PWM outputs on 3 timers (Timer0, Timer1, Timer2)
 *   - Timer0: 8-bit, used by millis() — changing it breaks timing!
 *   - Timer1: 16-bit, most flexible
 *   - Timer2: 8-bit
 *   - Only ~490Hz or ~980Hz with analogWrite()
 *   - Custom frequencies require direct timer register manipulation
 * 
 * ============================================================
 * THIS LESSON:
 * Drive an RGB LED with 3 PWM channels, controlled by a potentiometer
 * that cycles through the colour wheel (HSV → RGB conversion).
 * ============================================================
 */

// RGB LED pins — chosen from different slices so each has independent control
const int RED_PIN   = 2;   // Slice 1, Channel A
const int GREEN_PIN = 4;   // Slice 2, Channel A
const int BLUE_PIN  = 6;   // Slice 3, Channel A

const int POT_PIN = 26;    // ADC0 — potentiometer for colour selection

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Lesson 18: RP2040 PWM Slices — RGB LED");
  Serial.println("Turn the potentiometer to cycle through colours");
  Serial.println();
  
  // Set PWM frequency via analogWriteFreq() — Arduino-Pico framework
  // Default is 1000Hz. Let's use 1000Hz (fine for LEDs, no visible flicker).
  analogWriteFreq(1000);
  
  // Set PWM resolution to 8 bits (0-255) for simplicity
  // Could use up to 16 bits (0-65535) on RP2040!
  analogWriteRange(255);
  
  /*
   * Under the hood, analogWriteFreq() and analogWriteRange() configure:
   *   - The slice's wrap value (TOP) = range - 1
   *   - The clock divider = 125MHz / (frequency × range)
   * 
   * For 1000Hz with 8-bit range:
   *   divider = 125,000,000 / (1000 × 256) ≈ 488.28
   *   Actual divider: 488 + fractional part
   * 
   * For comparison, raw Pico SDK code would be:
   *   pwm_config cfg = pwm_get_default_config();
   *   pwm_config_set_clkdiv(&cfg, 488.28f);
   *   pwm_config_set_wrap(&cfg, 255);
   *   pwm_init(pwm_gpio_to_slice_num(RED_PIN), &cfg, true);
   *   gpio_set_function(RED_PIN, GPIO_FUNC_PWM);
   *   pwm_set_gpio_level(RED_PIN, duty);  // 0-255
   * 
   * On ESP32, equivalent:
   *   ledcSetup(0, 1000, 8);   // Channel 0, 1kHz, 8-bit
   *   ledcAttachPin(RED_PIN, 0);
   *   ledcWrite(0, duty);
   * 
   * On STM32 (HAL):
   *   // Configure TIM3 Channel 1 for PWM
   *   htim3.Instance = TIM3;
   *   htim3.Init.Prescaler = 48000000 / (1000 * 256) - 1;
   *   htim3.Init.Period = 255;
   *   HAL_TIM_PWM_Init(&htim3);
   *   HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
   *   __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
   */
  
  // Configure ADC for potentiometer
  analogReadResolution(12);
  
  Serial.println("PWM initialized on 3 slices for RGB LED");
}

void loop() {
  // Read potentiometer (0-4095)
  int potValue = analogRead(POT_PIN);
  
  // Map pot value to hue (0-359 degrees)
  int hue = map(potValue, 0, 4095, 0, 359);
  
  // Convert HSV to RGB
  // Hue: 0-359 (colour wheel), Saturation: 255, Value: 255
  uint8_t r, g, b;
  hsvToRgb(hue, 255, 255, r, g, b);
  
  // Write PWM duty cycles to each colour channel
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
  
  /*
   * analogWrite() on RP2040:
   *   1. Determines which slice/channel the pin belongs to
   *   2. Configures the pin for PWM function (GPIO_FUNC_PWM)
   *   3. Sets the compare value for that channel
   * 
   * Since R, G, B are on different slices, they're fully independent.
   * If we used two pins on the SAME slice (e.g., GP2 and GP3 = Slice 1),
   * they'd share the same frequency but could still have different duties.
   */
  
  // Print every 200ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.print("Hue: ");
    Serial.print(hue);
    Serial.print("°  RGB(");
    Serial.print(r);
    Serial.print(", ");
    Serial.print(g);
    Serial.print(", ");
    Serial.print(b);
    Serial.println(")");
  }
  
  delay(10);
}

// ═══════════════════════════════════════════
// HSV to RGB conversion
// ═══════════════════════════════════════════

void hsvToRgb(int h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
  /*
   * HSV (Hue-Saturation-Value) is much more intuitive for colour cycling
   * than RGB. Hue is the angle on the colour wheel:
   *   0°   = Red
   *   60°  = Yellow
   *   120° = Green
   *   180° = Cyan
   *   240° = Blue
   *   300° = Magenta
   *   360° = Red (wraps around)
   * 
   * By sweeping hue 0→359, we get a smooth rainbow.
   */
  
  if (s == 0) {
    r = g = b = v;
    return;
  }
  
  uint8_t region = h / 60;
  uint8_t remainder = (h - (region * 60)) * 255 / 60;
  
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
  switch (region) {
    case 0:  r = v; g = t; b = p; break;
    case 1:  r = q; g = v; b = p; break;
    case 2:  r = p; g = v; b = t; break;
    case 3:  r = p; g = q; b = v; break;
    case 4:  r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
}

/*
 * ══════════════════════════════════════════════════════════
 * PWM APPLICATIONS BEYOND LEDs
 * ══════════════════════════════════════════════════════════
 * 
 * PWM is everywhere in embedded systems:
 * 
 *   Motor speed control: Vary duty cycle → vary average voltage to motor
 *   Servo control: 50Hz PWM, 1-2ms pulse width = angle
 *   Audio generation: PWM at >20kHz + low-pass filter = DAC
 *   Power supplies: Switch-mode converters are just PWM + inductor
 *   Display brightness: Backlight dimming on LCD panels
 *   Fan control: PC fans use 25kHz PWM (Intel spec)
 *   Heating elements: Slow PWM (1Hz) for thermal control
 * 
 * RP2040 advantage: 16 PWM outputs means you can control
 * many motors/LEDs/servos without external PWM driver chips.
 * 
 * For motor control specifically, STM32's advanced timers
 * (TIM1/TIM8) have features RP2040 lacks:
 *   - Complementary outputs (for H-bridges)
 *   - Programmable dead-time (prevents shoot-through!)
 *   - Break input (emergency stop — hardware kills PWM instantly)
 * These are essential for safe motor/power converter control.
 */
