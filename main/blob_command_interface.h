#ifndef BLOB_COMMAND_INTERFACE_H
#define BLOB_COMMAND_INTERFACE_H

#include "simple_serial_comm.h"
#include "color_threshold_manager.h"
#include "region_manager.h"
#include "blob_detector_ccl.h"
#include <string>
#include <vector>

// ========================================
// EXTERNAL FUNCTIONS FOR MAIN CONTROL
// ========================================
// These need to be implemented in main.cpp
extern void enableCapture();
extern void disableCapture();
extern bool isCaptureEnabled();
// ========================================
// BLOB DETECTION COMMAND INTERFACE
// ========================================

class BlobCommandInterface {
private:
  SimpleSerialReceiver receiver;
  SimpleSerialSender sender;
  
  // Parse helpers
  bool parseInts(const String* tokens, int count, int* values, int expected) {
    if (count < expected) return false;
    for (int i = 0; i < expected; i++) {
      values[i] = tokens[i].toInt();
    }
    return true;
  }
  
  void sendError(const String& message) {
    sender.send("ERROR: " + message);
  }
  
  void sendOK() {
    sender.send("OK");
  }
  
  void sendAck(const String& command) {
    sender.send("ACK:" + command);
  }

public:
  BlobCommandInterface(HardwareSerial* ser = &Serial) : receiver(ser), sender(ser) {}
  
  void begin(unsigned long baud = 115200) {
    receiver.begin(baud);
    sender.begin(baud);
  }
  
