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

#endif // SIMPLE_SERIAL_COMM_H