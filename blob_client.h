#ifndef BLOB_CLIENT_H
#define BLOB_CLIENT_H

#include <Arduino.h>
#include <vector>
#include <string>

// ========================================
// CAMERA CLIENT
// ========================================

struct BlobResult {
  int region_id;
  std::string color;
  int x, y;
  int size;
  
  BlobResult(int r_id, const std::string& c, int px, int py, int s) 
    : region_id(r_id), color(c), x(px), y(py), size(s) {}
};

struct HSVPixel {
  uint8_t h, s, v;
  HSVPixel(uint8_t hue, uint8_t sat, uint8_t val) : h(hue), s(sat), v(val) {}
};

struct HSVRegionData {
  int region_id;
  int x, y, width, height;
  std::vector<std::vector<HSVPixel>> pixels; // [row][col]
};

class Camera {
private:
  HardwareSerial* serial;
  String last_response;
  bool debug_enabled;
  
  // Send command and wait for ACK
  bool sendCommand(const String& command) {
    serial->println(command);
    return waitForResponse("ACK:" + command.substring(0, command.indexOf(',')), 5000);
  }
  
  // Wait for specific response
  bool waitForResponse(const String& expected, unsigned long timeout_ms = 5000) {
    unsigned long start = millis();
    while (millis() - start < timeout_ms) {
      if (serial->available()) {
        last_response = serial->readStringUntil('\n');
        last_response.trim();
        
        if (debug_enabled) {
          Serial.println("RX: " + last_response);
        }
        
        if (last_response.startsWith(expected)) {
          return true;
        }
        
        if (last_response.startsWith("ERROR:")) {
          if (debug_enabled) {
            Serial.println("Server Error: " + last_response.substring(6));
          }
          return false;
        }
      }
      delay(1);
    }
    return false;
  }
  
  // Read lines until END or timeout
  std::vector<String> readUntilEnd(unsigned long timeout_ms = 10000) {
    std::vector<String> lines;
    unsigned long start = millis();
    
    while (millis() - start < timeout_ms) {
      if (serial->available()) {
        String line = serial->readStringUntil('\n');
        line.trim();
        
        if (debug_enabled) {
          Serial.println("RX: " + line);
        }
        
        if (line == "END") {
          break;
        }
        
        lines.push_back(line);
      }
      delay(1);
    }
    
    return lines;
  }
  
  // Parse CSV line into tokens
  std::vector<String> parseCSV(const String& line) {
    std::vector<String> tokens;
    int start = 0;
    
    while (start < line.length()) {
      int comma = line.indexOf(',', start);
      if (comma == -1) {
        tokens.push_back(line.substring(start));
        break;
      } else {
        tokens.push_back(line.substring(start, comma));
        start = comma + 1;
      }
    }
    
    return tokens;
  }

public:
  Camera(HardwareSerial* ser = &Serial1) : serial(ser), debug_enabled(false) {}
  
  // ========================================
  // INITIALIZATION
  // ========================================
  
  void begin(unsigned long baud = 115200, bool debug = false) {
    serial->begin(baud);
    debug_enabled = debug;
    
    if (debug_enabled) {
      Serial.println("Blob Detection Client Started");
    }
    
    delay(1000); // Give server time to initialize
  }
  
  void enableDebug(bool enabled = true) {
    debug_enabled = enabled;
  }
  
  // ========================================
  // SYSTEM CONTROL
  // ========================================
  
  bool startCapture() {
    if (debug_enabled) Serial.println("Starting capture...");
    return sendCommand("START") && waitForResponse("CAPTURE_STARTED");
  }
  
  bool stopCapture() {
    if (debug_enabled) Serial.println("Stopping capture...");
    return sendCommand("STOP") && waitForResponse("CAPTURE_STOPPED");
  }
  
  bool getStatus() {
    if (debug_enabled) Serial.println("Getting status...");
    if (!sendCommand("STATUS")) return false;
    
    if (waitForResponse("STATUS")) {
      auto lines = readUntilEnd();
      if (debug_enabled) {
        Serial.println("=== STATUS ===");
        for (const auto& line : lines) {
          Serial.println(line);
        }
        Serial.println("=============");
      }
      return true;
    }
    return false;
  }
  
  // ========================================
  // COLOR MANAGEMENT
  // ========================================
  
  bool setColor(const String& name, int h_min, int h_max, int s_min, int s_max, int v_min, int v_max) {
    String command = "COLOR_SET," + name + "," + 
                    String(h_min) + "," + String(h_max) + "," +
                    String(s_min) + "," + String(s_max) + "," +
                    String(v_min) + "," + String(v_max);
    
    if (debug_enabled) Serial.println("Setting color: " + name);
    return sendCommand(command) && waitForResponse("OK");
  }
  
