#include "cam_setup.h"
#include <WiFi.h>
#include <WebServer.h>

// ========================================
// CONFIGURATION
// ========================================
#define SERIAL_BAUD 115200
#define MAX_YUV_BUFFER_SIZE 38400  // 160x120x2 pixels for YUV422

// ========================================
// GLOBAL STATE
// ========================================
WebServer server(80);

// YUV Image Buffer
struct YUVImageBuffer {
  uint8_t yuv_data[MAX_YUV_BUFFER_SIZE];
  int width;
  int height;
  bool data_ready;
  unsigned long last_update;
} yuv_buffer = {0};

// Wi-Fi Configuration - UPDATE THESE
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ========================================
// CAMERA FUNCTIONS
// ========================================
bool captureYUVImage() {
  Serial.println("Capturing YUV image...");
  
  camera_fb_t* fb = captureImage();
  if (!fb) {
    Serial.println("Failed to capture image");
    return false;
  }
  
  // Get image dimensions
  getImageDimensions(&yuv_buffer.width, &yuv_buffer.height);
  
  if (fb->len > MAX_YUV_BUFFER_SIZE) {
    Serial.printf("Image too large: %d bytes (max: %d)\n", fb->len, MAX_YUV_BUFFER_SIZE);
    esp_camera_fb_return(fb);
    return false;
  }
  
  // Copy YUV data to buffer
  memcpy(yuv_buffer.yuv_data, fb->buf, fb->len);
  yuv_buffer.data_ready = true;
  yuv_buffer.last_update = millis();
  
  Serial.printf("YUV capture complete: %dx%d, %d bytes\n", 
                yuv_buffer.width, yuv_buffer.height, fb->len);
  
  esp_camera_fb_return(fb);
  return true;
}

