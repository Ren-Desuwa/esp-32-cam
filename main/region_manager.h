#ifndef REGION_MANAGER_H
#define REGION_MANAGER_H

#include <unordered_map>
#include <string>
#include <vector>

// ========================================
// DETECTION REGION STRUCTURE
// ========================================

struct DetectionRegion {
  int x, y, width, height;
  
  DetectionRegion(int _x, int _y, int _w, int _h) : x(_x), y(_y), width(_w), height(_h) {}
  
  bool contains(int px, int py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};

// ========================================
// REGION MANAGER CLASS
// ========================================

class RegionManager {
private:
  std::unordered_map<std::string, std::vector<DetectionRegion>> region_sets;
  
public:
  RegionManager() {}
  
  // ========================================
  // ESSENTIAL OPERATIONS
  // ========================================
  
  // Create/Add region set
  void setRegionSet(const std::string& set_name, const DetectionRegion& region) {
    region_sets[set_name] = {region};
  }
  
  void setRegionSet(const std::string& set_name, const std::vector<DetectionRegion>& regions) {
    region_sets[set_name] = regions;
  }
  
  // Edit existing region set
  bool editRegionSet(const std::string& set_name, const DetectionRegion& region) {
    auto it = region_sets.find(set_name);
    if (it != region_sets.end()) {
      it->second = {region};
      return true;
    }
    return false;
  }
  
  bool editRegionSet(const std::string& set_name, const std::vector<DetectionRegion>& regions) {
    auto it = region_sets.find(set_name);
    if (it != region_sets.end()) {
      it->second = regions;
      return true;
    }
    return false;
  }
  
  // Delete region set
  bool deleteRegionSet(const std::string& set_name) {
    return region_sets.erase(set_name) > 0;
  }
  
  // ========================================
  // ACCESS FOR BLOB DETECTOR
  // ========================================
  
  // Check if region set exists
  bool hasRegionSet(const std::string& set_name) const {
    return region_sets.find(set_name) != region_sets.end();
  }
  
  // Get regions from set
  const std::vector<DetectionRegion>& getRegions(const std::string& set_name) const {
    auto it = region_sets.find(set_name);
    if (it != region_sets.end()) {
      return it->second;
    }
    static const std::vector<DetectionRegion> empty_vector;
    return empty_vector;
  }
  
  // Get all region set names
  std::vector<std::string> getAllRegionSetNames() const {
    std::vector<std::string> names;
    names.reserve(region_sets.size());
    for (const auto& pair : region_sets) {
      names.push_back(pair.first);
    }
    return names;
  }
};

// ========================================
// GLOBAL ACCESS
// ========================================

inline RegionManager& getRegionManager() {
  static RegionManager instance;
  return instance;
}

#endif // REGION_MANAGER_H