  bool setColorDual(const String& name, 
                   int h1_min, int h1_max, int s1_min, int s1_max, int v1_min, int v1_max,
                   int h2_min, int h2_max, int s2_min, int s2_max, int v2_min, int v2_max) {
    String command = "COLOR_SET2," + name + "," + 
                    String(h1_min) + "," + String(h1_max) + "," + String(s1_min) + "," + String(s1_max) + "," + String(v1_min) + "," + String(v1_max) + "," +
                    String(h2_min) + "," + String(h2_max) + "," + String(s2_min) + "," + String(s2_max) + "," + String(v2_min) + "," + String(v2_max);
    
    if (debug_enabled) Serial.println("Setting dual-range color: " + name);
    return sendCommand(command) && waitForResponse("OK");
  }
  
  bool deleteColor(const String& name) {
    String command = "COLOR_DEL," + name;
    if (debug_enabled) Serial.println("Deleting color: " + name);
    return sendCommand(command) && waitForResponse("OK");
  }
  
  std::vector<String> listColors() {
    std::vector<String> colors;
    
    if (debug_enabled) Serial.println("Listing colors...");
    if (!sendCommand("COLOR_LIST")) return colors;
    
    if (waitForResponse("COLORS")) {
      colors = readUntilEnd();
    }
    
    return colors;
  }
  
  // ========================================
  // REGION MANAGEMENT
  // ========================================
  
  bool setRegion(const String& name, int x, int y, int width, int height) {
    String command = "REGION_SET," + name + "," + 
                    String(x) + "," + String(y) + "," + 
                    String(width) + "," + String(height);
    
    if (debug_enabled) Serial.println("Setting region: " + name);
    return sendCommand(command) && waitForResponse("OK");
  }
  
  bool setMultiRegion(const String& name, const std::vector<std::vector<int>>& regions) {
    String command = "REGION_MULTI," + name + "," + String(regions.size());
    
    for (const auto& region : regions) {
      if (region.size() != 4) return false; // Must have x,y,w,h
      command += "," + String(region[0]) + "," + String(region[1]) + "," + 
                String(region[2]) + "," + String(region[3]);
    }
    
    if (debug_enabled) Serial.println("Setting multi-region: " + name);
    return sendCommand(command) && waitForResponse("OK");
  }
  
  bool deleteRegion(const String& name) {
    String command = "REGION_DEL," + name;
    if (debug_enabled) Serial.println("Deleting region: " + name);
    return sendCommand(command) && waitForResponse("OK");
  }
  
  std::vector<String> listRegions() {
    std::vector<String> regions;
    
    if (debug_enabled) Serial.println("Listing regions...");
    if (!sendCommand("REGION_LIST")) return regions;
    
    if (waitForResponse("REGIONS")) {
      regions = readUntilEnd();
    }
    
    return regions;
  }
  
  // ========================================
  // BLOB DETECTION
  // ========================================
  
  std::vector<BlobResult> detect(const String& region_name, const std::vector<String>& colors) {
    std::vector<BlobResult> results;
    
    String command = "DETECT," + region_name;
    for (const auto& color : colors) {
      command += "," + color;
    }
    
    if (debug_enabled) Serial.println("Detecting blobs in region: " + region_name);
    if (!sendCommand(command)) return results;
    
    if (!waitForResponse("DETECT_READY")) return results;
    
    // Read detection setup confirmation
    auto setup_lines = readUntilEnd(3000);
    
    // Wait for blob results
    if (waitForResponse("BLOBS_START", 10000)) {
      auto lines = readUntilEnd(15000);
      results = parseBlobResults(lines);
    }
    
    return results;
  }
  
  std::vector<BlobResult> detectAll(const String& region_name) {
    std::vector<BlobResult> results;
    
    String command = "DETECT_ALL," + region_name;
    
    if (debug_enabled) Serial.println("Detecting all colors in region: " + region_name);
    if (!sendCommand(command)) return results;
    
    if (!waitForResponse("DETECT_ALL_READY")) return results;
    
    // Read detection setup confirmation
    auto setup_lines = readUntilEnd(3000);
    
    // Wait for blob results
    if (waitForResponse("BLOBS_START", 10000)) {
      auto lines = readUntilEnd(15000);
      results = parseBlobResults(lines);
    }
    
    return results;
  }
  
  // ========================================
  // HSV SERVER MODE
  // ========================================
  
  bool startServer(const String& region_name) {
    String command = "SERVER," + region_name;
    if (debug_enabled) Serial.println("Starting HSV server for region: " + region_name);
    return sendCommand(command) && waitForResponse("SERVER_STARTED");
  }
  
  std::vector<HSVRegionData> getHSVData(unsigned long timeout_ms = 10000) {
    std::vector<HSVRegionData> hsv_data;
    
    if (waitForResponse("HSV_START", timeout_ms)) {
      auto lines = readUntilEnd(timeout_ms);
      hsv_data = parseHSVData(lines);
    }
    
    return hsv_data;
  }
  
