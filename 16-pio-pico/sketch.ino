/*
 * LESSON 16: Pico — PIO (Programmable I/O)
 * Board: Raspberry Pi Pico (RP2040, Dual Cortex-M0+, 125MHz, 264KB RAM)
 * 
 * ============================================================
 * PIO: THE RP2040's KILLER FEATURE
 * ============================================================
 * 
 * PIO (Programmable I/O) is hardware unique to the RP2040.
 * It does NOT exist on STM32, AVR, ESP32, or any other common MCU.
 * 
 * What is it? The RP2040 contains TWO PIO blocks, each with:
 *   - 4 state machines (8 total)
 *   - 32 instructions of shared instruction memory (per block)
 *   - Each state machine runs independently of the CPU cores
 *   - Runs at system clock (125MHz) or divided down
 *   - Has its own tiny instruction set (9 instructions!)
 * 
 * ┌─────────────────────────────────────────────────┐
 * │                   RP2040                         │
 * │                                                  │
 * │  ┌──────────┐  ┌──────────┐                      │
 * │  │  Core 0  │  │  Core 1  │   CPU cores          │
 * │  └──────────┘  └──────────┘                      │
 * │                                                  │
 * │  ┌──────────────────────────────────────────┐    │
 * │  │  PIO Block 0                             │    │
 * │  │  ┌────┐ ┌────┐ ┌────┐ ┌────┐            │    │
 * │  │  │SM0 │ │SM1 │ │SM2 │ │SM3 │  4 SMs     │    │
 * │  │  └────┘ └────┘ └────┘ └────┘            │    │
 * │  │  [32 instructions shared memory]         │    │
 * │  └──────────────────────────────────────────┘    │
 * │                                                  │
 * │  ┌──────────────────────────────────────────┐    │
 * │  │  PIO Block 1  (identical)                │    │
 * │  │  ┌────┐ ┌────┐ ┌────┐ ┌────┐            │    │
 * │  │  │SM0 │ │SM1 │ │SM2 │ │SM3 │            │    │
 * │  │  └────┘ └────┘ └────┘ └────┘            │    │
 * │  │  [32 instructions shared memory]         │    │
 * │  └──────────────────────────────────────────┘    │
 * │                                                  │
 * └─────────────────────────────────────────────────┘
 * 
 * ============================================================
 * PIO INSTRUCTION SET (only 9 instructions!)
 * ============================================================
 * 
 *   JMP   — conditional jump
 *   WAIT  — stall until condition (pin high/low, IRQ)
 *   IN    — shift bits into ISR (input shift register)
 *   OUT   — shift bits from OSR (output shift register)
 *   PUSH  — push ISR to RX FIFO (CPU reads this)
 *   PULL  — pull TX FIFO to OSR (CPU writes this)
 *   MOV   — move data between registers
 *   IRQ   — set/clear/wait on IRQ flags
 *   SET   — set pin or register to immediate value
 * 
 * Each instruction executes in exactly ONE clock cycle.
 * At 125MHz, that's 8ns per instruction — precise enough to
 * bit-bang protocols like WS2812 (which needs ~300ns timing).
 * 
 * ============================================================
 * WHY PIO MATTERS
 * ============================================================
 * 
 * On other MCUs, driving WS2812 LEDs requires:
 *   AVR: Carefully timed assembly with interrupts DISABLED
 *   ESP32: RMT peripheral (designed for IR, repurposed for WS2812)
 *   STM32: DMA + Timer + PWM (complex setup)
 * 
 * On RP2040: PIO handles it in hardware, zero CPU involvement.
 * The CPU pushes colour data to a FIFO, PIO does the rest.
 * 
 * Other things PIO can implement:
 *   - Extra UART, SPI, I2C ports (unlimited!)
 *   - VGA/DVI video output
 *   - Rotary encoder reading
 *   - Logic analyser
 *   - SD card interface
 *   - Ethernet MAC
 * 
 * ============================================================
 * THIS LESSON: Drive WS2812 NeoPixels using Adafruit NeoPixel library
 * The library uses PIO under the hood on RP2040!
 * ============================================================
 * 
 * The Adafruit NeoPixel library detects RP2040 at compile time and
 * automatically uses PIO state machines instead of bit-banging.
 * We'll drive a ring of NeoPixels with a rainbow animation.
 */

