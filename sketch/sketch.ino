// "NORVI X-CPU-ESPS3-X1 -V2-N16R2" - LOVYANGFX & CST816S (CORRECTED)

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

#define OUTPUT5  5       // Direct GPIO
#define OUTPUT6  6
#define OUTPUT7  7
#define OUTPUT10 10
#define OUTPUT11 35      
#define OUTPUT12 36      // Not allowed with PSRAM
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
CST816S touch(8, 9, 47, 48);  // sda, scl, rst, irq

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

// ---------------- ETHERNET / NTP CONFIG ----------------
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
unsigned int localPort = 8888;       
const char timeServer[] = "time.nist.gov"; 
const int NTP_PACKET_SIZE = 48; 
byte packetBuffer[NTP_PACKET_SIZE]; 
EthernetUDP Udp;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int touchCounter = 0;

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

  // ==========================================
  // CRITICAL FIX 1: ISOLATE SHARED SPI BUS
  // ==========================================
  pinMode(ETH_CS, OUTPUT);
  digitalWrite(ETH_CS, HIGH); // Turn off Ethernet SPI
  pinMode(SD_chipSelect, OUTPUT);
  digitalWrite(SD_chipSelect, HIGH); // Turn off SD SPI
  pinMode(DSP_CS, OUTPUT);
  digitalWrite(DSP_CS, HIGH); // Turn off Display SPI

  // Enable main ports
  pinMode(PORT_RST, OUTPUT);
  pinMode(RS485_FC, OUTPUT); 
  digitalWrite(PORT_RST, HIGH); // Enable PCA9539 & internal routing
  delay(100);

  // Start internal comms
  Wire.begin(SDA, SCL);
  SPI.begin(SCLK, MISO, MOSI); 
  delay(100);

  // ==========================================
  // CRITICAL FIX 2: LED ACTIVE-LOW BLINKING
  // ==========================================
  if (io.begin() == false) {
    Serial.println("PCA9536 not detected. Please check wiring.");
  } else {
    Serial.println("Blinking Device LEDs...");
    io.pinMode(IO_PB1, INPUT);
    io.pinMode(IO_LED1, OUTPUT);
    io.pinMode(IO_LED2, OUTPUT);
    io.pinMode(IO_PB3, INPUT);

    // Turn both OFF initially (HIGH = OFF)
    io.digitalWrite(IO_LED1, HIGH);
    io.digitalWrite(IO_LED2, HIGH);

    for (int i = 0; i < 2; i++) {
      io.digitalWrite(IO_LED1, LOW);  // LED 1 ON
      delay(300);
      io.digitalWrite(IO_LED1, HIGH); // LED 1 OFF
      
      io.digitalWrite(IO_LED2, LOW);  // LED 2 ON
      delay(300);
      io.digitalWrite(IO_LED2, HIGH); // LED 2 OFF
    }
  }

  // ---------------- INITIALIZE DISPLAY ----------------
  Serial.println("Initializing Display...");
  tft.init();
  tft.fillScreen(TFT_BLACK); 
  tft.setTextColor(TFT_YELLOW);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setCursor(45, 100);      
  tft.print("NORVI X1");
  tft.setCursor(45, 130);
  tft.setTextColor(TFT_WHITE);
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.print("BOOTING...");

  // ---------------- BACKGROUND INITIALIZATIONS ----------------
  I2C_SCAN();
  
  touch.begin();

  // Configure PCA9539 Output pins
  ioport.pinMode(CS4, OUTPUT);
  ioport.pinMode(CS3, OUTPUT);
  ioport.pinMode(CS2, OUTPUT);
  ioport.pinMode(CS1, OUTPUT);
  ioport.pinMode(OUTPUT19, OUTPUT);
  ioport.pinMode(OUTPUT20, OUTPUT);
  ioport.pinMode(OUTPUT33, OUTPUT);
  ioport.pinMode(OUTPUT34, OUTPUT);
  ioport.pinMode(OUTPUT38, OUTPUT);

  // Configure Direct Output pins
  pinMode(OUTPUT5,  OUTPUT);
  pinMode(OUTPUT6,  OUTPUT);
  pinMode(OUTPUT7,  OUTPUT);
  pinMode(OUTPUT10,  OUTPUT);
  pinMode(OUTPUT14,  OUTPUT);
  pinMode(OUTPUT21,  OUTPUT);
  pinMode(OUTPUT39,  OUTPUT);
  pinMode(OUTPUT40,  OUTPUT);

  RTC_Check();
  SD_CHECK();
  ETHERNET_CHECK();     

  delay(1500); // Leave boot text on screen for a moment before starting loop
}

// ---------------- LOOP ----------------
void loop() {
  // Clear screen to black for text updates
  tft.fillScreen(TFT_BLACK); 

  // --- DRAW STATIC TEXT ---
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

  // --- DRAW RTC DATA ---
  DateTime now = rtc.now();
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 90);
  tft.printf("%02d/%02d/%04d", now.month(), now.day(), now.year());
  tft.setCursor(10, 110);
  tft.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  // --- HANDLE TOUCH DATA ---
  if (touch.available()) {
    touchCounter++; 
    Serial.printf("Touched! X: %d, Y: %d, Event: %d\n", touch.data.x, touch.data.y, touch.data.event);
  }

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 170);      
  tft.printf("Touches: %d", touchCounter);

  // --- READ BUTTONS (To Serial) ---
  Serial.println("--- IO & Comms Status ---");
  Serial.print("PB1 Read: "); Serial.println(io.digitalRead(IO_PB1));
  Serial.print("PB3 Read: "); Serial.println(io.digitalRead(IO_PB3));
  
  // --- RS485 TEST ---
  digitalWrite(RS485_FC, HIGH);                  
  Serial2.println("RS485 01 SUCCESS");     
  delay(50);                              
  digitalWrite(RS485_FC, LOW);                                                          
  
  Serial.print("RS485 Rx: ");
  while (Serial2.available()) {  
    char c = Serial2.read();     
    Serial.write(c);             
  }
  Serial.println("\n-------------------------");

  delay(1000);
}

// ---------------- UTILITY FUNCTIONS ----------------

void I2C_SCAN() {
  byte error, address;
  int deviceCount = 0;
  Serial.println("Scanning I2C Bus...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      deviceCount++;
      delay(1); 
    }
  }
  if (deviceCount == 0) Serial.println("No I2C devices found\n");
  else Serial.println("Scanning complete\n");
}

void SD_CHECK(){
  Serial.print("\nInitializing SD card...");
  uint8_t cardType = SD.cardType();
  if(SD.begin(SD_chipSelect)) {
    Serial.println("Card Mount: success");
    Serial.print("Card Type: ");
    if(cardType == CARD_MMC) Serial.println("MMC");
    else if(cardType == CARD_SD) Serial.println("SDSC");
    else if(cardType == CARD_SDHC) Serial.println("SDHC");
    else Serial.println("Unknown");

    int cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("Card Size: %lluMB\n", cardSize);
  } else {
    Serial.println("NO SD card found");            
  }
}

void RTC_Check(){
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  } else {
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, lets set the time!");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    Serial.println("RTC Initialized.");
  }
}

void ETHERNET_CHECK(){
  Ethernet.init(ETH_CS); 
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.");
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
  } else {
    Udp.begin(localPort);
    sendNTPpacket(timeServer); 
    delay(1000);
    if (Udp.parsePacket()) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE); 
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      Serial.print("NTP Sync Success! UTC Unix time = ");
      Serial.println(epoch);
    }
  }
}

void sendNTPpacket(const char * address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   
  packetBuffer[1] = 0;     
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