  // ========================================
  // CONVENIENCE METHODS
  // ========================================
  
  // Quick setup for common colors
  void setupDefaultColors() {
    setColor("RED", 0, 10, 50, 255, 50, 255);
    setColorDual("RED_FULL", 0, 10, 50, 255, 50, 255, 160, 179, 50, 255, 50, 255);
    setColor("GREEN", 40, 80, 50, 255, 50, 255);
    setColor("BLUE", 100, 130, 50, 255, 50, 255);
    setColor("YELLOW", 20, 30, 50, 255, 50, 255);
    setColor("BLACK", 0, 179, 0, 255, 0, 50);
    setColor("WHITE", 0, 179, 0, 50, 200, 255);
  }
  
  // Quick region setup
  bool setupFullScreen(const String& name = "FULL", int width = 640, int height = 480) {
    return setRegion(name, 0, 0, width, height);
  }
  
  bool setupQuadrants(const String& base_name = "Q", int width = 640, int height = 480) {
    int hw = width / 2;
    int hh = height / 2;
    
    std::vector<std::vector<int>> quadrants = {
      {0, 0, hw, hh},      // Top-left
      {hw, 0, hw, hh},     // Top-right  
      {0, hh, hw, hh},     // Bottom-left
      {hw, hh, hw, hh}     // Bottom-right
    };
    
    return setMultiRegion(base_name, quadrants);
  }
  
  // Simple detection with single color
  std::vector<BlobResult> findColor(const String& region_name, const String& color) {
    return detect(region_name, {color});
  }

private:
  // ========================================
  // RESULT PARSERS
  // ========================================
  
  std::vector<BlobResult> parseBlobResults(const std::vector<String>& lines) {
    std::vector<BlobResult> results;
    
    int i = 0;
    while (i < lines.size()) {
      if (lines[i] == "BLOBS_END") break;
      
      if (lines[i].startsWith("R") && lines[i].indexOf(',') > 0) {
        // Simple format: R{region_id},{color},{x},{y},{size}
        auto tokens = parseCSV(lines[i].substring(1)); // Remove 'R'
        if (tokens.size() >= 5) {
          results.emplace_back(tokens[0].toInt(), tokens[1].c_str(), 
                             tokens[2].toInt(), tokens[3].toInt(), tokens[4].toInt());
        }
        i++;
      }
      else if (i + 1 < lines.size() && lines[i+1] == "REGION") {
        // Structured format parsing would go here
        // Skip for now - implement if needed
        i += 2;
      }
      else {
        i++;
      }
    }
    
    return results;
  }
  
  std::vector<HSVRegionData> parseHSVData(const std::vector<String>& lines) {
    std::vector<HSVRegionData> hsv_regions;
    
    int i = 0;
    while (i < lines.size() && lines[i] != "HSV_END") {
      if (lines[i] == "REGION" && i + 2 < lines.size()) {
        HSVRegionData region_data;
        region_data.region_id = lines[i+1].toInt();
        
        // Parse region bounds: x,y,width,height
        auto bounds = parseCSV(lines[i+2]);
        if (bounds.size() >= 4) {
          region_data.x = bounds[0].toInt();
          region_data.y = bounds[1].toInt();
          region_data.width = bounds[2].toInt();
          region_data.height = bounds[3].toInt();
          
          // Initialize pixel array
          region_data.pixels.resize(region_data.height);
          for (auto& row : region_data.pixels) {
            row.reserve(region_data.width);
          }
          
          i += 3;
          
          // Read HSV pixel data
          int row = 0;
          while (i < lines.size() && lines[i] != "REGION" && lines[i] != "HSV_END") {
            if (row >= region_data.height) break;
            
            // Parse HSV pixels: "H,S,V H,S,V H,S,V..."
            String line = lines[i];
            int pos = 0;
            
            while (pos < line.length() && region_data.pixels[row].size() < region_data.width) {
              int space = line.indexOf(' ', pos);
              String pixel_str = (space == -1) ? line.substring(pos) : line.substring(pos, space);
              
              auto hsv_tokens = parseCSV(pixel_str);
              if (hsv_tokens.size() >= 3) {
                region_data.pixels[row].emplace_back(
                  hsv_tokens[0].toInt(), hsv_tokens[1].toInt(), hsv_tokens[2].toInt()
                );
              }
              
              pos = (space == -1) ? line.length() : space + 1;
            }
            
            if (region_data.pixels[row].size() >= region_data.width) {
              row++;
            }
            
            i++;
          }
          
          hsv_regions.push_back(region_data);
        } else {
          i += 3;
        }
      } else {
        i++;
      }
    }
    
    return hsv_regions;
  }
};

// ========================================
// GLOBAL ACCESS
// ========================================

inline Camera& getCamera() {
  static Camera instance;
  return instance;
}

#endif // BLOB_CLIENT_H