#include <Adafruit_NeoPixel.h>

// NeoPixel configuration
const int NEOPIXEL_PIN = 16;    // GP16 — data line to NeoPixel ring
const int NUM_PIXELS = 16;       // 16-LED ring

// Create NeoPixel object
// On RP2040, this internally:
//   1. Claims a PIO state machine (from PIO0 or PIO1)
//   2. Loads a tiny PIO program (~7 instructions) into PIO instruction memory
//   3. Configures the state machine's clock divider for WS2812 timing
//   4. Sets up DMA to feed pixel data from RAM to the PIO FIFO
Adafruit_NeoPixel strip(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

/*
 * ══════════════════════════════════════════════════════════
 * WS2812 PROTOCOL (what PIO is handling for us)
 * ══════════════════════════════════════════════════════════
 * 
 * WS2812 uses a single-wire protocol with timing-encoded bits:
 * 
 *   Bit 1: HIGH for ~700ns, LOW for ~600ns  (total ~1.25µs)
 *   Bit 0: HIGH for ~350ns, LOW for ~800ns  (total ~1.25µs)
 *   Reset: LOW for >50µs (tells strip: data complete)
 * 
 * Each LED takes 24 bits (8 green + 8 red + 8 blue).
 * Data cascades: first LED takes 24 bits, passes the rest along.
 * 
 * PIO program to drive WS2812 (simplified):
 * 
 *   .program ws2812
 *   .side_set 1
 *       pull block          side 0    ; Pull 24 bits from FIFO
 *   bitloop:
 *       out x, 1            side 0    ; Shift out one bit to X
 *       jmp !x, do_zero     side 1    ; Drive pin HIGH, branch on bit value
 *   do_one:
 *       nop                 side 1    ; Bit=1: stay HIGH longer
 *       jmp bitloop         side 0    ; Then go LOW
 *   do_zero:
 *       nop                 side 0    ; Bit=0: go LOW sooner
 *       jmp bitloop         side 0    ; Continue
 * 
 * The "side set" feature sets a pin as a SIDE EFFECT of each instruction.
 * This is how PIO achieves precise timing — the pin changes happen
 * on the exact clock cycle the instruction executes.
 * 
 * ══════════════════════════════════════════════════════════
 * HOW OTHER MCUs HANDLE WS2812 (for comparison)
 * ══════════════════════════════════════════════════════════
 * 
 * AVR (Arduino Uno):
 *   - Bit-banged in hand-tuned assembly (Adafruit library)
 *   - Interrupts MUST be disabled during transmission
 *   - At 16MHz, only ~5 instructions per bit period
 *   - Blocks CPU completely during update
 * 
 * ESP32:
 *   - Uses the RMT (Remote Control Transceiver) peripheral
 *   - RMT was designed for IR remote signals but works for WS2812
 *   - Hardware peripheral, doesn't block CPU
 *   - But only 8 RMT channels, and RMT memory is limited
 * 
 * STM32:
 *   - Timer + DMA + PWM: configure a timer for 800kHz,
 *     use DMA to update the PWM duty cycle for each bit
 *   - Complex setup (~100 lines of HAL code)
 *   - Or SPI at 2.4MHz with careful bit patterns
 *   - Works but it's a hack — not designed for this
 * 
 * RP2040 PIO:
 *   - Purpose-built for exactly this kind of protocol
 *   - 7 instructions, runs forever, zero CPU overhead
 *   - Can drive multiple strips on different state machines
 *   - Clean, elegant, efficient
 */

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Lesson 16: RP2040 PIO — NeoPixel Ring");
  Serial.println("PIO state machines driving WS2812 protocol in hardware");
  Serial.println();
  
  strip.begin();           // Initialize PIO state machine + pin
  strip.setBrightness(50); // Limit brightness (0-255) — good practice!
  strip.show();            // Initialize all pixels to OFF
  
  /*
   * At this point, a PIO state machine is running!
   * It's sitting idle, waiting for data in its TX FIFO.
   * When we call strip.show(), pixel data is pushed to the FIFO,
   * and PIO shifts it out with perfect timing — no CPU involvement.
   * 
   * The CPU is 100% free to do other work while PIO handles the LEDs.
   * On a dual-core RP2040, that means BOTH cores are free.
   */
  
  Serial.println("NeoPixel strip initialized via PIO");
  Serial.print("Using ");
  Serial.print(NUM_PIXELS);
  Serial.println(" LEDs on a ring");
}

