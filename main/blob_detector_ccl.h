#ifndef BLOB_DETECTOR_CCL_H
#define BLOB_DETECTOR_CCL_H

#include "simple_converter.h"
#include "color_threshold_manager.h"
#include "region_manager.h"
#include <vector>


// ========================================
// CORE STRUCTURES
// ========================================

struct Blob {
  int center_x;
  int center_y;
  int pixel_count;
  
  Blob() : center_x(0), center_y(0), pixel_count(0) {}
  Blob(int x, int y, int count) : center_x(x), center_y(y), pixel_count(count) {}
};

// ========================================
// REGION RESULTS STRUCTURE
// ========================================

struct RegionResults {
  int region_id;
  std::unordered_map<std::string, std::vector<Blob>> color_blobs;
  
  RegionResults(int id) : region_id(id) {}
  
  std::vector<Blob>& getBlobsForColor(const std::string& color) {
    return color_blobs[color];
  }
  
  const std::vector<Blob>& getBlobsForColor(const std::string& color) const {
    auto it = color_blobs.find(color);
    if (it != color_blobs.end()) {
      return it->second;
    }
    static const std::vector<Blob> empty_vector;
    return empty_vector;
  }
};

// ========================================
// UNION-FIND FOR CCL
// ========================================

class UnionFind {
private:
  uint16_t* parent;
  uint16_t* rank;
  uint16_t max_labels;
  
public:
  UnionFind(uint16_t max_size) : max_labels(max_size) {
    parent = new uint16_t[max_size];
    rank = new uint16_t[max_size];
    for (uint16_t i = 0; i < max_size; i++) {
      parent[i] = i;
      rank[i] = 0;
    }
  }
  
  ~UnionFind() {
    delete[] parent;
    delete[] rank;
  }
  
  uint16_t find(uint16_t x) {
    if (parent[x] != x) {
      parent[x] = find(parent[x]);
    }
    return parent[x];
  }
  
  void unite(uint16_t x, uint16_t y) {
    uint16_t root_x = find(x);
    uint16_t root_y = find(y);
    
    if (root_x != root_y) {
      if (rank[root_x] < rank[root_y]) {
        parent[root_x] = root_y;
      } else if (rank[root_x] > rank[root_y]) {
        parent[root_y] = root_x;
      } else {
        parent[root_y] = root_x;
        rank[root_x]++;
      }
    }
  }
};

// ========================================
// BLOB STATISTICS COLLECTOR
// ========================================

struct BlobStats {
  int sum_x;
  int sum_y;
  int count;
  
  BlobStats() : sum_x(0), sum_y(0), count(0) {}
  
  void add(int x, int y) {
    sum_x += x;
    sum_y += y;
    count++;
  }
  
  Blob toBlob() const {
    if (count > 0) {
      return Blob(sum_x / count, sum_y / count, count);
    }
    return Blob();
  }
};

// ========================================
// CORE CCL BLOB DETECTION
// ========================================

inline std::vector<Blob> detectSingleColorCCL(const HSVImage& hsv, const DetectionRegion& region,
                                              const std::string& color_name, int min_size = 10) {
  if (!hsv.isValid() || !getColorManager().hasColor(color_name)) return {};
  
  const int region_width = region.width;
  const int region_height = region.height;
  const int region_pixels = region_width * region_height;
  
  if (region_pixels == 0) return {};
  
  uint8_t* mask = new uint8_t[region_pixels];
  uint16_t* labels = new uint16_t[region_pixels];
  
  // Create binary mask
  int valid_pixels = 0;
  for (int ry = 0; ry < region_height; ry++) {
    int img_y = region.y + ry;
    if (img_y >= hsv.height) break;
    
    for (int rx = 0; rx < region_width; rx++) {
      int img_x = region.x + rx;
      if (img_x >= hsv.width) break;
      
      int img_idx = img_y * hsv.width + img_x;
      int region_idx = ry * region_width + rx;
      
      uint8_t h = hsv.h_data[img_idx];
      uint8_t s = hsv.s_data[img_idx];
      uint8_t v = hsv.v_data[img_idx];
      
      bool matches = getColorManager().matchesColor(h, s, v, color_name);
      mask[region_idx] = matches ? 1 : 0;
      labels[region_idx] = 0;
      if (matches) valid_pixels++;
    }
  }
  
  if (valid_pixels == 0) {
    delete[] mask;
    delete[] labels;
    return {};
  }
  
  // Two-pass CCL
  uint16_t next_label = 1;
  const uint16_t max_labels = (region_pixels / 4) + 1;
  UnionFind uf(max_labels);
  
  // Pass 1: Initial labeling
  for (int ry = 0; ry < region_height; ry++) {
    for (int rx = 0; rx < region_width; rx++) {
      int idx = ry * region_width + rx;
      
      if (mask[idx] == 0) continue;
      
      uint16_t min_neighbor_label = 0;
      
      if (rx > 0 && mask[idx - 1] && labels[idx - 1] > 0) {
        min_neighbor_label = labels[idx - 1];
      }
      
      if (ry > 0 && mask[idx - region_width] && labels[idx - region_width] > 0) {
        if (min_neighbor_label == 0) {
          min_neighbor_label = labels[idx - region_width];
        } else if (labels[idx - region_width] != min_neighbor_label) {
          uf.unite(min_neighbor_label, labels[idx - region_width]);
        }
      }
      
      if (min_neighbor_label == 0) {
        labels[idx] = next_label++;
        if (next_label >= max_labels) break;
      } else {
        labels[idx] = min_neighbor_label;
      }
    }
    if (next_label >= max_labels) break;
  }
  
  // Pass 2: Collect statistics
  BlobStats* stats = new BlobStats[next_label];
  
  for (int ry = 0; ry < region_height; ry++) {
    for (int rx = 0; rx < region_width; rx++) {
      int idx = ry * region_width + rx;
      
      if (labels[idx] > 0) {
        uint16_t root_label = uf.find(labels[idx]);
        int img_x = region.x + rx;
        int img_y = region.y + ry;
        stats[root_label].add(img_x, img_y);
      }
    }
  }
  
  // Create result blobs
  std::vector<Blob> blobs;
  for (uint16_t i = 1; i < next_label; i++) {
    if (stats[i].count >= min_size) {
      blobs.push_back(stats[i].toBlob());
    }
  }
  
  delete[] mask;
  delete[] labels;
  delete[] stats;
  
  return blobs;
}