// ========================================
// SERVER HANDLERS
// ========================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Enhanced ESP32 YUV Camera</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            background: #1a1a1a; 
            color: #fff; 
            margin: 0; 
            padding: 20px; 
        }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 20px; }
        
        .controls { 
            background: #2a2a2a; 
            padding: 15px; 
            border-radius: 8px; 
            margin-bottom: 15px;
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            align-items: center;
        }
        
        button { 
            background: #4a9eff; 
            color: white; 
            border: none; 
            padding: 8px 16px; 
            border-radius: 4px; 
            cursor: pointer; 
        }
        button:hover { background: #3a8eef; }
        button:disabled { background: #555; cursor: not-allowed; }
        
        .displays {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        
        .display-panel {
            background: #2a2a2a;
            padding: 15px;
            border-radius: 8px;
            text-align: center;
        }
        
        .display-panel h3 {
            margin-top: 0;
            color: #4a9eff;
        }
        
        canvas { 
            border: 1px solid #444; 
            image-rendering: pixelated;
            background: #000;
            width: 100%;
            max-width: 320px;
            height: auto;
        }
        
        .sliders {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 15px;
        }
        
        .slider-group {
            background: #2a2a2a;
            padding: 15px;
            border-radius: 8px;
        }
        
        .slider-group h4 {
            margin-top: 0;
            color: #4a9eff;
            text-align: center;
        }
        
        .slider-row {
            display: flex;
            align-items: center;
            margin-bottom: 10px;
            gap: 10px;
        }
        
        .slider-row label {
            width: 60px;
            font-size: 12px;
        }
        
        .slider-row input[type="range"] {
            flex: 1;
            margin: 0 10px;
        }
        
        .slider-row .value {
            width: 40px;
            text-align: center;
            font-size: 12px;
            background: #333;
            border: 1px solid #555;
            color: #fff;
            padding: 2px 4px;
            border-radius: 3px;
        }
        
        .status { 
            background: #333; 
            padding: 8px; 
            border-radius: 4px; 
            font-size: 14px; 
            margin-bottom: 10px; 
            text-align: center;
        }
        
        .region-controls {
            background: #2a2a2a;
            padding: 15px;
            border-radius: 8px;
            margin-top: 15px;
        }
        
        .region-controls h4 {
            margin-top: 0;
            color: #4a9eff;
        }
        
        .region-input {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 10px;
            margin-bottom: 10px;
        }
        
        .region-input input {
            padding: 5px;
            background: #333;
            border: 1px solid #555;
            color: #fff;
            border-radius: 3px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Enhanced ESP32 YUV Camera</h1>
            <div class="status" id="status">Ready</div>
        </div>
        
        <div class="controls">
            <button onclick="captureImage()">Capture</button>
            <button onclick="toggleAuto()" id="autoBtn">Auto: OFF</button>
            <button onclick="resetFilters()">Reset Filters</button>
            <button onclick="addRegion()">Add Region</button>
            <button onclick="clearRegions()">Clear Regions</button>
        </div>
        
        <div class="displays">
            <div class="display-panel">
                <h3>Original YUV</h3>
                <canvas id="yuvCanvas" width="160" height="120"></canvas>
            </div>
            
            <div class="display-panel">
                <h3>HSV Conversion</h3>
                <canvas id="hsvCanvas" width="160" height="120"></canvas>
            </div>
            
            <div class="display-panel">
                <h3>Filtered YUV</h3>
                <canvas id="filteredYuvCanvas" width="160" height="120"></canvas>
            </div>
            
            <div class="display-panel">
                <h3>Filtered HSV</h3>
                <canvas id="filteredHsvCanvas" width="160" height="120"></canvas>
            </div>
            
            <div class="display-panel">
                <h3>Detection Regions</h3>
                <canvas id="regionsCanvas" width="160" height="120"></canvas>
            </div>
        </div>
        
        <div class="sliders">
            <div class="slider-group">
                <h4>YUV Thresholds</h4>
                <div class="slider-row">
                    <label>Y Min:</label>
                    <input type="range" id="yMin" min="0" max="255" value="0" oninput="updateFilters()">
                    <span class="value" id="yMinVal">0</span>
                </div>
                <div class="slider-row">
                    <label>Y Max:</label>
                    <input type="range" id="yMax" min="0" max="255" value="255" oninput="updateFilters()">
                    <span class="value" id="yMaxVal">255</span>
                </div>
                <div class="slider-row">
                    <label>U Min:</label>
                    <input type="range" id="uMin" min="0" max="255" value="0" oninput="updateFilters()">
                    <span class="value" id="uMinVal">0</span>
                </div>
                <div class="slider-row">
                    <label>U Max:</label>
                    <input type="range" id="uMax" min="0" max="255" value="255" oninput="updateFilters()">
                    <span class="value" id="uMaxVal">255</span>
                </div>
                <div class="slider-row">
                    <label>V Min:</label>
                    <input type="range" id="vMin" min="0" max="255" value="0" oninput="updateFilters()">
                    <span class="value" id="vMinVal">0</span>
                </div>
                <div class="slider-row">
                    <label>V Max:</label>
                    <input type="range" id="vMax" min="0" max="255" value="255" oninput="updateFilters()">
                    <span class="value" id="vMaxVal">255</span>
                </div>
            </div>
            
            <div class="slider-group">
                <h4>HSV Thresholds</h4>
                <div class="slider-row">
                    <label>H Min:</label>
                    <input type="range" id="hMin" min="0" max="179" value="0" oninput="updateFilters()">
                    <span class="value" id="hMinVal">0</span>
                </div>
                <div class="slider-row">
                    <label>H Max:</label>
                    <input type="range" id="hMax" min="0" max="179" value="179" oninput="updateFilters()">
                    <span class="value" id="hMaxVal">179</span>
                </div>
                <div class="slider-row">
                    <label>S Min:</label>
                    <input type="range" id="sMin" min="0" max="255" value="0" oninput="updateFilters()">
                    <span class="value" id="sMinVal">0</span>
                </div>
                <div class="slider-row">
                    <label>S Max:</label>
                    <input type="range" id="sMax" min="0" max="255" value="255" oninput="updateFilters()">
                    <span class="value" id="sMaxVal">255</span>
                </div>
                <div class="slider-row">
                    <label>V Min:</label>
                    <input type="range" id="vHsvMin" min="0" max="255" value="0" oninput="updateFilters()">
                    <span class="value" id="vHsvMinVal">0</span>
                </div>
                <div class="slider-row">
                    <label>V Max:</label>
                    <input type="range" id="vHsvMax" min="0" max="255" value="255" oninput="updateFilters()">
                    <span class="value" id="vHsvMaxVal">255</span>
                </div>
            </div>
        </div>
        
        <div class="region-controls">
            <h4>Detection Regions</h4>
            <div class="region-input">
                <input type="number" id="regionX" placeholder="X" value="0" min="0" max="159">
                <input type="number" id="regionY" placeholder="Y" value="0" min="0" max="119">
                <input type="number" id="regionW" placeholder="Width" value="80" min="1" max="160">
                <input type="number" id="regionH" placeholder="Height" value="60" min="1" max="120">
            </div>
            <div id="regionsList"></div>
        </div>
    </div>

    <script>
        // Canvas elements
        const yuvCanvas = document.getElementById('yuvCanvas');
        const hsvCanvas = document.getElementById('hsvCanvas');
        const filteredYuvCanvas = document.getElementById('filteredYuvCanvas');
        const filteredHsvCanvas = document.getElementById('filteredHsvCanvas');
        const regionsCanvas = document.getElementById('regionsCanvas');
        
        const yuvCtx = yuvCanvas.getContext('2d');
        const hsvCtx = hsvCanvas.getContext('2d');
        const filteredYuvCtx = filteredYuvCanvas.getContext('2d');
        const filteredHsvCtx = filteredHsvCanvas.getContext('2d');
        const regionsCtx = regionsCanvas.getContext('2d');
        
        const status = document.getElementById('status');
        const autoBtn = document.getElementById('autoBtn');
        
        let autoMode = false;
        let autoInterval = null;
        let currentYuvData = null;
        let currentHsvData = null;
        let regions = [];
        
        function updateStatus(msg) {
            status.textContent = msg;
        }
        
        // Fast YUV to RGB conversion (matches simple_converter.h exactly)
        function yuvToRgb(y, u, v) {
            const c = Math.max(0, y - 16);
            const d = u - 128;
            const e = v - 128;
            
            let r = (298 * c + 409 * e + 128) >> 8;
            let g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            let b = (298 * c + 516 * d + 128) >> 8;
            
            return [
                Math.max(0, Math.min(255, r)),
                Math.max(0, Math.min(255, g)),
                Math.max(0, Math.min(255, b))
            ];
        }
        
        // Fast RGB to HSV conversion (matches simple_converter.h exactly)
        function rgbToHsv(r, g, b) {
            const minVal = Math.min(r, g, b);
            const maxVal = Math.max(r, g, b);
            const delta = maxVal - minVal;
            
            const v = maxVal;
            const s = maxVal === 0 ? 0 : Math.floor((delta * 255) / maxVal);
            
            let h = 0;
            if (delta !== 0) {
                if (maxVal === r) {
                    h = Math.floor((60 * (g - b)) / delta);
                } else if (maxVal === g) {
                    h = Math.floor(120 + (60 * (b - r)) / delta);
                } else {
                    h = Math.floor(240 + (60 * (r - g)) / delta);
                }
                if (h < 0) h += 360;
            }
            
            return [Math.floor(h / 2), s, v]; // H scaled to 0-179
        }
        
        // HSV to RGB for display
        function hsvToRgb(h, s, v) {
            h *= 2; // Scale back to 0-360
            const sNorm = s / 255;
            const vNorm = v / 255;
            
            const c = vNorm * sNorm;
            const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
            const m = vNorm - c;
            
            let r, g, b;
            if (h < 60) {
                [r, g, b] = [c, x, 0];
            } else if (h < 120) {
                [r, g, b] = [x, c, 0];
            } else if (h < 180) {
                [r, g, b] = [0, c, x];
            } else if (h < 240) {
                [r, g, b] = [0, x, c];
            } else if (h < 300) {
                [r, g, b] = [x, 0, c];
            } else {
                [r, g, b] = [c, 0, x];
            }
            
            return [
                Math.round((r + m) * 255),
                Math.round((g + m) * 255),
                Math.round((b + m) * 255)
            ];
        }
        
        // Direct YUV422 to HSV conversion (matches simple_converter.h approach)
        function yuv422ToHsv(yuvData) {
            const pixels = 160 * 120;
            const hsvData = {
                h: new Uint8Array(pixels),
                s: new Uint8Array(pixels),
                v: new Uint8Array(pixels)
            };
            
            let pixelIndex = 0;
            for (let i = 0; i < yuvData.length; i += 4) {
                const y0 = yuvData[i];
                const u = yuvData[i + 1];
                const y1 = yuvData[i + 2];
                const v = yuvData[i + 3];
                
                // First pixel - direct YUV to HSV
                {
                    const c = Math.max(0, y0 - 16);
                    const d = u - 128;
                    const e = v - 128;
                    
                    let r = (298 * c + 409 * e + 128) >> 8;
                    let g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                    let b = (298 * c + 516 * d + 128) >> 8;
                    
                    r = Math.max(0, Math.min(255, r));
                    g = Math.max(0, Math.min(255, g));
                    b = Math.max(0, Math.min(255, b));
                    
                    const minVal = Math.min(r, g, b);
                    const maxVal = Math.max(r, g, b);
                    const delta = maxVal - minVal;
                    
                    hsvData.v[pixelIndex] = maxVal;
                    
                    if (maxVal === 0) {
                        hsvData.h[pixelIndex] = 0;
                        hsvData.s[pixelIndex] = 0;
                    } else {
                        hsvData.s[pixelIndex] = Math.floor((delta * 255) / maxVal);
                        
                        if (delta === 0) {
                            hsvData.h[pixelIndex] = 0;
                        } else {
                            let h;
                            if (maxVal === r) h = Math.floor((60 * (g - b)) / delta);
                            else if (maxVal === g) h = Math.floor(120 + (60 * (b - r)) / delta);
                            else h = Math.floor(240 + (60 * (r - g)) / delta);
                            
                            if (h < 0) h += 360;
                            hsvData.h[pixelIndex] = Math.floor(h / 2);
                        }
                    }
                    pixelIndex++;
                }
                
                // Second pixel
                if (pixelIndex < pixels) {
                    const c = Math.max(0, y1 - 16);
                    const d = u - 128;
                    const e = v - 128;
                    
                    let r = (298 * c + 409 * e + 128) >> 8;
                    let g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                    let b = (298 * c + 516 * d + 128) >> 8;
                    
                    r = Math.max(0, Math.min(255, r));
                    g = Math.max(0, Math.min(255, g));
                    b = Math.max(0, Math.min(255, b));
                    
                    const minVal = Math.min(r, g, b);
                    const maxVal = Math.max(r, g, b);
                    const delta = maxVal - minVal;
                    
                    hsvData.v[pixelIndex] = maxVal;
                    
                    if (maxVal === 0) {
                        hsvData.h[pixelIndex] = 0;
                        hsvData.s[pixelIndex] = 0;
                    } else {
                        hsvData.s[pixelIndex] = Math.floor((delta * 255) / maxVal);
                        
                        if (delta === 0) {
                            hsvData.h[pixelIndex] = 0;
                        } else {
                            let h;
                            if (maxVal === r) h = Math.floor((60 * (g - b)) / delta);
                            else if (maxVal === g) h = Math.floor(120 + (60 * (b - r)) / delta);
                            else h = Math.floor(240 + (60 * (r - g)) / delta);
                            
                            if (h < 0) h += 360;
                            hsvData.h[pixelIndex] = Math.floor(h / 2);
                        }
                    }
                    pixelIndex++;
                }
            }
            
            return hsvData;
        }
        
        // Get filter thresholds
        function getFilterThresholds() {
            return {
                yuv: {
                    yMin: parseInt(document.getElementById('yMin').value),
                    yMax: parseInt(document.getElementById('yMax').value),
                    uMin: parseInt(document.getElementById('uMin').value),
                    uMax: parseInt(document.getElementById('uMax').value),
                    vMin: parseInt(document.getElementById('vMin').value),
                    vMax: parseInt(document.getElementById('vMax').value)
                },
                hsv: {
                    hMin: parseInt(document.getElementById('hMin').value),
                    hMax: parseInt(document.getElementById('hMax').value),
                    sMin: parseInt(document.getElementById('sMin').value),
                    sMax: parseInt(document.getElementById('sMax').value),
                    vMin: parseInt(document.getElementById('vHsvMin').value),
                    vMax: parseInt(document.getElementById('vHsvMax').value)
                }
            };
        }
        
        // Update filter displays
        function updateFilters() {
            // Update all slider value displays
            const sliders = ['yMin', 'yMax', 'uMin', 'uMax', 'vMin', 'vMax', 
                           'hMin', 'hMax', 'sMin', 'sMax', 'vHsvMin', 'vHsvMax'];
            sliders.forEach(id => {
                document.getElementById(id + 'Val').textContent = document.getElementById(id).value;
            });
            
            if (currentYuvData && currentHsvData) {
                displayFilteredImages();
            }
        }
        
        // Display filtered images
        function displayFilteredImages() {
            const thresholds = getFilterThresholds();
            const pixels = 160 * 120;
            
            const filteredYuvImageData = filteredYuvCtx.createImageData(160, 120);
            const filteredHsvImageData = filteredHsvCtx.createImageData(160, 120);
            
            let pixelIndex = 0;
            for (let i = 0; i < currentYuvData.length; i += 4) {
                const y0 = currentYuvData[i];
                const u = currentYuvData[i + 1];
                const y1 = currentYuvData[i + 2];
                const v = currentYuvData[i + 3];
                
                // First pixel
                {
                    const h = currentHsvData.h[pixelIndex];
                    const s = currentHsvData.s[pixelIndex];
                    const hsvV = currentHsvData.v[pixelIndex];
                    
                    const yuvMatch = (y0 >= thresholds.yuv.yMin && y0 <= thresholds.yuv.yMax &&
                                    u >= thresholds.yuv.uMin && u <= thresholds.yuv.uMax &&
                                    v >= thresholds.yuv.vMin && v <= thresholds.yuv.vMax);
                    
                    const hsvMatch = (h >= thresholds.hsv.hMin && h <= thresholds.hsv.hMax &&
                                    s >= thresholds.hsv.sMin && s <= thresholds.hsv.sMax &&
                                    hsvV >= thresholds.hsv.vMin && hsvV <= thresholds.hsv.vMax);
                    
                    const [r, g, b] = yuvToRgb(y0, u, v);
                    const [hsvR, hsvG, hsvB] = hsvToRgb(h, s, hsvV);
                    
                    // YUV filter visualization
                    const yuvIdx = pixelIndex * 4;
                    if (yuvMatch) {
                        filteredYuvImageData.data[yuvIdx] = r;
                        filteredYuvImageData.data[yuvIdx + 1] = g;
                        filteredYuvImageData.data[yuvIdx + 2] = b;
                        filteredYuvImageData.data[yuvIdx + 3] = 255;
                    } else {
                        filteredYuvImageData.data[yuvIdx] = r >> 2;
                        filteredYuvImageData.data[yuvIdx + 1] = g >> 2;
                        filteredYuvImageData.data[yuvIdx + 2] = b >> 2;
                        filteredYuvImageData.data[yuvIdx + 3] = 64;
                    }
                    
                    // HSV filter visualization
                    if (hsvMatch) {
                        filteredHsvImageData.data[yuvIdx] = hsvR;
                        filteredHsvImageData.data[yuvIdx + 1] = hsvG;
                        filteredHsvImageData.data[yuvIdx + 2] = hsvB;
                        filteredHsvImageData.data[yuvIdx + 3] = 255;
                    } else {
                        filteredHsvImageData.data[yuvIdx] = hsvR >> 2;
                        filteredHsvImageData.data[yuvIdx + 1] = hsvG >> 2;
                        filteredHsvImageData.data[yuvIdx + 2] = hsvB >> 2;
                        filteredHsvImageData.data[yuvIdx + 3] = 64;
                    }
                    
                    pixelIndex++;
                }
                
                // Second pixel
                if (pixelIndex < pixels) {
                    const h = currentHsvData.h[pixelIndex];
                    const s = currentHsvData.s[pixelIndex];
                    const hsvV = currentHsvData.v[pixelIndex];
                    
                    const yuvMatch = (y1 >= thresholds.yuv.yMin && y1 <= thresholds.yuv.yMax &&
                                    u >= thresholds.yuv.uMin && u <= thresholds.yuv.uMax &&
                                    v >= thresholds.yuv.vMin && v <= thresholds.yuv.vMax);
                    
                    const hsvMatch = (h >= thresholds.hsv.hMin && h <= thresholds.hsv.hMax &&
                                    s >= thresholds.hsv.sMin && s <= thresholds.hsv.sMax &&
                                    hsvV >= thresholds.hsv.vMin && hsvV <= thresholds.hsv.vMax);
                    
                    const [r, g, b] = yuvToRgb(y1, u, v);
                    const [hsvR, hsvG, hsvB] = hsvToRgb(h, s, hsvV);
                    
                    const yuvIdx = pixelIndex * 4;
                    if (yuvMatch) {
                        filteredYuvImageData.data[yuvIdx] = r;
                        filteredYuvImageData.data[yuvIdx + 1] = g;
                        filteredYuvImageData.data[yuvIdx + 2] = b;
                        filteredYuvImageData.data[yuvIdx + 3] = 255;
                    } else {
                        filteredYuvImageData.data[yuvIdx] = r >> 2;
                        filteredYuvImageData.data[yuvIdx + 1] = g >> 2;
                        filteredYuvImageData.data[yuvIdx + 2] = b >> 2;
                        filteredYuvImageData.data[yuvIdx + 3] = 64;
                    }
                    
                    if (hsvMatch) {
                        filteredHsvImageData.data[yuvIdx] = hsvR;
                        filteredHsvImageData.data[yuvIdx + 1] = hsvG;
                        filteredHsvImageData.data[yuvIdx + 2] = hsvB;
                        filteredHsvImageData.data[yuvIdx + 3] = 255;
                    } else {
                        filteredHsvImageData.data[yuvIdx] = hsvR >> 2;
                        filteredHsvImageData.data[yuvIdx + 1] = hsvG >> 2;
                        filteredHsvImageData.data[yuvIdx + 2] = hsvB >> 2;
                        filteredHsvImageData.data[yuvIdx + 3] = 64;
                    }
                    
                    pixelIndex++;
                }
            }
            
            filteredYuvCtx.putImageData(filteredYuvImageData, 0, 0);
            filteredHsvCtx.putImageData(filteredHsvImageData, 0, 0);
        }
        // Generate random YUV422 data for testing (160x120)
        
        // Convert YUV422 to separate YUV channels (matches simple_converter.h exactly)
        function yuv422ToYUV(yuv422Data) {
            const width = 160;
            const height = 120;
            const pixels = width * height;
            
            const yData = new Uint8Array(pixels);
            const uData = new Uint8Array(pixels);
            const vData = new Uint8Array(pixels);
            
            for (let i = 0; i < pixels; i += 2) {
                const yuvIdx = i * 2;
                
                const y0 = yuv422Data[yuvIdx];
                const u = yuv422Data[yuvIdx + 1];
                const y1 = yuv422Data[yuvIdx + 2];
                const v = yuv422Data[yuvIdx + 3];
                
                yData[i] = y0;
                uData[i] = u;
                vData[i] = v;
                
                if (i + 1 < pixels) {
                    yData[i + 1] = y1;
                    uData[i + 1] = u;
                    vData[i + 1] = v;
                }
            }
            
            return { y: yData, u: uData, v: vData, width, height };
        }
        
        // Direct YUV422 to HSV conversion (matches simple_converter.h exactly)
        function yuv422ToHSV(yuv422Data) {
            const width = 160;
            const height = 120;
            const pixels = width * height;
            
            const hData = new Uint8Array(pixels);
            const sData = new Uint8Array(pixels);
            const vData = new Uint8Array(pixels);
            
            for (let i = 0; i < pixels; i += 2) {
                const yuvIdx = i * 2;
                
                const y0 = yuv422Data[yuvIdx];
                const u = yuv422Data[yuvIdx + 1];
                const y1 = yuv422Data[yuvIdx + 2];
                const v = yuv422Data[yuvIdx + 3];
                
                // Process first pixel
                {
                    let c = y0 - 16;
                    if (c < 0) c = 0;
                    const d = u - 128;
                    const e = v - 128;
                    
                    let r = (298 * c + 409 * e + 128) >> 8;
                    let g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                    let b = (298 * c + 516 * d + 128) >> 8;
                    
                    if (r < 0) r = 0; else if (r > 255) r = 255;
                    if (g < 0) g = 0; else if (g > 255) g = 255;
                    if (b < 0) b = 0; else if (b > 255) b = 255;
                    
                    const minVal = Math.min(r, g, b);
                    const maxVal = Math.max(r, g, b);
                    const delta = maxVal - minVal;
                    
                    vData[i] = maxVal;
                    
                    if (maxVal === 0) {
                        hData[i] = 0;
                        sData[i] = 0;
                    } else {
                        sData[i] = Math.floor((delta * 255) / maxVal);
                        
                        if (delta === 0) {
                            hData[i] = 0;
                        } else {
                            let hue;
                            if (maxVal === r) hue = Math.floor((60 * (g - b)) / delta);
                            else if (maxVal === g) hue = 120 + Math.floor((60 * (b - r)) / delta);
                            else hue = 240 + Math.floor((60 * (r - g)) / delta);
                            
                            if (hue < 0) hue += 360;
                            hData[i] = Math.floor(hue / 2);
                        }
                    }
                }
                
                // Process second pixel
                if (i + 1 < pixels) {
                    let c = y1 - 16;
                    if (c < 0) c = 0;
                    const d = u - 128;
                    const e = v - 128;
                    
                    let r = (298 * c + 409 * e + 128) >> 8;
                    let g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                    let b = (298 * c + 516 * d + 128) >> 8;
                    
                    if (r < 0) r = 0; else if (r > 255) r = 255;
                    if (g < 0) g = 0; else if (g > 255) g = 255;
                    if (b < 0) b = 0; else if (b > 255) b = 255;
                    
                    const minVal = Math.min(r, g, b);
                    const maxVal = Math.max(r, g, b);
                    const delta = maxVal - minVal;
                    
                    vData[i + 1] = maxVal;
                    
                    if (maxVal === 0) {
                        hData[i + 1] = 0;
                        sData[i + 1] = 0;
                    } else {
                        sData[i + 1] = Math.floor((delta * 255) / maxVal);
                        
                        if (delta === 0) {
                            hData[i + 1] = 0;
                        } else {
                            let hue;
                            if (maxVal === r) hue = Math.floor((60 * (g - b)) / delta);
                            else if (maxVal === g) hue = 120 + Math.floor((60 * (b - r)) / delta);
                            else hue = 240 + Math.floor((60 * (r - g)) / delta);
                            
                            if (hue < 0) hue += 360;
                            hData[i + 1] = Math.floor(hue / 2);
                        }
                    }
                }
            }
            
            return { h: hData, s: sData, v: vData, width, height };
        }

        // Display YUV image as RGB
        function displayYUVImage(yuvData, canvas, ctx) {
            const imageData = ctx.createImageData(160, 120);
            
            for (let i = 0; i < 160 * 120; i++) {
                const [r, g, b] = yuvToRgb(yuvData.y[i], yuvData.u[i], yuvData.v[i]);
                
                const idx = i * 4;
                imageData.data[idx] = r;
                imageData.data[idx + 1] = g;
                imageData.data[idx + 2] = b;
                imageData.data[idx + 3] = 255;
            }
            
            ctx.putImageData(imageData, 0, 0);
        }
        
        // Display HSV image as RGB
        function displayHSVImage(hsvData, canvas, ctx) {
            const imageData = ctx.createImageData(160, 120);
            
            for (let i = 0; i < 160 * 120; i++) {
                const [r, g, b] = hsvToRgb(hsvData.h[i], hsvData.s[i], hsvData.v[i]);
                
                const idx = i * 4;
                imageData.data[idx] = r;
                imageData.data[idx + 1] = g;
                imageData.data[idx + 2] = b;
                imageData.data[idx + 3] = 255;
            }
            
            ctx.putImageData(imageData, 0, 0);
        }
        
        // Get current filter thresholds
        function getFilterThresholds() {
            return {
                yuv: {
                    yMin: parseInt(document.getElementById('yMin').value),
                    yMax: parseInt(document.getElementById('yMax').value),
                    uMin: parseInt(document.getElementById('uMin').value),
                    uMax: parseInt(document.getElementById('uMax').value),
                    vMin: parseInt(document.getElementById('vMin').value),
                    vMax: parseInt(document.getElementById('vMax').value)
                },
                hsv: {
                    hMin: parseInt(document.getElementById('hMin').value),
                    hMax: parseInt(document.getElementById('hMax').value),
                    sMin: parseInt(document.getElementById('sMin').value),
                    sMax: parseInt(document.getElementById('sMax').value),
                    vMin: parseInt(document.getElementById('vHsvMin').value),
                    vMax: parseInt(document.getElementById('vHsvMax').value)
                }
            };
        }
        
        // Display filtered images
        function displayFilteredImages() {
            if (!currentYuvData || !currentHsvData) return;
            
            const thresholds = getFilterThresholds();
            const pixels = 160 * 120;
            
            const filteredYuvImageData = filteredYuvCtx.createImageData(160, 120);
            const filteredHsvImageData = filteredHsvCtx.createImageData(160, 120);
            
            for (let i = 0; i < pixels; i++) {
                const y = currentYuvData.y[i];
                const u = currentYuvData.u[i];
                const v = currentYuvData.v[i];
                const h = currentHsvData.h[i];
                const s = currentHsvData.s[i];
                const hsvV = currentHsvData.v[i];
                
                const yuvMatch = (y >= thresholds.yuv.yMin && y <= thresholds.yuv.yMax &&
                                u >= thresholds.yuv.uMin && u <= thresholds.yuv.uMax &&
                                v >= thresholds.yuv.vMin && v <= thresholds.yuv.vMax);
                
                const hsvMatch = (h >= thresholds.hsv.hMin && h <= thresholds.hsv.hMax &&
                                s >= thresholds.hsv.sMin && s <= thresholds.hsv.sMax &&
                                hsvV >= thresholds.hsv.vMin && hsvV <= thresholds.hsv.vMax);
                
                const [r, g, b] = yuvToRgb(y, u, v);
                const [hsvR, hsvG, hsvB] = hsvToRgb(h, s, hsvV);
                
                const idx = i * 4;
                
                // YUV filter visualization
                if (yuvMatch) {
                    filteredYuvImageData.data[idx] = r;
                    filteredYuvImageData.data[idx + 1] = g;
                    filteredYuvImageData.data[idx + 2] = b;
                    filteredYuvImageData.data[idx + 3] = 255;
                } else {
                    filteredYuvImageData.data[idx] = r >> 2;
                    filteredYuvImageData.data[idx + 1] = g >> 2;
                    filteredYuvImageData.data[idx + 2] = b >> 2;
                    filteredYuvImageData.data[idx + 3] = 64;
                }
                
                // HSV filter visualization
                if (hsvMatch) {
                    filteredHsvImageData.data[idx] = hsvR;
                    filteredHsvImageData.data[idx + 1] = hsvG;
                    filteredHsvImageData.data[idx + 2] = hsvB;
                    filteredHsvImageData.data[idx + 3] = 255;
                } else {
                    filteredHsvImageData.data[idx] = hsvR >> 2;
                    filteredHsvImageData.data[idx + 1] = hsvG >> 2;
                    filteredHsvImageData.data[idx + 2] = hsvB >> 2;
                    filteredHsvImageData.data[idx + 3] = 64;
                }
            }
            
            filteredYuvCtx.putImageData(filteredYuvImageData, 0, 0);
            filteredHsvCtx.putImageData(filteredHsvImageData, 0, 0);
        }
        
        // Display regions overlay
        function displayRegions() {
            regionsCtx.fillStyle = '#000';
            regionsCtx.fillRect(0, 0, 160, 120);
            
            // Copy dimmed original image as background if available
            if (currentYuvData) {
                const regionImageData = regionsCtx.createImageData(160, 120);
                
                for (let i = 0; i < 160 * 120; i++) {
                    const [r, g, b] = yuvToRgb(currentYuvData.y[i], currentYuvData.u[i], currentYuvData.v[i]);
                    
                    const idx = i * 4;
                    regionImageData.data[idx] = r >> 1;
                    regionImageData.data[idx + 1] = g >> 1;
                    regionImageData.data[idx + 2] = b >> 1;
                    regionImageData.data[idx + 3] = 128;
                }
                
                regionsCtx.putImageData(regionImageData, 0, 0);
            }
            
            // Draw region boxes
            regions.forEach((region, index) => {
                const hue = (index * 60) % 360;
                regionsCtx.strokeStyle = `hsl(${hue}, 100%, 60%)`;
                regionsCtx.lineWidth = 2;
                regionsCtx.strokeRect(region.x, region.y, region.width, region.height);
                
                regionsCtx.fillStyle = `hsla(${hue}, 100%, 60%, 0.3)`;
                regionsCtx.fillRect(region.x, region.y, region.width, region.height);
                
                // Draw region number
                regionsCtx.fillStyle = '#fff';
                regionsCtx.font = '12px Arial';
                regionsCtx.fillText(index.toString(), region.x + 2, region.y + 14);
            });
        }
        
        // Update filter displays
        function updateFilters() {
            // Update all slider value displays
            const sliders = ['yMin', 'yMax', 'uMin', 'uMax', 'vMin', 'vMax', 
                           'hMin', 'hMax', 'sMin', 'sMax', 'vHsvMin', 'vHsvMax'];
            sliders.forEach(id => {
                document.getElementById(id + 'Val').textContent = document.getElementById(id).value;
            });
            
            displayFilteredImages();
        }
        
        // Capture and process image
        async function captureImage() {
            try {
                updateStatus('Capturing...');
                
                const response = await fetch("/yuv");
                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}`);
                }
                
                const yuv422Data = new Uint8Array(await response.arrayBuffer());
                
                // Convert to structured formats
                currentYuvData = yuv422ToYUV(yuv422Data);
                currentHsvData = yuv422ToHSV(yuv422Data);
                
                // Display all images
                displayYUVImage(currentYuvData, yuvCanvas, yuvCtx);
                displayHSVImage(currentHsvData, hsvCanvas, hsvCtx);
                displayFilteredImages();
                displayRegions();
                
                updateStatus('Captured');
                
            } catch (error) {
                console.error("Capture failed:", error);
                updateStatus(`Error: ${error.message}`);
            }
        }

        
        // Auto capture mode
        function toggleAuto() {
            autoMode = !autoMode;
            autoBtn.textContent = `Auto: ${autoMode ? 'ON' : 'OFF'}`;
            
            if (autoMode) {
                autoInterval = setInterval(captureImage, 1000);
                updateStatus('Auto mode ON');
            } else {
                clearInterval(autoInterval);
                autoInterval = null;
                updateStatus('Auto mode OFF');
            }
        }
        
        // Reset all filters
        function resetFilters() {
            document.getElementById('yMin').value = 0;
            document.getElementById('yMax').value = 255;
            document.getElementById('uMin').value = 0;
            document.getElementById('uMax').value = 255;
            document.getElementById('vMin').value = 0;
            document.getElementById('vMax').value = 255;
            
            document.getElementById('hMin').value = 0;
            document.getElementById('hMax').value = 179;
            document.getElementById('sMin').value = 0;
            document.getElementById('sMax').value = 255;
            document.getElementById('vHsvMin').value = 0;
            document.getElementById('vHsvMax').value = 255;
            
            updateFilters();
        }
        
        // Region management
        function addRegion() {
            const x = parseInt(document.getElementById('regionX').value) || 0;
            const y = parseInt(document.getElementById('regionY').value) || 0;
            const width = parseInt(document.getElementById('regionW').value) || 80;
            const height = parseInt(document.getElementById('regionH').value) || 60;
            
            // Clamp values
            const clampedX = Math.max(0, Math.min(159, x));
            const clampedY = Math.max(0, Math.min(119, y));
            const clampedW = Math.max(1, Math.min(160 - clampedX, width));
            const clampedH = Math.max(1, Math.min(120 - clampedY, height));
            
            regions.push({
                x: clampedX,
                y: clampedY,
                width: clampedW,
                height: clampedH
            });
            
            updateRegionsList();
            displayRegions();
        }
        
        function clearRegions() {
            regions = [];
            updateRegionsList();
            displayRegions();
        }
        
        function updateRegionsList() {
            const list = document.getElementById('regionsList');
            list.innerHTML = '';
            
            regions.forEach((region, index) => {
                const div = document.createElement('div');
                div.style.cssText = 'margin: 5px 0; padding: 5px; background: #333; border-radius: 3px; display: flex; justify-content: space-between; align-items: center;';
                
                div.innerHTML = `
                    <span>Region ${index}: (${region.x}, ${region.y}) ${region.width}×${region.height}</span>
                    <button onclick="removeRegion(${index})" style="background: #f44; padding: 2px 8px; border: none; border-radius: 3px; color: white; cursor: pointer;">×</button>
                `;
                
                list.appendChild(div);
            });
        }
        
        function removeRegion(index) {
            regions.splice(index, 1);
            updateRegionsList();
            displayRegions();
        }
        
        // Initialize
        updateFilters();
        captureImage();
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleYUV() {
  // Capture fresh image
  if (!captureYUVImage()) {
    server.send(500, "text/plain", "Capture failed");
    return;
  }
  
  int data_size = yuv_buffer.width * yuv_buffer.height * 2;
  
  // Send binary YUV data directly
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Length", String(data_size));
  server.sendHeader("Cache-Control", "no-cache");
  
  server.send_P(200, "application/octet-stream", (const char*)yuv_buffer.yuv_data, data_size);
  
  Serial.printf("Sent %d bytes YUV binary data\n", data_size);
}

// ========================================
// MAIN SETUP
// ========================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("ESP32 YUV Camera Server");
  
  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera init failed!");
    while (1) delay(5000);
  }
  Serial.println("Camera OK");
  
  // Initialize flash
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  setFlash(false);
  
  // Connect WiFi
  Serial.printf("Connecting to %s...", ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  
  // Setup routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/yuv", HTTP_GET, handleYUV);
  
  server.begin();
  Serial.println("Server started");
  
  // Test capture
  captureYUVImage();
  Serial.println("Ready!");
}

// ========================================
// MAIN LOOP
// ========================================
void loop() {
  server.handleClient();
  
  // Serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "CAPTURE") {
      captureYUVImage();
    } else if (cmd == "STATUS") {
      Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "OK" : "FAIL");
      Serial.printf("YUV: %s\n", yuv_buffer.data_ready ? "Ready" : "None");
      Serial.printf("Heap: %d bytes\n", ESP.getFreeHeap());
    } else if (cmd == "FLASH") {
      static bool flash_on = false;
      flash_on = !flash_on;
      setFlash(flash_on);
      Serial.printf("Flash: %s\n", flash_on ? "ON" : "OFF");
    }
  }
  
  delay(1);
}