// ═══════════════════════════════════════════
// Rainbow animation variables
// ═══════════════════════════════════════════
uint16_t hueOffset = 0;

void loop() {
  // Rainbow cycle: each pixel has a different hue, and we rotate over time
  for (int i = 0; i < NUM_PIXELS; i++) {
    // Calculate hue for this pixel (0-65535 in NeoPixel library)
    // Spread the full colour wheel across the ring
    uint16_t pixelHue = hueOffset + (i * 65536L / NUM_PIXELS);
    
    // ColorHSV converts hue/saturation/value to RGB
    // Hue: 0-65535 (full colour wheel)
    // Saturation: 0-255 (0 = white, 255 = full colour)
    // Value: 0-255 (brightness)
    uint32_t colour = strip.ColorHSV(pixelHue, 255, 255);
    
    // Gamma correction makes brightness look more natural to human eyes
    // Without it, the middle range looks too bright (LEDs are non-linear)
    colour = strip.gamma32(colour);
    
    strip.setPixelColor(i, colour);
  }
  
  strip.show();  // Push all pixel data to PIO FIFO → PIO shifts it out
  
  // Advance the rainbow
  hueOffset += 256;  // Speed of rotation
  
  delay(20);  // ~50fps update rate
  
  /*
   * ══════════════════════════════════════════════════════════
   * WHAT HAPPENS DURING strip.show():
   * ══════════════════════════════════════════════════════════
   * 
   * 1. Library prepares pixel data in RAM (GRB byte order)
   * 2. Data is written to the PIO TX FIFO (32 bits at a time)
   * 3. PIO state machine shifts out each bit with precise timing
   * 4. After all pixels sent, PIO idles (pin LOW > 50µs = reset)
   * 
   * For 16 pixels × 24 bits = 384 bits at 800kHz = 480µs total
   * Less than half a millisecond! PIO handles the timing-critical
   * part while the CPU just dumps data into the FIFO.
   * 
   * ══════════════════════════════════════════════════════════
   * PIO RESOURCES AND LIMITS
   * ══════════════════════════════════════════════════════════
   * 
   * Each PIO block has:
   *   - 4 state machines
   *   - 32 instruction slots (shared among all 4 SMs in that block)
   * 
   * The WS2812 program uses ~7 instructions = plenty of room for
   * other PIO programs in the same block.
   * 
   * With 2 PIO blocks × 4 state machines = 8 independent I/O engines.
   * You could drive 8 separate NeoPixel strips simultaneously!
   * 
   * Or mix: 1 SM for NeoPixels + 1 SM for extra UART + 1 SM for
   * a logic analyser + 1 SM for rotary encoder... all in hardware.
   * 
   * ══════════════════════════════════════════════════════════
   * PIO vs DMA vs INTERRUPTS vs POLLING
   * ══════════════════════════════════════════════════════════
   * 
   * Polling: CPU manually toggles pins → blocks everything
   *   Used on: AVR for WS2812
   * 
   * Interrupts: CPU services events → responsive but ISR overhead
   *   Used on: everything, but not precise enough for WS2812
   * 
   * DMA: Hardware moves data memory→peripheral → CPU free
   *   Used on: STM32, ESP32 for SPI/UART/Timer
   *   But DMA still needs a peripheral to do the actual I/O
   * 
   * PIO: Programmable hardware that IS the peripheral → CPU free,
   *   no existing peripheral needed. You CREATE the peripheral.
   *   Only on: RP2040
   * 
   * PIO fills a gap that no other MCU addresses:
   * custom, timing-critical I/O protocols in hardware.
   */
}