// ========================================
// MAIN DETECTION FUNCTIONS
// ========================================

// Using region set name from RegionManager
inline std::vector<RegionResults> detectBlobsStructured(
    const HSVImage& hsv,
    const std::string& region_set_name,
    const std::vector<std::string>& colors_to_detect,
    bool multi_blob_per_color = true,
    int min_size = 10) {
  
  if (!getRegionManager().hasRegionSet(region_set_name)) {
    return {};
  }
  
  const std::vector<DetectionRegion>& regions = getRegionManager().getRegions(region_set_name);
  std::vector<RegionResults> results;
  results.reserve(regions.size());
  
  for (size_t region_idx = 0; region_idx < regions.size(); region_idx++) {
    RegionResults region_result(static_cast<int>(region_idx));
    const DetectionRegion& region = regions[region_idx];
    
    for (const std::string& color : colors_to_detect) {
      std::vector<Blob> color_blobs = detectSingleColorCCL(hsv, region, color, min_size);
      
      if (!multi_blob_per_color && !color_blobs.empty()) {
        auto largest = std::max_element(color_blobs.begin(), color_blobs.end(),
          [](const Blob& a, const Blob& b) { return a.pixel_count < b.pixel_count; });
        color_blobs = {*largest};
      }
      
      region_result.getBlobsForColor(color) = std::move(color_blobs);
    }
    
    results.push_back(std::move(region_result));
  }
  
  return results;
}

// Legacy function - using explicit region vector (for backward compatibility)
inline std::vector<RegionResults> detectBlobsStructured(
    const HSVImage& hsv,
    const std::vector<DetectionRegion>& regions,
    const std::vector<std::string>& colors_to_detect,
    bool multi_blob_per_color = true,
    int min_size = 10) {
  
  std::vector<RegionResults> results;
  results.reserve(regions.size());
  
  for (size_t region_idx = 0; region_idx < regions.size(); region_idx++) {
    RegionResults region_result(static_cast<int>(region_idx));
    const DetectionRegion& region = regions[region_idx];
    
    for (const std::string& color : colors_to_detect) {
      std::vector<Blob> color_blobs = detectSingleColorCCL(hsv, region, color, min_size);
      
      if (!multi_blob_per_color && !color_blobs.empty()) {
        auto largest = std::max_element(color_blobs.begin(), color_blobs.end(),
          [](const Blob& a, const Blob& b) { return a.pixel_count < b.pixel_count; });
        color_blobs = {*largest};
      }
      
      region_result.getBlobsForColor(color) = std::move(color_blobs);
    }
    
    results.push_back(std::move(region_result));
  }
  
  return results;
}

// ========================================
// CONVENIENCE FUNCTIONS
// ========================================

// Detect all colors using region set name
inline std::vector<RegionResults> detectAllColorsStructured(
    const HSVImage& hsv,
    const std::string& region_set_name,
    bool multi_blob_per_color = true,
    int min_size = 10) {
  
  std::vector<std::string> all_colors = getColorManager().getAllColorNames();
  return detectBlobsStructured(hsv, region_set_name, all_colors, multi_blob_per_color, min_size);
}

// Detect single color using region set name
inline std::vector<RegionResults> detectSingleColorStructured(
    const HSVImage& hsv,
    const std::string& region_set_name,
    const std::string& color,
    bool multi_blob_per_color = true,
    int min_size = 10) {
  
  return detectBlobsStructured(hsv, region_set_name, {color}, multi_blob_per_color, min_size);
}

// Legacy functions - using explicit region vectors
inline std::vector<RegionResults> detectAllColorsStructured(
    const HSVImage& hsv,
    const std::vector<DetectionRegion>& regions,
    bool multi_blob_per_color = true,
    int min_size = 10) {
  
  std::vector<std::string> all_colors = getColorManager().getAllColorNames();
  return detectBlobsStructured(hsv, regions, all_colors, multi_blob_per_color, min_size);
}

inline std::vector<RegionResults> detectSingleColorStructured(
    const HSVImage& hsv,
    const std::vector<DetectionRegion>& regions,
    const std::string& color,
    bool multi_blob_per_color = true,
    int min_size = 10) {
  
  return detectBlobsStructured(hsv, regions, {color}, multi_blob_per_color, min_size);
}



#endif // BLOB_DETECTOR_CCL_H