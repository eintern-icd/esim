#include <SPI.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ---------------- LovyanGFX Setup ----------------
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;
  lgfx::Touch_GT911  _touch_instance; // Added Touch Controller Config

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      
      cfg.pin_sclk = 12; 
      cfg.pin_mosi = 11; 
      cfg.pin_miso = 13; 
      cfg.pin_dc   = 46; 
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 45; 
      cfg.pin_rst          = 47; 
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = true; 
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true; 
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 0;
      cfg.x_max      = 239;
      cfg.y_min      = 0;
      cfg.y_max      = 319;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      
      // I2C pins for NORVI XN Touch Controller
      cfg.i2c_port   = I2C_NUM_0;
      cfg.pin_sda    = 8;
      cfg.pin_scl    = 9;
      cfg.freq       = 400000;
      
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft; 

int counterValue = 0;
bool isTouched = false;

// Button Geometries
const int btnMinusX = 20;
const int btnMinusY = 220;
const int btnMinusW = 80;
const int btnMinusH = 60;

const int btnPlusX = 140;
const int btnPlusY = 220;
const int btnPlusW = 80;
const int btnPlusH = 60;

void drawHeader() {
  tft.fillRect(0, 0, 240, 50, tft.color565(0, 102, 204)); // Deep Blue
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.drawString("NORVI XN", 120, 25);
  // Add a neat underline
  tft.fillRect(0, 50, 240, 4, tft.color565(0, 255, 255)); // Cyan underline
}

void drawCounter() {
  // Clear the area where the number is
  tft.fillRect(10, 80, 220, 100, TFT_BLACK); 
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(6);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.drawNumber(counterValue, 120, 130);
}

void drawButtons() {
  // Minus Button
  tft.fillRoundRect(btnMinusX, btnMinusY, btnMinusW, btnMinusH, 12, tft.color565(220, 20, 60)); // Crimson Red
  tft.drawRoundRect(btnMinusX, btnMinusY, btnMinusW, btnMinusH, 12, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.drawString("-", btnMinusX + (btnMinusW/2), btnMinusY + (btnMinusH/2));

  // Plus Button
  tft.fillRoundRect(btnPlusX, btnPlusY, btnPlusW, btnPlusH, 12, tft.color565(50, 205, 50)); // Lime Green
  tft.drawRoundRect(btnPlusX, btnPlusY, btnPlusW, btnPlusH, 12, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.drawString("+", btnPlusX + (btnPlusW/2), btnPlusY + (btnPlusH/2));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- NORVI XN Interactive UI ---");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK); 

  drawHeader();
  drawCounter();
  drawButtons();
}

void loop() {
  uint16_t x, y;
  
  // If the screen is touched
  if (tft.getTouch(&x, &y)) {
    if (!isTouched) {
      isTouched = true;
      Serial.printf("Touch detected at X:%d Y:%d\n", x, y);

      // Check if touch is inside the Minus Button
      if (x >= btnMinusX && x <= (btnMinusX + btnMinusW) && y >= btnMinusY && y <= (btnMinusY + btnMinusH)) {
        counterValue--;
        drawCounter();
        // Visual Touch Feedback (Flash White)
        tft.fillRoundRect(btnMinusX, btnMinusY, btnMinusW, btnMinusH, 12, TFT_WHITE);
        delay(80);
        drawButtons(); // Restore colors
      }
      
      // Check if touch is inside the Plus Button
      if (x >= btnPlusX && x <= (btnPlusX + btnPlusW) && y >= btnPlusY && y <= (btnPlusY + btnPlusH)) {
        counterValue++;
        drawCounter();
        // Visual Touch Feedback (Flash White)
        tft.fillRoundRect(btnPlusX, btnPlusY, btnPlusW, btnPlusH, 12, TFT_WHITE);
        delay(80);
        drawButtons(); // Restore colors
      }
    }
  } else {
    isTouched = false; // Reset touch state when finger is lifted
  }
  
  delay(20); // Small delay to keep the loop stable
}