  // Process incoming commands
  void processCommands() {
    if (!receiver.receiveLine(10)) return; // Quick 10ms timeout
    
    String input = receiver.getString();
    if (input.length() == 0) return;
    
    String tokens[10];
    int token_count = receiver.parseCSV(tokens, 10);
    if (token_count == 0) return;
    
    String cmd = tokens[0];
    cmd.toUpperCase();
    
    // ========================================
    // SYSTEM CONTROL COMMANDS
    // ========================================
    
    if (cmd == "START") {
      enableCapture();
      sendAck("START");
      sender.send("CAPTURE_STARTED");
      return;
    }
    
    else if (cmd == "STOP") {
      disableCapture();
      sendAck("STOP");
      sender.send("CAPTURE_STOPPED");
      return;
    }
    
    else if (cmd == "STATUS") {
      sendAck("STATUS");
      sendStatus();
      return;
    }
    
    // ========================================
    // COLOR COMMANDS
    // ========================================
    
    else if (cmd == "COLOR_SET") {
      // COLOR_SET,name,h_min,h_max,s_min,s_max,v_min,v_max
      if (token_count < 8) {
        sendAck("COLOR_SET");
        sendError("COLOR_SET needs: name,h_min,h_max,s_min,s_max,v_min,v_max");
        return;
      }
      
      int values[6];
      if (!parseInts(&tokens[2], 6, values, 6)) {
        sendAck("COLOR_SET");
        sendError("Invalid color threshold values");
        return;
      }
      
      ColorThresholds threshold(values[0], values[1], values[2], values[3], values[4], values[5]);
      getColorManager().setColor(std::string(tokens[1].c_str()), threshold);
      sendAck("COLOR_SET," + tokens[1]);
      sendOK();
    }
    
    else if (cmd == "COLOR_SET2") {
      // COLOR_SET2,name,h1_min,h1_max,s1_min,s1_max,v1_min,v1_max,h2_min,h2_max,s2_min,s2_max,v2_min,v2_max
      if (token_count < 14) {
        sendAck("COLOR_SET2");
        sendError("COLOR_SET2 needs: name + 12 threshold values");
        return;
      }
      
      int values[12];
      if (!parseInts(&tokens[2], 12, values, 12)) {
        sendAck("COLOR_SET2");
        sendError("Invalid color threshold values");
        return;
      }
      
      std::vector<ColorThresholds> thresholds = {
        ColorThresholds(values[0], values[1], values[2], values[3], values[4], values[5]),
        ColorThresholds(values[6], values[7], values[8], values[9], values[10], values[11])
      };
      getColorManager().setColor(std::string(tokens[1].c_str()), thresholds);
      sendAck("COLOR_SET2," + tokens[1]);
      sendOK();
    }
    
    else if (cmd == "COLOR_DEL") {
      // COLOR_DEL,name
      if (token_count < 2) {
        sendAck("COLOR_DEL");
        sendError("COLOR_DEL needs: name");
        return;
      }
      
      sendAck("COLOR_DEL," + tokens[1]);
      if (getColorManager().deleteColor(std::string(tokens[1].c_str()))) {
        sendOK();
      } else {
        sendError("Color not found");
      }
    }
    
    else if (cmd == "COLOR_LIST") {
      // COLOR_LIST
      sendAck("COLOR_LIST");
      std::vector<std::string> colors = getColorManager().getAllColorNames();
      sender.send("COLORS");
      for (const auto& color : colors) {
        sender.send(color.c_str());
      }
      sender.endTransmission();
    }
    
    // ========================================
    // REGION COMMANDS
    // ========================================
    
    else if (cmd == "REGION_SET") {
      // REGION_SET,name,x,y,width,height
      if (token_count < 6) {
        sendAck("REGION_SET");
        sendError("REGION_SET needs: name,x,y,width,height");
        return;
      }
      
      int values[4];
      if (!parseInts(&tokens[2], 4, values, 4)) {
        sendAck("REGION_SET");
        sendError("Invalid region values");
        return;
      }
      
      DetectionRegion region(values[0], values[1], values[2], values[3]);
      getRegionManager().setRegionSet(std::string(tokens[1].c_str()), region);
      sendAck("REGION_SET," + tokens[1]);
      sendOK();
    }
    
    else if (cmd == "REGION_MULTI") {
      // REGION_MULTI,name,count,x1,y1,w1,h1,x2,y2,w2,h2,...
      if (token_count < 3) {
        sendAck("REGION_MULTI");
        sendError("REGION_MULTI needs: name,count,regions...");
        return;
      }
      
      int region_count = tokens[2].toInt();
      if (region_count <= 0 || token_count < 3 + (region_count * 4)) {
        sendAck("REGION_MULTI");
        sendError("Invalid region count or insufficient data");
        return;
      }
      
      std::vector<DetectionRegion> regions;
      regions.reserve(region_count);
      
      for (int i = 0; i < region_count; i++) {
        int offset = 3 + (i * 4);
        int values[4];
        if (!parseInts(&tokens[offset], 4, values, 4)) {
          sendAck("REGION_MULTI");
          sendError("Invalid region values");
          return;
        }
        regions.emplace_back(values[0], values[1], values[2], values[3]);
      }
      
      getRegionManager().setRegionSet(std::string(tokens[1].c_str()), regions);
      sendAck("REGION_MULTI," + tokens[1] + "," + String(region_count));
      sendOK();
    }
    
    else if (cmd == "REGION_DEL") {
      // REGION_DEL,name
      if (token_count < 2) {
        sendAck("REGION_DEL");
        sendError("REGION_DEL needs: name");
        return;
      }
      
      sendAck("REGION_DEL," + tokens[1]);
      if (getRegionManager().deleteRegionSet(std::string(tokens[1].c_str()))) {
        sendOK();
      } else {
        sendError("Region set not found");
      }
    }
    
    else if (cmd == "REGION_LIST") {
      // REGION_LIST
      sendAck("REGION_LIST");
      std::vector<std::string> region_sets = getRegionManager().getAllRegionSetNames();
      sender.send("REGIONS");
      for (const auto& set_name : region_sets) {
        sender.send(set_name.c_str());
      }
      sender.endTransmission();
    }
    
    // ========================================
    // BLOB DETECTION COMMANDS
    // ========================================
    
    else if (cmd == "DETECT") {
      // DETECT,region_set,color1,color2,...
      if (token_count < 3) {
        sendAck("DETECT");
        sendError("DETECT needs: region_set,color1,color2,...");
        return;
      }
      
      std::vector<std::string> colors;
      for (int i = 2; i < token_count; i++) {
        colors.push_back(std::string(tokens[i].c_str()));
      }
      
      sendAck("DETECT," + tokens[1] + "," + String(colors.size()));
      sender.send("DETECT_READY");
      sender.send(tokens[1]); // region_set name
      sender.send(String(colors.size())); // number of colors
      for (const auto& color : colors) {
        sender.send(color.c_str());
      }
      sender.endTransmission();
    }
    
    else if (cmd == "DETECT_ALL") {
      // DETECT_ALL,region_set
      if (token_count < 2) {
        sendAck("DETECT_ALL");
        sendError("DETECT_ALL needs: region_set");
        return;
      }
      
      sendAck("DETECT_ALL," + tokens[1]);
      sender.send("DETECT_ALL_READY");
      sender.send(tokens[1]); // region_set name
      sender.endTransmission();
    }
    
    else if (cmd == "PONG") {
      sender.send("PING");
      sendAck("CONNECTION_OK");
    }

    else if (cmd == "PING") {
      sender.send("PONG");
      sendAck("CONNECTION_OK");
    }

    else {
      sendAck(cmd);
      sendError("Unknown command: " + cmd);
    }
  }
  
  // ========================================
  // BLOB DETECTION RESULTS SENDER
  // ========================================
  
