#include "secrets.h"
#include <WiFiNINA.h> // change to #include <WiFi101.h> for MKR1000

#include <WiFiSSLClient.h>
#include <MQTT.h>

#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>

//sensor library
#include <Arduino_MKRENV.h>

#include <ArduinoJson.h>

int status = WL_IDLE_STATUS;
WiFiClient    wifiClient;

Client *netClient;
CloudIoTCoreDevice *device;
CloudIoTCoreMqtt *mqtt;
MQTTClient *mqttClient;
unsigned long iat = 0;
String jwt;

void connect() {
  mqtt->mqttConnect();
}

unsigned long getTime() {
  // get the current time from the WiFi module
  return WiFi.getTime();
}


String publishReadings() {
  String jsonOutput;
  StaticJsonDocument<300> doc;
  Serial.println("Publishing message");
  float temperature = ENV.readTemperature(FAHRENHEIT);
  float humidity    = ENV.readHumidity();
  float pressure    = ENV.readPressure(PSI);
  float illuminance = ENV.readIlluminance(FOOTCANDLE);
  
  doc["timestamp"] = getTime();
  doc["temp"] = temperature;
  doc["humidity"] = humidity;
  doc["pressure"] = pressure;
  doc["illuminance"] = illuminance;
  serializeJson(doc,Serial);
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
}

bool publishTelemetry(String data) {
  return mqtt->publishTelemetry(data);
}

bool publishTelemetry(const char* data, int length) {
  return mqtt->publishTelemetry(data, length);
}

bool publishTelemetry(String subfolder, String data) {
  return mqtt->publishTelemetry(subfolder, data);
}

bool publishTelemetry(String subfolder, const char* data, int length) {
  return mqtt->publishTelemetry(subfolder, data, length);
}

String getJwt() {
  // Disable software watchdog as these operations can take a while.
  Serial.println("Refreshing JWT");
  iat = WiFi.getTime();
  jwt = device->createJWT(iat, jwt_exp_secs);
  return jwt;
}

void setupCloudIoT() {
  device = new CloudIoTCoreDevice(
      project_id, location, registry_id, device_id,
      private_key_str);

  netClient = new WiFiSSLClient();

  mqttClient = new MQTTClient(512);
  mqttClient->setOptions(180, true, 1000); // keepAlive, cleanSession, timeout
  mqtt = new CloudIoTCoreMqtt(mqttClient, netClient, device);
  mqtt->startMQTT();
}

void setup() {
  Serial.begin(9600);

  if (!ENV.begin()) {
    Serial.println("Failed to initialize MKR ENV shield!");
    while (1);
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, password);

  }
    Serial.println("Waiting on time sync...");
    while (WiFi.getTime() < 1510644967) {
      delay(10);
    }

    delay(5000);
    Serial.println("setting up iot");
    setupCloudIoT();

}

unsigned long lastMillis = 0;
void loop() {
  mqtt->loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!mqttClient->connected()) {
    connect();
  }

  // publish a message roughly every second.
  if (millis() - lastMillis > 20000) {
    lastMillis = millis();
    publishTelemetry(publishReadings());
  }
}