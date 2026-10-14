#include <SPI.h>
#include <Wire.h> // Required for the I2C IO Expander LEDs

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Include your perfectly sized 240x320 image array
#include "Untitled_Design.h"

// --- NORVI I2C LED Expander Settings ---
#define I2C_SDA 8
#define I2C_SCL 9
#define IO_EXPANDER_ADDR 0x41
#define REG_OUTPUT 0x01
#define REG_CONFIG 0x03

// ---------------- LovyanGFX Setup ----------------
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;

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
    setPanel(&_panel_instance);
  }
};

LGFX tft; 

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- NORVI Boot Sequence Started ---");

  // ---------------- BLINK LEDs (via I2C IO Expander) ----------------
  Serial.println("Blinking RUN and ERROR LEDs...");
  
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Configure the IO Expander: Set P1 (ERROR) and P2 (RUN) as outputs (0), P0 and P3 as inputs (1)
  // Binary 1001 = 0x09
  Wire.beginTransmission(IO_EXPANDER_ADDR);
  Wire.write(REG_CONFIG);
  Wire.write(0x09); 
  Wire.endTransmission();

  // Blink the LEDs 3 times
  for (int i = 0; i < 3; i++) {
    // Turn LEDs ON (Set P1 and P2 HIGH -> Binary 0110 = 0x06)
    Wire.beginTransmission(IO_EXPANDER_ADDR);
    Wire.write(REG_OUTPUT);
    Wire.write(0x06); 
    Wire.endTransmission();
    
    delay(300); // Wait 300ms
    
    // Turn LEDs OFF (Set P1 and P2 LOW -> Binary 0000 = 0x00)
    Wire.beginTransmission(IO_EXPANDER_ADDR);
    Wire.write(REG_OUTPUT);
    Wire.write(0x00); 
    Wire.endTransmission();
    
    delay(300); // Wait 300ms
  }

  // ---------------- DISPLAY SETUP ----------------
  Serial.println("Initializing Display...");
  
  // Initialize the display
  tft.init();
  
  // Fill the screen with white so the transparent PNG background shows as white, not black
  tft.fillScreen(TFT_WHITE); 

  Serial.println("Drawing Untitled_Design PNG...");

  // Draw the image starting at the top-left corner (0, 0)
  tft.drawPng(Untitled_Design, sizeof(Untitled_Design), 0, 0);
  
  Serial.println("Image drawn successfully!");
}

void loop() {
  // Do nothing, just display the image
  delay(1000);
}