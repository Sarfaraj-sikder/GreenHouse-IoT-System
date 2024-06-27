#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <DHT.h> 

const char* ssid = "MyPiAP";
const char* password = "raspberry"; 
const char* mqtt_server = "192.168.5.1";
const char* mqtt_username = "ESP";
const char* mqtt_password = "IoTsensor";

// #define DHTPIN 2 // GPIO 2 (D4)
#define DHTPIN D5 // GPIO 14 (D5)
#define DHTTYPE DHT11 // DHT 11
DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long sampleRate = 10000;  // Default sample rate: 10 seconds
unsigned long lastSampleTime=0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
//setting ESP in station mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void handleConfig(char* topic, byte* payload, unsigned int length) {
  
  // Handle incoming configuration messages
  DynamicJsonDocument doc(256);
  deserializeJson(doc, payload, length);

  Serial.print("Message received on topic: ");
  Serial.println(topic);

  Serial.print("Message arrived ");
  serializeJson(doc, Serial);
  Serial.println();

  if (doc.containsKey("sampleRate")) {
  if (doc["sampleRate"].is<long>()) {
    sampleRate = doc["sampleRate"].as<long>();
    Serial.print("Updated sampleRate: ");
      Serial.println(sampleRate);
  } else {
    Serial.println("Invalid sampleRate value. Expected a long integer.");
    client.publish("device/Sensors", "Invalid sampleRate value. Expected a long integer.");
  }
}
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
    // if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("device/Sensors", "MQTT Server is connected");
      
      // ... and resubscribe
      client.subscribe("config/sampleRate/DHT11");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  dht.begin();
  client.setServer(mqtt_server, 1883); //1883 -->  MQTT Port
  client.setCallback(handleConfig);
 

  
//debugging
  if (client.subscribe("config/sampleRate/DHT11")) {
    Serial.println("Subscribed to topic: config/sampleRate/DHT11");
  } else {
    Serial.println("Failed to subscribe to topic: config/sampleRate/DHT11");
  }

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); //processing incoming messages, maintaining the connection to the MQTT broker, and handling other networking-related tasks.

// check the samplerate desired and collect data
   if (millis() - lastSampleTime >= sampleRate) {
      float h = dht.readHumidity();
      // Read temperature as Celsius (the default)
      float t = dht.readTemperature();

      // Check if any reads failed and exit early (to try again).
      if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT11 sensor!");
      return;
      }

// send json data over MQTT
  StaticJsonDocument<32> doc;
  doc["HUM"] = h;
  doc["temp"] = t;

  char output[55];
  serializeJson(doc, output);

  client.publish("device/Sensors", output);

//Update lastSampleTime for next sensor reading

  lastSampleTime = millis();
  
  // Reading Pressure in KPa!
  
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(", Temperature: ");
  Serial.println(t);
  
  }
}
