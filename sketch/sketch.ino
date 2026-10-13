void setup() {
  // Start serial communication at 115200 baud rate
  Serial.begin(115200);
  
  // Wait a moment for the serial connection to establish
  delay(1000); 
  
  Serial.println("ESP32-S3 is awake and running!");
}

void loop() {
  // Print a message every 2 seconds
  Serial.println("Looping...");
  delay(2000);
}