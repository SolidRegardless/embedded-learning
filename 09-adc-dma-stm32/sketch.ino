/*
 * LESSON 9: STM32 — ADC with DMA
 * Board: STM32 Nucleo C031C6 (Cortex-M0+, 48MHz)
 * 
 * ============================================================
 * DMA: DIRECT MEMORY ACCESS — THE HARDWARE COPY MACHINE
 * ============================================================
 * 
 * DMA is one of the most important concepts in embedded systems,
 * and one that Arduino/ESP32 users almost never encounter directly.
 * 
 * The problem DMA solves:
 *   Without DMA (polling or interrupt-driven):
 *     1. ADC finishes converting → triggers interrupt
 *     2. CPU stops what it's doing → saves context → runs ISR
 *     3. ISR reads ADC data register → stores in RAM
 *     4. CPU restores context → returns to main code
 *     Result: CPU is interrupted for EVERY SINGLE sample!
 *     At 1Msps (1 million samples/sec), CPU is constantly interrupted.
 * 
 *   With DMA:
 *     1. ADC finishes converting → DMA hardware notices
 *     2. DMA copies data from ADC register to RAM buffer — NO CPU!
 *     3. After N samples, DMA interrupts CPU ONCE: "buffer is full"
 *     4. CPU processes the entire buffer at once
 *     Result: CPU is free to run your code while samples collect.
 * 
 * ============================================================
 * DMA ARCHITECTURE
 * ============================================================
 * 
 *   ┌─────────┐    DMA Channel     ┌──────────┐
 *   │   ADC   │ ──────────────────> │   RAM    │
 *   │ (data)  │   (hardware copy)   │ (buffer) │
 *   └─────────┘                     └──────────┘
 *        │                               │
 *        └── Peripheral address           └── Memory address
 *            (source)                         (destination)
 * 
 * Each DMA transfer needs:
 *   - Source address (peripheral register, e.g., ADC->DR)
 *   - Destination address (RAM buffer)
 *   - Transfer count (number of data items)
 *   - Data width (8-bit, 16-bit, or 32-bit)
 *   - Mode: Normal (stop after N transfers) or Circular (wrap around)
 * 
 * Circular mode is perfect for ADC: the DMA fills the buffer, wraps
 * to the start, and keeps going. You always have fresh data.
 * 
 * ============================================================
 * WHERE DMA EXISTS (and doesn't)
 * ============================================================
 * 
 * STM32: ALL models have DMA. It's deeply integrated — ADC, UART, SPI,
 *   I2C, and timers can all trigger DMA. This is a core STM32 strength.
 * 
 * ESP32: Has a DMA-like system for SPI and I2S, but it's not a general
 *   purpose DMA controller. The ADC has a "DMA" mode but it's really
 *   the I2S peripheral driving it. Much less flexible.
 * 
 * AVR (ATmega328P): NO DMA at all. Every byte must be moved by the CPU.
 *   This is why AVR can't do high-speed data acquisition.
 *   (Some newer AVR like AVR-DA series have a basic event system, still no DMA)
 * 
 * RP2040 (Pico): Has 12 DMA channels! Very capable. Used heavily for PIO.
 * 
 * ARM Cortex-M in general: DMA is standard. If you see DMA in job postings,
 *   they mean STM32/NXP/TI ARM chips, not Arduino.
 * 
 * ============================================================
 * PRACTICAL IMPACT
 * ============================================================
 * 
 * Without DMA, an STM32 sampling ADC at 100kHz:
 *   - 100,000 interrupts/second
 *   - Each interrupt ~2µs overhead (save/restore + ISR code)
 *   - CPU spends 20% of its time just handling ADC interrupts!
 * 
 * With DMA, buffer size 1000:
 *   - 100 interrupts/second (every 1000 samples)
 *   - CPU overhead drops to <0.1%
 *   - Process data in batches (FFT, averaging, filtering)
 * 
 * ============================================================
 * THIS LESSON: Simulated DMA concept using Arduino framework
 * ============================================================
 * 
 * Wokwi's STM32duino doesn't expose raw HAL DMA setup, so we:
 * 1. Demonstrate the CONCEPT with a buffer-fill approach
 * 2. Show EXACTLY what the HAL/register DMA code looks like in comments
 * 3. Use analogRead in a fast loop to fill a buffer (CPU-driven, as contrast)
 */

const int POT_PIN = PA0;
const int BUFFER_SIZE = 64;     // DMA would fill this automatically
uint16_t adcBuffer[BUFFER_SIZE]; // This is where DMA would deposit samples

