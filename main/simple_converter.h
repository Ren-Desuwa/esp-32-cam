#ifndef SIMPLE_CONVERTER_H
#define SIMPLE_CONVERTER_H

#include <cstdint>

// ========================================
// SIMPLE IMAGE STRUCTURES
// ========================================

struct YUVImage {
  uint8_t* y_data;    // Y channel
  uint8_t* u_data;    // U channel  
  uint8_t* v_data;    // V channel
  int width;
  int height;
  
  YUVImage() : y_data(nullptr), u_data(nullptr), v_data(nullptr), width(0), height(0) {}
  
  void clear() {
    if (y_data) { free(y_data); y_data = nullptr; }
    if (u_data) { free(u_data); u_data = nullptr; }
    if (v_data) { free(v_data); v_data = nullptr; }
    width = height = 0;
  }
  
  bool isValid() const {
    return y_data && u_data && v_data && width > 0 && height > 0;
  }
};

struct HSVImage {
  uint8_t* h_data;    // H channel (0-179)
  uint8_t* s_data;    // S channel (0-255)
  uint8_t* v_data;    // V channel (0-255)
  int width;
  int height;
  
  HSVImage() : h_data(nullptr), s_data(nullptr), v_data(nullptr), width(0), height(0) {}
  
  void clear() {
    if (h_data) { free(h_data); h_data = nullptr; }
    if (s_data) { free(s_data); s_data = nullptr; }
    if (v_data) { free(v_data); v_data = nullptr; }
    width = height = 0;
  }
  
  bool isValid() const {
    return h_data && s_data && v_data && width > 0 && height > 0;
  }
};

// ========================================
// FAST CONVERSION FUNCTIONS
// ========================================

/**
 * Convert raw YUV422 to structured YUV
 * YUV422 format: Y0 U Y1 V (4 bytes for 2 pixels)
 */
inline bool yuv422ToYUV(const uint8_t* yuv422_data, int width, int height, YUVImage& output) {
  int pixels = width * height;
  
  // Allocate memory
  output.y_data = (uint8_t*)malloc(pixels);
  output.u_data = (uint8_t*)malloc(pixels);
  output.v_data = (uint8_t*)malloc(pixels);
  
  if (!output.y_data || !output.u_data || !output.v_data) {
    output.clear();
    return false;
  }
  
  output.width = width;
  output.height = height;
  
  // Convert YUV422 to separate channels
  for (int i = 0; i < pixels; i += 2) {
    int yuv_idx = i * 2; // 4 bytes per 2 pixels
    
    // Extract YUV values
    uint8_t y0 = yuv422_data[yuv_idx];
    uint8_t u = yuv422_data[yuv_idx + 1];
    uint8_t y1 = yuv422_data[yuv_idx + 2];
    uint8_t v = yuv422_data[yuv_idx + 3];
    
    // Store in separate channels
    output.y_data[i] = y0;
    output.u_data[i] = u;
    output.v_data[i] = v;
    
    if (i + 1 < pixels) {
      output.y_data[i + 1] = y1;
      output.u_data[i + 1] = u;  // Shared U
      output.v_data[i + 1] = v;  // Shared V
    }
  }
  
  return true;
}

/**
 * Fast YUV to HSV conversion
 * Uses integer math for speed
 */
inline bool yuvToHSV(const YUVImage& yuv, HSVImage& output) {
  if (!yuv.isValid()) return false;
  
  int pixels = yuv.width * yuv.height;
  
  // Allocate memory
  output.h_data = (uint8_t*)malloc(pixels);
  output.s_data = (uint8_t*)malloc(pixels);
  output.v_data = (uint8_t*)malloc(pixels);
  
  if (!output.h_data || !output.s_data || !output.v_data) {
    output.clear();
    return false;
  }
  
  output.width = yuv.width;
  output.height = yuv.height;
  
  // Convert each pixel
  for (int i = 0; i < pixels; i++) {
    uint8_t y = yuv.y_data[i];
    uint8_t u = yuv.u_data[i];
    uint8_t v = yuv.v_data[i];
    
    // YUV to RGB (fast integer math)
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;
    
    if (c < 0) c = 0;
    
    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;
    
    // Clamp RGB
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    
    // RGB to HSV (fast integer approximation)
    uint8_t min_val = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    uint8_t max_val = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    
    output.v_data[i] = max_val;
    
    if (max_val == 0) {
      output.h_data[i] = 0;
      output.s_data[i] = 0;
      continue;
    }
    
    uint8_t delta = max_val - min_val;
    output.s_data[i] = (delta * 255) / max_val;
    
    if (delta == 0) {
      output.h_data[i] = 0;
      continue;
    }
    
    // Fast hue calculation (simplified)
    int hue;
    if (max_val == r) {
      hue = (60 * (g - b)) / delta;
    } else if (max_val == g) {
      hue = 120 + (60 * (b - r)) / delta;
    } else {
      hue = 240 + (60 * (r - g)) / delta;
    }
    
    if (hue < 0) hue += 360;
    output.h_data[i] = hue / 2; // Scale to 0-179
  }
  
  return true;
}

