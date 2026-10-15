# Project Context: NORVI X1 (ESP32-S3) TFT Display Configuration

## 1. Hardware Architecture & Display Specifications
* **Microcontroller:** ESP32-S3 (NORVI X1 Board)
* **Display Driver:** ST7789 TFT LCD
* **Native Resolution:** 240 (Width) x 320 (Height) pixels
* **Default Orientation:** Portrait (`offset_rotation = 0`)
* **Color Depth:** 16-bit RGB565 (supports up to 65,536 colors)
* **Communication Interface:** Hardware SPI (SPI2_HOST)

## 2. MCU Pin Configuration (ESP32-S3 to ST7789)
The display is wired to the ESP32-S3 using the following GPIO pins:
* **SCLK (Clock):** GPIO 12
* **MOSI (Master Out Slave In):** GPIO 11
* **MISO (Master In Slave Out):** GPIO 13
* **DC (Data / Command):** GPIO 46
* **CS (Chip Select):** GPIO 45
* **RST (Reset):** GPIO 47
* **Busy Pin:** Not used (`-1`)

## 3. Libraries Used
* **`LovyanGFX` (`<LovyanGFX.hpp>`)**: A highly optimized graphics library for ESP32. It is used here because it offers high framerates, custom bus configurations, and built-in image decoding (like `.drawPng()`) directly from PROGMEM (Flash memory).
* **`SPI` (`<SPI.h>`)**: Standard Arduino SPI library required for the hardware bus interface.

## 4. Capabilities & Future Possibilities
* **Graphics & Images:** The setup currently decodes and draws a static PNG byte array (`Untitled_Design.h`) directly to the screen at coordinates (0,0). LovyanGFX can also draw BMPs, JPEGs, and geometric primitives (lines, circles, filled rectangles).
* **Colors:** Colors are handled in 16-bit format. You can use macros like `TFT_WHITE`, `TFT_BLACK`, or define custom RGB colors using `tft.color565(r, g, b)`.
* **Touch Support:** 
  * The ST7789 controller itself handles *only* the visual display. 
  * If the specific NORVI screen module includes a capacitive or resistive touch overlay (e.g., GT911, XPT2046, or FT6236), touch *can* be enabled. 
  * To enable touch, an `lgfx::Touch` configuration block must be added to the `LGFX` class in the code, mapping the specific I2C or SPI pins used by the touch controller. (Currently, touch is not configured).

## 5. Verified Source Code (`sketch.ino`)
This code initializes the display bus, sets the background, and draws a 240x320 PNG image.

```cpp
#include <SPI.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Include your perfectly sized 240x320 image array
#include "Untitled_Design.h"

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

  Serial.println("\n--- NORVI Display Boot ---");

  // Initialize the display
  tft.init();
  
  // Fill the screen with white so the transparent PNG background shows as white
  tft.fillScreen(TFT_WHITE); 

  Serial.println("Drawing Untitled_Design PNG...");

  // Draw the image starting at the top-left corner (0, 0)
  tft.drawPng(Untitled_Design, sizeof(Untitled_Design), 0, 0);
  
  Serial.println("Image drawn successfully!");
}

void loop() {
  delay(1000);
}
