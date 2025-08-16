#ifndef CAM_SETUP_H
#define CAM_SETUP_H

#include "esp_camera.h"
#include <Arduino.h>

// ========================================
// CAMERA PIN DEFINITIONS
// ========================================

#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27

#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

#define FLASH_GPIO_NUM 4

// ========================================
// CORE FUNCTIONS ONLY
// ========================================

/**
 * Initialize camera for YUV422 blob detection
 * Optimized settings for maximum speed and color detection
 */
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  // Optimized for speed and color detection
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QQVGA;  // 160x120 for speed
  config.pixel_format = PIXFORMAT_YUV422; // Required for blob detection
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    // Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  // Sensor settings optimized for blob detection
  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // Natural brightness
  s->set_contrast(s, 1);       // Enhanced edges
  s->set_saturation(s, 1);     // Boost colors
  s->set_vflip(s, 0);
  s->set_hmirror(s, 1);
  s->set_colorbar(s, 0);
  s->set_aec2(s, 1);           // Auto exposure
  s->set_ae_level(s, 0);
  s->set_whitebal(s, 1);       // Disable auto white balance
  s->set_gain_ctrl(s, 0);      // Disable auto gain
  s->set_exposure_ctrl(s, 0);  // Disable auto exposure

  // Serial.println("Camera initialized for YUV422 blob detection");
  return true;
}

/**
 * Capture image - optimized for speed
 * Returns raw camera frame buffer
 */
camera_fb_t* captureImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  
  if (!fb) {
    // Serial.println("Capture failed");
    return nullptr;
  }
  
  return fb;
}

/**
 * Get current image dimensions (always 160x120)
 */
void getImageDimensions(int* width, int* height) {
  *width = 160;
  *height = 120;
}

/**
 * Control flash LED
 */
void setFlash(bool on) {
  digitalWrite(FLASH_GPIO_NUM, on ? HIGH : LOW);
}


#endif // CAM_SETUP_H
