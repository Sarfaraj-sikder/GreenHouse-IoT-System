#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <OneWire.h>

//credentials for the AP
const char* ssid = "MyPiAP"; 
const char* password = "raspberry"; 
const char* mqtt_server = "192.168.5.1"; //static IP set in the RPI AP
//mosquitto broker authentication
const char* mqtt_username = "ESP";
const char* mqtt_password = "IoTsensor";

WiFiClient espClient;
PubSubClient client(espClient);

// const int oneWireBus = D2; // Connect the Digital input pin D2
const int oneWireBus = D6; // Connect the Digital input pin D6
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
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
      client.subscribe("config/sampleRate/Temperature");
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
  Serial.begin(115200); //BAUD RATE
  sensors.begin();
  //sensors.requestTemperatures();
  setup_wifi();
  client.setServer(mqtt_server, 1883); //1883 -->  MQTT Port
  client.setCallback(handleConfig);

  //debugging
  if (client.subscribe("config/sampleRate/Temperature")) {
    Serial.println("Subscribed to topic: config/sampleRate/Temperature");
  } else {
    Serial.println("Failed to subscribe to topic: config/sampleRate/Temperature");
  }
}


void loop() {
  sensors.requestTemperatures();
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); //processing incoming messages, maintaining the connection to the MQTT broker, and handling other networking-related tasks.

   

    // check the samplerate desired and collect data
   if (millis() - lastSampleTime >= sampleRate) {
   float tempSensor_value = sensors.getTempCByIndex(0); // Gets the values of the temperature

// send json data over MQTT
  StaticJsonDocument<32> doc;
  doc["t6"] = tempSensor_value;

  char output[55];
  serializeJson(doc, output);

  client.publish("device/Sensors", output);

//Update lastSampleTime for next sensor reading

  lastSampleTime = millis();
  
  // Reading Temperature in degree Celsius!
  
  Serial.print("Temperature C:  ");
  Serial.println(tempSensor_value);
  
  }
}
