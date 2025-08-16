#include "cam_setup.h"
#include "simple_converter.h"
#include "blob_detector_ccl.h"
#include "blob_command_interface.h"
#include "dual_core.h"

// ========================================
// GLOBAL STATE
// ========================================
volatile bool connection_established = false;
volatile bool system_ready = false;
volatile bool capture_enabled = false;

BlobCommandInterface command_interface(&Serial);
HSVImage hsv_image;

// ========================================
// CONNECTION CHECK (CORE 0)
// ========================================
void loop2() {
  if (!connection_established) {
    // Flash LED to indicate looking for connection
    static unsigned long last_flash = 0;
    static bool flash_state = false;
    
    if (millis() - last_flash > 2000) {
      flash_state = !flash_state;
      setFlash(flash_state);
      last_flash = millis();
      
      // Send ping
      Serial.println("PING");
    }
    
    // Check for response
    if (Serial.available()) {
      String response = Serial.readStringUntil('\n');
      response.trim();
      
      if (response == "PONG") {
        connection_established = true;
        setFlash(false);  // Turn off flash
        Serial.println("CONNECTION_OK");
      }
    }
  }
  
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// ========================================
// MAIN SETUP
// ========================================
void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.setTimeout(100);
  
  // Initialize flash pin
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  setFlash(true);
  delay(100);
  setFlash(false);
  delay(100);
  setFlash(true);
  delay(100);
  setFlash(false);
  
  // Start dual core for connection checking
  startDualCore();
  
  // Wait for connection
  while (!connection_established) {
    delay(100);
  }
  
  // Initialize camera
  if (!initCamera()) {
    Serial.println("ERROR: Camera init failed");
    while (1) {
      setFlash(true);
      delay(500);
      setFlash(false);
      delay(50);
    }
  }
  
  // Initialize command interface
  command_interface.begin(115200);
  
  Serial.println("READY");
  system_ready = true;
}

// ========================================
// MAIN LOOP (CORE 1)
// ========================================
void loop() {
  if (!system_ready) {
    delay(100);
    return;
  }

  // Process ALL incoming commands
  command_interface.processCommands();
  
  // Capture and process if enabled
  if (capture_enabled) {
    // Capture image
    camera_fb_t* fb = captureImage();
    if (!fb) {
      Serial.println("ERROR: Capture failed");
      delay(100);
      return;
    }
    
    // Convert to HSV
    hsv_image.clear();  // Clean up previous image
    
    if (yuv422ToHSV(fb->buf, fb->width, fb->height, hsv_image)) {
        // Normal blob detection
      auto results = detectBlobsStructured(hsv_image, "main", {"RED","GREEN"}, true, 10);
      command_interface.sendSimpleBlobResults(results);
    } else {
      Serial.println("ERROR: Conversion failed");
    }
    
    // Release frame buffer
    esp_camera_fb_return(fb);
    
    // Small delay for stability
    delay(50);
  } else {
    delay(100);
  }
}

// ========================================
// GLOBAL FUNCTIONS FOR COMMAND INTERFACE
// ========================================

void enableCapture() {
  capture_enabled = true;
}

void disableCapture() {
  capture_enabled = false;
}

bool isCaptureEnabled() {
  return capture_enabled;
}