// Statistics
float avgValue = 0;
uint16_t minValue = 4095;
uint16_t maxValue = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Lesson 9: STM32 ADC with DMA (concept demonstration)");
  Serial.println("In real firmware, DMA fills the buffer with ZERO CPU involvement.");
  Serial.println();
  
  /*
   * ══════════════════════════════════════════════════════════
   * REAL HAL DMA SETUP (what production code looks like):
   * ══════════════════════════════════════════════════════════
   * 
   * ADC_HandleTypeDef hadc;
   * DMA_HandleTypeDef hdma_adc;
   * 
   * // 1. Enable clocks
   * __HAL_RCC_DMA1_CLK_ENABLE();
   * __HAL_RCC_ADC_CLK_ENABLE();
   * __HAL_RCC_GPIOA_CLK_ENABLE();
   * 
   * // 2. Configure DMA
   * hdma_adc.Instance                 = DMA1_Channel1;
   * hdma_adc.Init.Request             = DMA_REQUEST_ADC1;
   * hdma_adc.Init.Direction           = DMA_PERIPH_TO_MEMORY;
   * hdma_adc.Init.PeriphInc           = DMA_PINC_DISABLE;    // ADC register doesn't move
   * hdma_adc.Init.MemInc              = DMA_MINC_ENABLE;     // Increment through buffer
   * hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  // 16-bit
   * hdma_adc.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
   * hdma_adc.Init.Mode                = DMA_CIRCULAR;        // Wrap around!
   * hdma_adc.Init.Priority            = DMA_PRIORITY_HIGH;
   * HAL_DMA_Init(&hdma_adc);
   * 
   * // 3. Link DMA to ADC
   * __HAL_LINKDMA(&hadc, DMA_Handle, hdma_adc);
   * 
   * // 4. Configure ADC
   * hadc.Instance                   = ADC1;
   * hadc.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;
   * hadc.Init.Resolution            = ADC_RESOLUTION_12B;
   * hadc.Init.ScanConvMode          = ADC_SCAN_ENABLE;     // Multiple channels
   * hadc.Init.ContinuousConvMode    = ENABLE;              // Keep converting
   * hadc.Init.DMAContinuousRequests = ENABLE;              // DMA after each conversion
   * HAL_ADC_Init(&hadc);
   * 
   * // 5. Start ADC + DMA — THIS IS THE MAGIC LINE
   * HAL_ADC_Start_DMA(&hadc, (uint32_t*)adcBuffer, BUFFER_SIZE);
   * 
   * // After this, the ADC continuously converts and DMA fills the buffer.
   * // CPU does NOTHING. The buffer always has fresh data.
   * // You can set up a "half transfer" callback to process the first half
   * // while DMA fills the second half — double buffering!
   * 
   * // Callbacks (optional):
   * // HAL_ADC_ConvCpltCallback()   — called when buffer is full
   * // HAL_ADC_ConvHalfCpltCallback() — called when buffer is half full
   * 
   * ══════════════════════════════════════════════════════════
   * RAW REGISTER EQUIVALENT:
   * ══════════════════════════════════════════════════════════
   * 
   *   RCC->AHBENR  |= RCC_AHBENR_DMA1EN;
   *   RCC->APBENR2 |= RCC_APBENR2_ADCEN;
   *   
   *   // DMA Channel 1 for ADC
   *   DMA1_Channel1->CPAR  = (uint32_t)&ADC1->DR;       // Source: ADC data reg
   *   DMA1_Channel1->CMAR  = (uint32_t)adcBuffer;       // Dest: RAM buffer
   *   DMA1_Channel1->CNDTR = BUFFER_SIZE;                // Transfer count
   *   DMA1_Channel1->CCR   = DMA_CCR_MINC               // Memory increment
   *                        | DMA_CCR_MSIZE_0             // 16-bit memory
   *                        | DMA_CCR_PSIZE_0             // 16-bit peripheral
   *                        | DMA_CCR_CIRC                // Circular mode
   *                        | DMA_CCR_TCIE                // Transfer complete interrupt
   *                        | DMA_CCR_EN;                 // Enable channel
   *   
   *   // ADC continuous mode + DMA
   *   ADC1->CFGR1 = ADC_CFGR1_CONT | ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG;
   *   ADC1->CR   |= ADC_CR_ADSTART;  // Go!
   */
}

void loop() {
  // ── Simulate what DMA does (but using CPU — for demonstration) ──
  // In real code, this entire loop would be replaced by:
  //   "DMA is filling the buffer in the background, just read it"
  
  unsigned long startMicros = micros();
  
  // Fill buffer (CPU-intensive way — DMA does this for free)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    adcBuffer[i] = analogRead(POT_PIN);
  }
  
  unsigned long elapsedMicros = micros() - startMicros;
  
  // Process the buffer — this is where your CPU time SHOULD go
  // Calculate statistics (average, min, max)
  uint32_t sum = 0;
  minValue = 4095;
  maxValue = 0;
  
  for (int i = 0; i < BUFFER_SIZE; i++) {
    sum += adcBuffer[i];
    if (adcBuffer[i] < minValue) minValue = adcBuffer[i];
    if (adcBuffer[i] > maxValue) maxValue = adcBuffer[i];
  }
  avgValue = (float)sum / BUFFER_SIZE;
  
  // Convert to voltage (3.3V reference, 12-bit ADC)
  float voltage = (avgValue / 4095.0) * 3.3;
  
  Serial.print("Buffer filled in ");
  Serial.print(elapsedMicros);
  Serial.print("µs (CPU busy!) | Avg: ");
  Serial.print(avgValue, 1);
  Serial.print(" (");
  Serial.print(voltage, 3);
  Serial.print("V) | Min: ");
  Serial.print(minValue);
  Serial.print(" Max: ");
  Serial.println(maxValue);
  
  Serial.println("  → With DMA, that fill time would be 0µs of CPU time!");
  
  delay(500);
  
  /*
   * ══════════════════════════════════════════════════════════
   * DOUBLE BUFFERING WITH DMA
   * ══════════════════════════════════════════════════════════
   * 
   * Advanced technique used in audio, signal processing, motor control:
   * 
   *   Buffer: [────── First Half ──────|────── Second Half ──────]
   *                                    
   *   1. DMA fills first half → "Half Transfer" interrupt fires
   *   2. CPU processes first half while DMA fills second half
   *   3. DMA fills second half → "Transfer Complete" interrupt fires
   *   4. CPU processes second half while DMA wraps to first half
   *   
   * Result: CONTINUOUS data processing with no gaps. This is how
   * audio codecs, oscilloscopes, and motor controllers work.
   * 
   * You simply CANNOT do this on AVR. There's no DMA.
   * ESP32 can do it with I2S+ADC, but it's hacky and limited.
   * STM32 was designed for this from the ground up.
   */
}
