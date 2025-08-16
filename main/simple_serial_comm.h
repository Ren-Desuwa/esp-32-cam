#ifndef SIMPLE_SERIAL_COMM_H
#define SIMPLE_SERIAL_COMM_H

#include <Arduino.h>

// ========================================
// SIMPLE GENERIC SENDER
// ========================================

class SimpleSerialSender {
private:
  HardwareSerial* serial;
  
public:
  SimpleSerialSender(HardwareSerial* ser = &Serial) : serial(ser) {}
  
  void begin(unsigned long baud = 115200) {
    serial->begin(baud);
  }
  
  // Send any printable data
  template<typename T>
  void send(const T& data) {
    serial->println(data);
  }
  
  // Send multiple values separated by delimiter
  template<typename T>
  void send(const T& data, const char* delimiter) {
    serial->print(data);
    serial->print(delimiter);
  }
  
  // Send end marker
  void endTransmission() {
    serial->println("END");
  }
  
  // Send raw bytes
  void sendBytes(const uint8_t* data, size_t length) {
    serial->write(data, length);
  }
  
  // Send string
  void sendString(const String& str) {
    serial->println(str);
  }
};

// ========================================
// SIMPLE GENERIC RECEIVER
// ========================================

class SimpleSerialReceiver {
private:
  HardwareSerial* serial;
  String last_received;
  
public:
  SimpleSerialReceiver(HardwareSerial* ser = &Serial) : serial(ser) {}
  
  void begin(unsigned long baud = 115200) {
    serial->begin(baud);
  }
  
  // Check if data is available
  bool available() {
    return serial->available() > 0;
  }
  
  // Receive a line of data (blocks until received or timeout)
  bool receiveLine(unsigned long timeout_ms = 1000) {
    unsigned long start_time = millis();
    
    while (!serial->available() && (millis() - start_time < timeout_ms)) {
      delay(1);
    }
    
    if (!serial->available()) {
      return false;
    }
    
    last_received = serial->readStringUntil('\n');
    last_received.trim();
    return true;
  }
  
  // Get the last received string
  String getString() {
    return last_received;
  }
  
  // Convert last received to int
  int getInt() {
    return last_received.toInt();
  }
  
  // Convert last received to float
  float getFloat() {
    return last_received.toFloat();
  }
  
  // Check if last received matches a string
  bool equals(const String& str) {
    return last_received.equals(str);
  }
  
  // Parse comma-separated values into array
  int parseCSV(String* values, int max_values) {
    int count = 0;
    int start = 0;
    
    while (start < last_received.length() && count < max_values) {
      int comma = last_received.indexOf(',', start);
      
      if (comma == -1) {
        values[count] = last_received.substring(start);
        values[count].trim();
        count++;
        break;
      } else {
        values[count] = last_received.substring(start, comma);
        values[count].trim();
        count++;
        start = comma + 1;
      }
    }
    
    return count;
  }
  
  // Receive raw bytes
  int receiveBytes(uint8_t* buffer, size_t max_length, unsigned long timeout_ms = 1000) {
    unsigned long start_time = millis();
    int bytes_read = 0;
    
    while (bytes_read < max_length && (millis() - start_time < timeout_ms)) {
      if (serial->available()) {
        buffer[bytes_read] = serial->read();
        bytes_read++;
        start_time = millis(); // Reset timeout on each byte received
      }
      delay(1);
    }
    
    return bytes_read;
  }
};

// ========================================
// USAGE EXAMPLES
// ========================================

/*
// SENDER EXAMPLES:
SimpleSerialSender sender(&Serial2);

void setup() {
  Serial.begin(115200);
  sender.begin(115200);
}

void loop() {
  // Send different types of data
  sender.send(123);                    // Send integer
  sender.send(45.67);                  // Send float
  sender.send("Hello World");          // Send string
  sender.send(true);                   // Send boolean (as 1/0)
  
  // Send comma-separated values
  sender.send(10, ",");
  sender.send(20, ",");
  sender.send(30, ",");
  sender.endTransmission();            // End with "END"
  
  // Send custom formatted string
  String data = "R0,B1,100,200,300";
  sender.sendString(data);
  
  delay(1000);
}

// RECEIVER EXAMPLES:
SimpleSerialReceiver receiver(&Serial2);

void setup() {
  Serial.begin(115200);
  receiver.begin(115200);
}

void loop() {
  if (receiver.receiveLine()) {
    String data = receiver.getString();
    Serial.println("Received: " + data);
    
    // Check for specific values
    if (receiver.equals("END")) {
      Serial.println("Transmission ended");
    }
    
    // Parse as different types
    int int_val = receiver.getInt();
    float float_val = receiver.getFloat();
    
    // Parse CSV data
    String values[10];
    int count = receiver.parseCSV(values, 10);
    
    Serial.printf("Parsed %d CSV values:\n", count);
    for (int i = 0; i < count; i++) {
      Serial.printf("  [%d]: %s\n", i, values[i].c_str());
    }
  }
  
  delay(10);
}

// RAW BYTES EXAMPLE:
void sendRawData() {
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  sender.sendBytes(data, sizeof(data));
}

void receiveRawData() {
  uint8_t buffer[100];
  int bytes_received = receiver.receiveBytes(buffer, sizeof(buffer));
  
  Serial.printf("Received %d raw bytes\n", bytes_received);
  for (int i = 0; i < bytes_received; i++) {
    Serial.printf("0x%02X ", buffer[i]);
  }
  Serial.println();
}
*/

#endif // SIMPLE_SERIAL_COMM_H