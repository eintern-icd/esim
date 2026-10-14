#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi Credentials
const char* ssid = "ICONIC DEVICES (PVT) LTD";
const char* password = "YOUR_WIFI_PASSWORD";

// HiveMQ Public Broker Details
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID to avoid conflicts on the public broker
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect (Public broker doesn't require username/password)
    if (client.connect(clientId.c_str())) {
      Serial.println("connected to HiveMQ!");
      
      // Example of subscribing to a topic
      // client.subscribe("norvi/test/topic/in");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // 1. Connect to Wi-Fi
  setup_wifi();
  
  // 2. Configure MQTT Broker
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // 3. Ensure MQTT connection stays alive
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 4. Publish a test message every 5 seconds
  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    
    // Publish to a specific topic
    client.publish("norvi/test/topic/out", "Hello from ESP32!");
    Serial.println("Message published to HiveMQ");
  }
}