/**
 * Combined conversion: YUV422 directly to HSV
 * Most efficient for your use case
 */
inline bool yuv422ToHSV(const uint8_t* yuv422_data, int width, int height, HSVImage& output) {
  int pixels = width * height;
  
  // Allocate memory
  output.h_data = (uint8_t*)malloc(pixels);
  output.s_data = (uint8_t*)malloc(pixels);
  output.v_data = (uint8_t*)malloc(pixels);
  
  if (!output.h_data || !output.s_data || !output.v_data) {
    output.clear();
    return false;
  }
  
  output.width = width;
  output.height = height;
  
  // Direct YUV422 to HSV conversion
  for (int i = 0; i < pixels; i += 2) {
    int yuv_idx = i * 2;
    
    uint8_t y0 = yuv422_data[yuv_idx];
    uint8_t u = yuv422_data[yuv_idx + 1];
    uint8_t y1 = yuv422_data[yuv_idx + 2];
    uint8_t v = yuv422_data[yuv_idx + 3];
    
    // Process first pixel
    {
      int c = y0 - 16; if (c < 0) c = 0;
      int d = u - 128;
      int e = v - 128;
      
      int r = (298 * c + 409 * e + 128) >> 8;
      int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
      int b = (298 * c + 516 * d + 128) >> 8;
      
      if (r < 0) r = 0; else if (r > 255) r = 255;
      if (g < 0) g = 0; else if (g > 255) g = 255;
      if (b < 0) b = 0; else if (b > 255) b = 255;
      
      uint8_t min_val = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
      uint8_t max_val = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
      
      output.v_data[i] = max_val;
      
      if (max_val == 0) {
        output.h_data[i] = 0;
        output.s_data[i] = 0;
      } else {
        uint8_t delta = max_val - min_val;
        output.s_data[i] = (delta * 255) / max_val;
        
        if (delta == 0) {
          output.h_data[i] = 0;
        } else {
          int hue;
          if (max_val == r) hue = (60 * (g - b)) / delta;
          else if (max_val == g) hue = 120 + (60 * (b - r)) / delta;
          else hue = 240 + (60 * (r - g)) / delta;
          
          if (hue < 0) hue += 360;
          output.h_data[i] = hue / 2;
        }
      }
    }
    
    // Process second pixel (if exists)
    if (i + 1 < pixels) {
      int c = y1 - 16; if (c < 0) c = 0;
      int d = u - 128;
      int e = v - 128;
      
      int r = (298 * c + 409 * e + 128) >> 8;
      int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
      int b = (298 * c + 516 * d + 128) >> 8;
      
      if (r < 0) r = 0; else if (r > 255) r = 255;
      if (g < 0) g = 0; else if (g > 255) g = 255;
      if (b < 0) b = 0; else if (b > 255) b = 255;
      
      uint8_t min_val = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
      uint8_t max_val = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
      
      output.v_data[i + 1] = max_val;
      
      if (max_val == 0) {
        output.h_data[i + 1] = 0;
        output.s_data[i + 1] = 0;
      } else {
        uint8_t delta = max_val - min_val;
        output.s_data[i + 1] = (delta * 255) / max_val;
        
        if (delta == 0) {
          output.h_data[i + 1] = 0;
        } else {
          int hue;
          if (max_val == r) hue = (60 * (g - b)) / delta;
          else if (max_val == g) hue = 120 + (60 * (b - r)) / delta;
          else hue = 240 + (60 * (r - g)) / delta;
          
          if (hue < 0) hue += 360;
          output.h_data[i + 1] = hue / 2;
        }
      }
    }
  }
  
  return true;
}

#endif // SIMPLE_CONVERTER_H