  // Send blob detection results
  void sendBlobResults(const std::vector<RegionResults>& results) {
    sender.send("BLOBS_START");
    sender.send(String(results.size())); // number of regions
    
    for (const auto& region_result : results) {
      sender.send("REGION");
      sender.send(String(region_result.region_id));
      
      // Get all colors that have blobs
      std::vector<std::string> colors_with_blobs;
      for (const auto& color_pair : region_result.color_blobs) {
        if (!color_pair.second.empty()) {
          colors_with_blobs.push_back(color_pair.first);
        }
      }
      
      sender.send(String(colors_with_blobs.size())); // number of colors with blobs
      
      for (const std::string& color : colors_with_blobs) {
        const auto& blobs = region_result.getBlobsForColor(color);
        sender.send("COLOR");
        sender.send(color.c_str());
        sender.send(String(blobs.size())); // number of blobs for this color
        
        for (const auto& blob : blobs) {
          // Send: x,y,pixel_count
          String blob_data = String(blob.center_x) + "," + 
                           String(blob.center_y) + "," + 
                           String(blob.pixel_count);
          sender.send(blob_data);
        }
      }
    }
    
    sender.send("BLOBS_END");
  }
  
  // Simplified blob result sender (just coordinates)
  void sendSimpleBlobResults(const std::vector<RegionResults>& results) {
    for (const auto& region_result : results) {
      for (const auto& color_pair : region_result.color_blobs) {
        if (!color_pair.second.empty()) {
          for (const auto& blob : color_pair.second) {
            // Format: R{region_id},{color},{x},{y},{size}
            String result = "R" + String(region_result.region_id) + "," +
                           String(color_pair.first.c_str()) + "," +
                           String(blob.center_x) + "," +
                           String(blob.center_y) + "," +
                           String(blob.pixel_count);
            sender.send(result);
          }
        }
      }
    }
    sender.endTransmission();
  }
  
  // ========================================
  // HSV DATA SENDER
  // ========================================
  
  // Send HSV data for specified regions
  void sendHSVData(const HSVImage& hsv, const std::string& region_set_name) {
    // Get regions
    const auto& regions = getRegionManager().getRegions(region_set_name);
    if (regions.empty()) {
      sendError(String("No regions found for set: ") + region_set_name.c_str());
      return;
    }
    
    sender.send("HSV_START");
    sender.send(String(regions.size())); // number of regions
    
    for (size_t region_id = 0; region_id < regions.size(); region_id++) {
      const DetectionRegion& region = regions[region_id];
      
      // Validate region bounds
      int x_end = min(region.x + region.width, hsv.width);
      int y_end = min(region.y + region.height, hsv.height);
      int x_start = max(region.x, 0);
      int y_start = max(region.y, 0);
      
      sender.send("REGION");
      sender.send(String(region_id));
      sender.send(String(x_start) + "," + String(y_start) + "," + 
                 String(x_end - x_start) + "," + String(y_end - y_start));
      
      // Send HSV data row by row
      for (int y = y_start; y < y_end; y++) {
        String row_data = "";
        for (int x = x_start; x < x_end; x++) {
          int idx = y * hsv.width + x;
          
          // Format: H,S,V (space-separated pixels)
          if (row_data.length() > 0) row_data += " ";
          row_data += String(hsv.h_data[idx]) + "," + 
                     String(hsv.s_data[idx]) + "," + 
                     String(hsv.v_data[idx]);
          
          // Prevent buffer overflow - send row if getting too long
          if (row_data.length() > 200) {
            sender.send(row_data);
            row_data = "";
          }
        }
        
        // Send remaining data for this row
        if (row_data.length() > 0) {
          sender.send(row_data);
        }
      }
    }
    
    sender.send("HSV_END");
  }
  
  // ========================================
  // CONVENIENCE METHODS
  // ========================================
  
  // Perform detection and send results
  void detectAndSend(const HSVImage& hsv, const std::string& region_set_name, 
                    const std::vector<std::string>& colors, bool simple_format = false) {
    auto results = detectBlobsStructured(hsv, region_set_name, colors);
    if (simple_format) {
      sendSimpleBlobResults(results);
    } else {
      sendBlobResults(results);
    }
  }
  
  // Detect all colors and send results
  void detectAllAndSend(const HSVImage& hsv, const std::string& region_set_name, bool simple_format = false) {
    auto results = detectAllColorsStructured(hsv, region_set_name);
    if (simple_format) {
      sendSimpleBlobResults(results);
    } else {
      sendBlobResults(results);
    }
  }
  
  // Send status info
  void sendStatus() {
    sender.send("STATUS");
    sender.send("Capture: " + String(isCaptureEnabled() ? "ON" : "OFF"));
    sender.send("Colors: " + String(getColorManager().getAllColorNames().size()));
    sender.send("Regions: " + String(getRegionManager().getAllRegionSetNames().size()));
    sender.endTransmission();
  }
};

// ========================================
// GLOBAL ACCESS
// ========================================

inline BlobCommandInterface& getCommandInterface() {
  static BlobCommandInterface instance;
  return instance;
}

#endif // BLOB_COMMAND_INTERFACE_H