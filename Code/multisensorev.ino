#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

Adafruit_INA219 ina219;
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
#define GAS_PIN 34
#define VOLTAGE_PIN 35
#define RELAY_PIN 18

const char* ssid      = "Connected";
const char* password  = "WifiBd197@";
const char* scriptURL = "https://script.google.com/macros/s/AKfycbynDIJUN2i0Ep5xa6d7foKhfU2_UYV_GNRcy56BUG_sXk00lynbUlGURd5y5cg09NBT/exec";

float current_mA, temp_c;
int gasValue;
float voltageADC;
int label;

unsigned long lastSensorTime = 0;  // ✅ For 2 sec sensor interval
unsigned long lastSendTime   = 0;  // ✅ For sending (non-blocking)
const int SENSOR_INTERVAL    = 2000; // 2 seconds

void sendToSheets() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(scriptURL)
      + "?time="    + String(millis() / 1000)
      + "&voltage=" + String(voltageADC, 3)
      + "&current=" + String(current_mA, 3)
      + "&temp="    + String(temp_c, 2)
      + "&gas="     + String(gasValue)
      + "&label="   + String(label);

    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    Serial.println(code == 200 ? "✅ Sent" : "❌ Failed: " + String(code));
    http.end();
  } else {
    Serial.println("⚠️ WiFi lost, reconnecting...");
    WiFi.begin(ssid, password);
  }
}

void readSensors() {
  current_mA = ina219.getCurrent_mA();

  sensors.requestTemperatures();
  temp_c = sensors.getTempCByIndex(0);

  gasValue = analogRead(GAS_PIN);

  long sum = 0;
  for (int i = 0; i < 20; i++) { sum += analogRead(VOLTAGE_PIN); delay(2); }
  float adcValue      = sum / 20.0;
  float sensorVoltage = adcValue * (3.3 / 4095.0);
  voltageADC          = sensorVoltage * 6.012;

  label = 0;
  if (temp_c > 40 || gasValue > 200) label = 1;
  if (temp_c > 55 || gasValue > 350) label = 2;

  digitalWrite(RELAY_PIN, (label == 2) ? LOW : HIGH);

  // Print to serial
  Serial.print(millis() / 1000); Serial.print(",");
  Serial.print(voltageADC);      Serial.print(",");
  Serial.print(current_mA);      Serial.print(",");
  Serial.print(temp_c);          Serial.print(",");
  Serial.print(gasValue);        Serial.print(",");
  Serial.println(label);
}

void setup() {
  Serial.begin(115200);
  ina219.begin();
  sensors.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  analogSetAttenuation(ADC_11db);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

void loop() {
  unsigned long now = millis();

  // ✅ Read sensors every exactly 2 seconds
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = now;
    readSensors();
    sendToSheets(); // send after every reading
  }
}