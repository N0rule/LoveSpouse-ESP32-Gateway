#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bluetooth_service.h"

#include "muse.h"
#include "lovense.h"

// Add TFT_eSPI library for display
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

static const char* TAG = "main";

// Hardware configuration
SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// Display constants
constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 240;
constexpr int BORDER_SIZE = 4;
constexpr int PADDING = 10;
constexpr int TOP_NAME_HEIGHT = 35;
float globalIntensity = 0.0;
// Color palette
uint16_t LIGHTEST_COLOR = 0xDED6; // White
uint16_t LIGHT_COLOR = 0xA534;    // Blue
uint16_t DARK_COLOR = 0x7408;     // Dark Blue
uint16_t DARKEST_COLOR = 0x4284;  // Darkest Blue

// Gauge variables
float currentIntensity = 0.0;
bool isConnected = false;

// Function prototypes
void drawDisplay();
void drawGauge(float intensity);
void updateDisplayTask(void *pvParameters);
void cycleBrightness();

// Brightness cycling
int currentBrightnessIndex = 1; // Start at 50%
const int BRIGHTNESS_LEVELS[] = {0, 50, 75, 100};
unsigned long lastTapTime = 0;
bool isTouching = false;

// Add this function to handle touch events:
void handleTouch() {
  if (ts.tirqTouched() && ts.touched() && !isTouching) {
    isTouching = true;
    TS_Point p = ts.getPoint();
    
    // Cycle brightness on tap
    cycleBrightness();
    
    Serial.println("Touch: X=" + String(p.x) + " Y=" + String(p.y) + " P=" + String(p.z));
  }
  else if (!ts.tirqTouched()) {
    isTouching = false;
  }
}

// Add this function to cycle through brightness levels:
void cycleBrightness() {
  currentBrightnessIndex = (currentBrightnessIndex + 1) % 4;
  analogWrite(TFT_BL, BRIGHTNESS_LEVELS[currentBrightnessIndex] * 2.55);
  Serial.println("Brightness: " + String(BRIGHTNESS_LEVELS[currentBrightnessIndex]) + "%");
}

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }

  Serial.println("Lovense Gateway started!");
  Serial.println();

  // Initialize touch screen
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(3);

  // Initialize display
  tft.init();
  tft.setRotation(3);
  
  // Initialize backlight
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, BRIGHTNESS_LEVELS[currentBrightnessIndex] * 2.55);
  
  // Draw initial display
  drawDisplay();

  bluetooth_service_init();

  lovense_init();
  muse_init();
  muse_start();

  bluetooth_service_start();

  // Create task to update the display
  xTaskCreatePinnedToCore(updateDisplayTask, "update_display_task", 4096, nullptr, 1, nullptr, 1);

  ESP_LOGI(TAG, "SETUP DONE.");
}

void loop() {
  // Handle touch events
  handleTouch();
  
  // Main loop still has a delay for system stability
  delay(100);
}

void drawGauge(float intensity) {
  int gaugeWidth = SCREEN_WIDTH - (BORDER_SIZE * 2) - (PADDING * 2);
  int fillWidth = int(gaugeWidth * intensity);
  
  // Clear previous gauge fill
  tft.fillRect(BORDER_SIZE + PADDING, 
               SCREEN_HEIGHT - 80, 
               gaugeWidth, 
               40, 
               DARKEST_COLOR);
  
  // Draw new gauge fill
  tft.fillRect(BORDER_SIZE + PADDING, 
               SCREEN_HEIGHT - 80, 
               fillWidth, 
               40, 
               LIGHT_COLOR);
  
  // Draw tick marks
  for (int i = 0; i <= 20; i++) {
    int x = BORDER_SIZE + PADDING + (i * gaugeWidth / 20);
    int tickHeight = (i % 5 == 0) ? 10 : 5;
    tft.drawLine(x, SCREEN_HEIGHT - 80, x, SCREEN_HEIGHT - 80 + tickHeight, LIGHTEST_COLOR);
    
    // Draw labels at major ticks
    if (i % 5 == 0) {
      tft.setTextColor(LIGHTEST_COLOR);
      tft.setTextSize(1);
      tft.setCursor(x - 3, SCREEN_HEIGHT - 65);
      tft.print(i);
    }
  }
}

