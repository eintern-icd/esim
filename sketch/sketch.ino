// "NORVI X-CPU-ESPS3-X1 -V2-N16R2" - FAST LOOP WITH BUTTONS & FIXED RTC

#include <Wire.h>
#include <WiFi.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "PCA9539.h"
#include <PCA9536D.h>
#include "RTClib.h"
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include <CST816S.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ---------------- PINS ----------------
#define SDA   8     
#define SCL   9     

#define RS485_RXD 16
#define RS485_TXD 15
#define RS485_FC  41

#define ETH_CS 1
#define DSP_CS 45
#define SD_chipSelect 42

#define MISO 13
#define MOSI 11
#define SCLK 12

#define CS4      PCA_A5 
#define CS3      PCA_A6 
#define CS2      PCA_A7 
#define CS1      PCA_B0 
#define OUTPUT19 PCA_B1
#define OUTPUT20 PCA_B2
#define OUTPUT33 PCA_B3
#define OUTPUT34 PCA_B4
#define OUTPUT38 PCA_B5

#define PORT_RST 38

#define OUTPUT5  5       
#define OUTPUT6  6
#define OUTPUT7  7
#define OUTPUT10 10
#define OUTPUT11 35      
#define OUTPUT12 36      
#define OUTPUT13 37      
#define OUTPUT14 14
#define OUTPUT21 21
#define OUTPUT39 39
#define OUTPUT40 40

#define TX1 17
#define RX1 18

// PCA9536 Pins (Indicators & Buttons)
#define IO_PB1  0
#define IO_LED1 1
#define IO_LED2 2
#define IO_PB3  3

// ---------------- OBJECTS ----------------
PCA9539 ioport(0x75);
PCA9536 io;
RTC_DS3231 rtc; 
CST816S touch(8, 9, 47, 48);  

// ---------------- LOVYANGFX SETUP ----------------
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

// ---------------- ETHERNET & TIMERS ----------------
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
unsigned int localPort = 8888;       
const char timeServer[] = "time.nist.gov"; 
const int NTP_PACKET_SIZE = 48; 
byte packetBuffer[NTP_PACKET_SIZE]; 
EthernetUDP Udp;

int touchCounter = 0;
bool isTouched = false;
unsigned long lastScreenUpdate = 0; // Tracks 1-second interval

// ---------------- FUNCTION PROTOTYPES ----------------
void I2C_SCAN();
void SD_CHECK();
void RTC_Check();
void ETHERNET_CHECK();
void sendNTPpacket(const char * address);

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial2.begin(115200, SERIAL_8N1, RS485_RXD, RS485_TXD); 

  Serial.println("\n--- NORVI X-CPU-ESPS3-X1 BOOTING ---");

  pinMode(PORT_RST, OUTPUT);
  pinMode(RS485_FC, OUTPUT); 
  digitalWrite(PORT_RST, HIGH);   // WAKE UP PCA9536 & PCA9539
  delay(100);

  Wire.begin(SDA, SCL);
  SPI.begin(SCLK, MISO, MOSI); 
  delay(100);

  // Isolate SPI slaves
  pinMode(ETH_CS, OUTPUT); digitalWrite(ETH_CS, HIGH); 
  pinMode(SD_chipSelect, OUTPUT); digitalWrite(SD_chipSelect, HIGH); 
  pinMode(DSP_CS, OUTPUT); digitalWrite(DSP_CS, HIGH); 

  if (io.begin() == false) {
    Serial.println("PCA9536 not detected.");
  } else {
    io.pinMode(IO_PB1, INPUT);
    io.pinMode(IO_LED1, OUTPUT);
    io.pinMode(IO_LED2, OUTPUT);
    io.pinMode(IO_PB3, INPUT);

    io.digitalWrite(IO_LED1, HIGH);
    io.digitalWrite(IO_LED2, HIGH);
  }

  touch.begin();
  
  tft.init();
  tft.fillScreen(TFT_BLACK); 

  tft.setTextColor(TFT_YELLOW);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setCursor(45, 20);      
  tft.print("NORVI X1");

  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, 60);      
  tft.print("RTC Time:");

  tft.setTextColor(TFT_MAGENTA);
  tft.setCursor(10, 140);      
  tft.print("Touch Counter:");

  I2C_SCAN();

  ioport.pinMode(CS4, OUTPUT); ioport.pinMode(CS3, OUTPUT);
  ioport.pinMode(CS2, OUTPUT); ioport.pinMode(CS1, OUTPUT);
  ioport.pinMode(OUTPUT19, OUTPUT); ioport.pinMode(OUTPUT20, OUTPUT);
  ioport.pinMode(OUTPUT33, OUTPUT); ioport.pinMode(OUTPUT34, OUTPUT);
  ioport.pinMode(OUTPUT38, OUTPUT);

  RTC_Check();
  SD_CHECK();
  ETHERNET_CHECK();     
}

