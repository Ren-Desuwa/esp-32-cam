#ifndef COLOR_THRESHOLD_MANAGER_H
#define COLOR_THRESHOLD_MANAGER_H

#include <unordered_map>
#include <string>
#include <vector>

// ========================================
// COLOR THRESHOLD STRUCTURE
// ========================================

struct ColorThresholds {
  uint8_t h_min, h_max;
  uint8_t s_min, s_max;
  uint8_t v_min, v_max;
  
  ColorThresholds(uint8_t hmin, uint8_t hmax, uint8_t smin, uint8_t smax, uint8_t vmin, uint8_t vmax)
    : h_min(hmin), h_max(hmax), s_min(smin), s_max(smax), v_min(vmin), v_max(vmax) {}
    
  ColorThresholds() : h_min(0), h_max(179), s_min(0), s_max(255), v_min(0), v_max(255) {}
};

// ========================================
// COLOR THRESHOLD MANAGER CLASS
// ========================================

class ColorThresholdManager {
private:
  std::unordered_map<std::string, std::vector<ColorThresholds>> color_map;
  
  void initializeDefaults() {
    // BLACK
    color_map["BLACK"] = {ColorThresholds(0, 179, 0, 255, 0, 50)};
    
    // WHITE  
    color_map["WHITE"] = {ColorThresholds(0, 179, 0, 50, 200, 255)};
    
    // RED (two ranges due to hue wraparound)
    color_map["RED"] = {
      ColorThresholds(0, 10, 50, 255, 50, 255),
      ColorThresholds(160, 179, 50, 255, 50, 255)
    };
    
    // GREEN
    color_map["GREEN"] = {ColorThresholds(40, 80, 50, 255, 50, 255)};
  }
  
public:
  ColorThresholdManager() {
    initializeDefaults();
  }
  
  // ========================================
  // ESSENTIAL OPERATIONS
  // ========================================
  
  // Create/Add color
  void setColor(const std::string& color_name, const ColorThresholds& threshold) {
    color_map[color_name] = {threshold};
  }
  
  void setColor(const std::string& color_name, const std::vector<ColorThresholds>& thresholds) {
    color_map[color_name] = thresholds;
  }
  
  // Edit existing color
  bool editColor(const std::string& color_name, const ColorThresholds& threshold) {
    auto it = color_map.find(color_name);
    if (it != color_map.end()) {
      it->second = {threshold};
      return true;
    }
    return false;
  }
  
  bool editColor(const std::string& color_name, const std::vector<ColorThresholds>& thresholds) {
    auto it = color_map.find(color_name);
    if (it != color_map.end()) {
      it->second = thresholds;
      return true;
    }
    return false;
  }
  
  // Delete color
  bool deleteColor(const std::string& color_name) {
    return color_map.erase(color_name) > 0;
  }
  
  // ========================================
  // ACCESS FOR BLOB DETECTOR
  // ========================================
  
  // Check if color exists
  bool hasColor(const std::string& color_name) const {
    return color_map.find(color_name) != color_map.end();
  }
  
  // Match HSV values against color
  bool matchesColor(uint8_t h, uint8_t s, uint8_t v, const std::string& color_name) const {
    auto it = color_map.find(color_name);
    if (it == color_map.end()) return false;
    
    for (const auto& threshold : it->second) {
      if (h >= threshold.h_min && h <= threshold.h_max &&
          s >= threshold.s_min && s <= threshold.s_max &&
          v >= threshold.v_min && v <= threshold.v_max) {
        return true;
      }
    }
    return false;
  }
  
  // Get all color names
  std::vector<std::string> getAllColorNames() const {
    std::vector<std::string> names;
    names.reserve(color_map.size());
    for (const auto& pair : color_map) {
      names.push_back(pair.first);
    }
    return names;
  }
};

// ========================================
// GLOBAL ACCESS
// ========================================

inline ColorThresholdManager& getColorManager() {
  static ColorThresholdManager instance;
  return instance;
}

#endif // COLOR_THRESHOLD_MANAGER_H