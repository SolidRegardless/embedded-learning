/*
 * LESSON 3: Serial / UART Communication
 * 
 * CONCEPTS:
 * - UART (Universal Asynchronous Receiver-Transmitter)
 * - Baud rate: both sides must agree on speed (115200 bits/sec here)
 * - TX (transmit) and RX (receive) lines
 * - Serial monitor: your debug lifeline in embedded development
 * 
 * WHAT IS UART?
 * The simplest communication protocol. Two wires: TX and RX.
 * Data is sent one bit at a time at an agreed speed (baud rate).
 * No clock wire needed — both sides just agree on timing.
 * 
 * Frame format: [START bit] [8 data bits] [STOP bit]
 * At 115200 baud, each bit is ~8.7 microseconds.
 * 
 * On ESP32, Serial uses UART0 (USB connection to your PC).
 * Serial1 and Serial2 are additional hardware UARTs you can 
 * connect to other devices (GPS modules, Bluetooth, other MCUs).
 * 
 * WHY THIS MATTERS:
 * In embedded, you can't just printf and see output on screen.
 * Serial/UART is how you debug — it's your equivalent of console.log().
 * Professional firmware uses UART for logging, configuration, and
 * inter-device communication.
 */

const int POT_PIN = 34;    // GPIO34 — ADC input (analog read)
const int LED_PIN = 2;

// Simple command parser state
String inputBuffer = "";

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(POT_PIN, INPUT);
  
  // Wait a moment for serial to connect
  delay(100);
  
  Serial.println("========================================");
  Serial.println("  Lesson 3: Serial Communication");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  ON      — turn LED on");
  Serial.println("  OFF     — turn LED off");
  Serial.println("  READ    — read potentiometer value");
  Serial.println("  STATUS  — show system info");
  Serial.println();
  Serial.print("> ");
}

void loop() {
  // Check if data arrived on the serial port
  while (Serial.available() > 0) {
    char c = Serial.read();  // Read one byte from UART receive buffer
    
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
        Serial.print("> ");
      }
    } else {
      inputBuffer += c;
      Serial.print(c);  // Echo back (like a terminal)
    }
  }
  
  // Non-blocking: we don't delay() here, we just keep checking
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  
  Serial.println();  // Newline after the echoed command
  
  if (cmd == "ON") {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED turned ON");
    
  } else if (cmd == "OFF") {
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED turned OFF");
    
  } else if (cmd == "READ") {
    // ADC (Analog-to-Digital Converter) reads voltage as a number
    // ESP32 ADC: 12-bit = 0-4095 (0V to 3.3V)
    int raw = analogRead(POT_PIN);
    float voltage = raw * (3.3 / 4095.0);
    
    Serial.print("Potentiometer: raw=");
    Serial.print(raw);
    Serial.print(" voltage=");
    Serial.print(voltage, 2);  // 2 decimal places
    Serial.println("V");
    
  } else if (cmd == "STATUS") {
    Serial.println("--- System Status ---");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.print("CPU freq: ");
    Serial.print(ESP.getCpuFreqMHz());
    Serial.println(" MHz");
    Serial.print("Chip model: ");
    Serial.println(ESP.getChipModel());
    
  } else {
    Serial.print("Unknown command: '");
    Serial.print(cmd);
    Serial.println("' — try ON, OFF, READ, STATUS");
  }
}

/*
 * KEY EMBEDDED CONCEPTS IN THIS LESSON:
 * 
 * 1. UART BUFFERING: Serial data arrives in a hardware FIFO buffer.
 *    Serial.available() tells you how many bytes are waiting.
 *    If you don't read fast enough, the buffer overflows and data is lost.
 * 
 * 2. ADC (Analog-to-Digital Converter): Converts continuous voltage 
 *    (0-3.3V) into a discrete number (0-4095 on ESP32's 12-bit ADC).
 *    Resolution: 3.3V / 4096 = ~0.8mV per step.
 * 
 * 3. ESP.getFreeHeap(): In embedded, you track memory obsessively.
 *    There's no swap file, no virtual memory. When RAM is gone, 
 *    your firmware crashes or behaves unpredictably.
 */