// ---------------- FAST LOOP ----------------
void loop() {
  
  // ==========================================
  // FAST TASKS (Run continuously with no delay)
  // ==========================================
  
  // 1. READ BUTTONS & CONTROL LEDS INSTANTLY
  // Assuming buttons pull LOW when pressed. LEDs are Active-LOW.
  if (io.digitalRead(IO_PB1) == LOW) {
    io.digitalWrite(IO_LED1, LOW);  // Turn LED 1 ON
  } else {
    io.digitalWrite(IO_LED1, HIGH); // Turn LED 1 OFF
  }

  if (io.digitalRead(IO_PB3) == LOW) {
    io.digitalWrite(IO_LED2, LOW);  // Turn LED 2 ON
  } else {
    io.digitalWrite(IO_LED2, HIGH); // Turn LED 2 OFF
  }

  // 2. HANDLE TOUCH ACCURATELY
  if (touch.available()) {
    if (!isTouched) { // Only count once per press
      touchCounter++; 
      isTouched = true;
      Serial.printf("Touched! X: %d, Y: %d\n", touch.data.x, touch.data.y);
    }
  } else {
    isTouched = false; // Reset when finger is removed
  }


  // ==========================================
  // SLOW TASKS (Run only once per second)
  // ==========================================
  if (millis() - lastScreenUpdate >= 1000) {
    lastScreenUpdate = millis();

    // 1. UPDATE RTC TIME
    DateTime now = rtc.now();
    tft.setTextColor(TFT_WHITE, TFT_BLACK); 
    tft.setCursor(10, 90);
    tft.printf("%02d/%02d/%04d", now.month(), now.day(), now.year());
    tft.setCursor(10, 110);
    tft.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());

    // 2. UPDATE TOUCH COUNTER
    tft.setCursor(10, 170);      
    tft.printf("Touches: %-5d", touchCounter); 
    
    // 3. RUN RS485 TEST
    digitalWrite(RS485_FC, HIGH);                  
    Serial2.println("RS485 01 SUCCESS");     
    delay(10); // Very short delay just for sending data                            
    digitalWrite(RS485_FC, LOW);                                                          
    
    while (Serial2.available()) {  
      Serial.write(Serial2.read());             
    }
  }

  delay(10); // Small 10ms rest to keep the ESP32 CPU happy
}

// ---------------- UTILITY FUNCTIONS ----------------

void I2C_SCAN() {
  byte error, address;
  int deviceCount = 0;
  Serial.println("Scanning I2C...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("I2C device found at address 0x%02X\n", address);
      deviceCount++;
      delay(1); 
    }
  }
}

void SD_CHECK(){
  Serial.print("\nInitializing SD card...");
  if(SD.begin(SD_chipSelect)) {
    Serial.printf("Success! Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
  } else {
    Serial.println("NO SD card found");            
  }
}

void RTC_Check(){
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  } else {
    // THIS FORCES THE RTC TO SET TO YOUR COMPUTER'S COMPILE TIME
    // Run this once, then you can comment out the next line for future uploads.
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    Serial.println("RTC Initialized & Time Synchronized.");
  }
}

void ETHERNET_CHECK(){
  Ethernet.init(ETH_CS); 
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  } else {
    Udp.begin(localPort);
    sendNTPpacket(timeServer); 
    delay(1000);
    if (Udp.parsePacket()) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE); 
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long epoch = (highWord << 16 | lowWord) - 2208988800UL;
      Serial.print("NTP Sync Success! UTC Unix time = ");
      Serial.println(epoch);
    }
  }
}

void sendNTPpacket(const char * address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   
  packetBuffer[2] = 6;     
  packetBuffer[3] = 0xEC;  
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  Udp.beginPacket(address, 123); 
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket(); 
}