void drawDisplay() {
  tft.startWrite();
  
  // Clear screen
  tft.fillScreen(DARK_COLOR);
  
  // Draw border
  tft.fillRect(0, 0, SCREEN_WIDTH, BORDER_SIZE, LIGHTEST_COLOR);
  tft.fillRect(0, SCREEN_HEIGHT - BORDER_SIZE, SCREEN_WIDTH, BORDER_SIZE, LIGHTEST_COLOR);
  tft.fillRect(0, 0, BORDER_SIZE, SCREEN_HEIGHT, LIGHTEST_COLOR);
  tft.fillRect(SCREEN_WIDTH - BORDER_SIZE, 0, BORDER_SIZE, SCREEN_HEIGHT, LIGHTEST_COLOR);
  
  // Header bar
  tft.fillRect(BORDER_SIZE, BORDER_SIZE,
               SCREEN_WIDTH - (BORDER_SIZE * 2),
               TOP_NAME_HEIGHT,
               DARKEST_COLOR);
  
  tft.endWrite();
  
// Draw connection status - centered in header bar
tft.setTextColor(LIGHTEST_COLOR);
tft.setTextSize(2);
const char* statusText = isConnected ? "Connected" : "Waiting...";
int textWidth = strlen(statusText) * 12;
int statusX = (SCREEN_WIDTH - textWidth) / 2;
int statusY = BORDER_SIZE + (TOP_NAME_HEIGHT - 16) / 2; // Center text vertically in header
tft.setCursor(statusX, statusY);
tft.println(statusText);
  
  // Draw title - centered with larger text and lightest color
  tft.setTextColor(LIGHTEST_COLOR); // Changed to lightest color
  tft.setTextSize(3); // Increased size
  int titleWidth = 18 * 15; // Approximate width of "Lovense Gateway" text at size 3
  int titleX = (SCREEN_WIDTH - titleWidth) / 2;
  int titleY = BORDER_SIZE + TOP_NAME_HEIGHT + 30; // More space for larger text
  tft.setCursor(titleX, titleY);
  tft.println("Lovense Gateway");
  
  // Draw intensity label - same size as value
  tft.setTextColor(LIGHTEST_COLOR);
  tft.setTextSize(3); // Matched with value size
  int labelY = SCREEN_HEIGHT - 110;
  tft.setCursor(BORDER_SIZE + PADDING, labelY);
  tft.println("Intensity:");
  
  // Draw intensity value - same size as label
  tft.setTextColor(LIGHT_COLOR); // Changed to lightest color
  tft.setTextSize(3); // Already size 3
  tft.setCursor(SCREEN_WIDTH - 60, labelY);
  tft.print(int(currentIntensity * 20));
  
  // Draw gauge
  drawGauge(currentIntensity);
}

void updateConnectionStatus(bool connected) {
  if (connected != isConnected) {
    isConnected = connected;
    
    // Get the status text
    const char* statusText = isConnected ? "Connected" : "Waiting...";
    
    // Calculate text width (approximate: each character is about 12 pixels wide at text size 2)
    int textWidth = strlen(statusText) * 12;
    
    // Calculate center position in header
    int statusX = (SCREEN_WIDTH - textWidth) / 2;
    
    // Update only connection status part of the display
    tft.fillRect(BORDER_SIZE, BORDER_SIZE, SCREEN_WIDTH - (BORDER_SIZE * 2), TOP_NAME_HEIGHT, DARKEST_COLOR);
    tft.setTextColor(LIGHTEST_COLOR);
    tft.setTextSize(2);
    int statusY = BORDER_SIZE + (TOP_NAME_HEIGHT - 16) / 2; // Center text vertically in header
    tft.setCursor(statusX, statusY);
    tft.println(statusText);
  }
}

void updateDisplayTask(void *pvParameters) {
  float lastIntensity = -1.0;
  bool lastConnected = !isConnected;
  
  while (1) {
    // Get current intensity from muse module
    float newIntensity = get_current_vibration_speed() / 20.0f;
    
    // Check connection status from BLE
    bool newConnected = NimBLEDevice::getServer()->getConnectedCount() > 0;
    
    // Update display only if values changed
    if (newIntensity != lastIntensity || newConnected != lastConnected) {
      currentIntensity = newIntensity;
      updateConnectionStatus(newConnected);
      
      // Update intensity display - same size and color
      tft.fillRect(SCREEN_WIDTH - 70, SCREEN_HEIGHT - 110, 65, 30, DARK_COLOR);
      tft.setTextColor(LIGHT_COLOR); // Changed to lightest color
      tft.setTextSize(3);
      tft.setCursor(SCREEN_WIDTH - 60, SCREEN_HEIGHT - 110);
      tft.print(int(currentIntensity * 20));
      
      // Update gauge
      drawGauge(currentIntensity);
      
      lastIntensity = currentIntensity;
      lastConnected = newConnected;
    }
    
    delay(50); // Update every 50ms
  }
}
