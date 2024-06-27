#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* ssid = "MyPiAP";
const char* password = "raspberry"; 
const char* mqtt_server = "192.168.5.1";
const char* mqtt_username = "ESP";
const char* mqtt_password = "IoTsensor";


WiFiClient espClient;
PubSubClient client(espClient);
const int analogPin = A0; // Connect the analog output of the sensor to A0
unsigned long sampleRate = 10000;  // Default sample rate: 10 seconds
unsigned long lastSampleTime=0;

float slope_MPX5100DP = 1; // Default slope for MPX5100DP calibration for tensiometer
float intercept_MPX5100DP = 0; // Default intercept for MPX5100DP calibration for tensiometer

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

if (doc.containsKey("slope_MPX5100DP")) {
    if (doc["slope_MPX5100DP"].is<float>()) {
      slope_MPX5100DP = doc["slope_MPX5100DP"].as<float>();
      Serial.print("Updated slope_MPX5100DP: ");
      Serial.println(slope_MPX5100DP);
    } else {
      Serial.println("Invalid slope_MPX5100DP value. Expected a floating point number.");
      client.publish("device/Sensors", "Invalid slope_MPX5100DP value. Expected a floating point number.");
    }
  }

if (doc.containsKey("intercept_MPX5100DP")) {
    if (doc["intercept_MPX5100DP"].is<float>()) {
      intercept_MPX5100DP = doc["intercept_MPX5100DP"].as<float>();
      Serial.print("Updated intercept_MPX5100DP: ");
      Serial.println(intercept_MPX5100DP);
    } else {
      Serial.println("Invalid intercept_MPX5100DP value. Expected a floating point number.");
      client.publish("device/Sensors", "Invalid intercept_MPX5100DP value. Expected a floating point number.");
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
      client.subscribe("config/sampleRate/Pressure");
      client.subscribe("config/slope_MPX5100DP");
      client.subscribe("config/intercept_MPX5100DP");
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
  client.setServer(mqtt_server, 1883); //1883 -->  MQTT Port
  client.setCallback(handleConfig);
 

  
//debugging
  if (client.subscribe("config/sampleRate/Pressure")) {
    Serial.println("Subscribed to topic: config/sampleRate/Pressure");
  } else {
    Serial.println("Failed to subscribe to topic: config/sampleRate/Pressure");
  }

  if (client.subscribe("config/slope_MPX5100DP")) {
    Serial.println("Subscribed to topic: config/slope_MPX5100DP");
  } else {
    Serial.println("Failed to subscribe to topic: config/slope_MPX5100DP");
  }

  if (client.subscribe("config/intercept_MPX5100DP")) {
    Serial.println("Subscribed to topic: config/intercept_MPX5100DP");
  } else {
    Serial.println("Failed to subscribe to topic: config/intercept_MPX5100DP");
  }


}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); //processing incoming messages, maintaining the connection to the MQTT broker, and handling other networking-related tasks.

// check the samplerate desired and collect data
   if (millis() - lastSampleTime >= sampleRate) {
   int sensorValue = analogRead(analogPin);
   float voltage = sensorValue * (3.3 / 1023.0); // Convert ADC value to voltage
   float pressure = (voltage - 0.132) / 45 * 1000; // Convert voltage to pressure using the sensor's specifications, sensitivity=45 kPa, offset=0.132
  //  float slope_MPX5100DP = 1.094947308; // for MPX5100DP calibration for tensiometer
  //  float intercept_MPX5100DP = -5.301696878; // for MPX5100DP calibration for tensiometer
  //  float slope_MPX5100DP = 1; // for MPX5100DP calibration for tensiometer
  //  float intercept_MPX5100DP = 0; // for MPX5100DP calibration for tensiometer
   float kPa = slope_MPX5100DP * pressure +intercept_MPX5100DP;
   float hPa = kPa/ 10;

// send json data over MQTT
  StaticJsonDocument<32> doc;
  doc["p5"] = hPa;

  char output[55];
  serializeJson(doc, output);

  client.publish("device/Sensors", output);

//Update lastSampleTime for next sensor reading

  lastSampleTime = millis();
  
  // Reading Pressure in KPa!
  
  Serial.print("Sensor Value: ");
  Serial.print(sensorValue);
  Serial.print(", Voltage: ");
  Serial.print(voltage);
  Serial.print(", Pressure hPA: ");
  Serial.println(hPa);

  }
}
