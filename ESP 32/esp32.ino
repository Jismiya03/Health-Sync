#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include "DHT.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>
// WiFi credentials
const char ssid = "redmi";
const char password = "123456789";
// Google Apps Script web app URL
const char scriptUrl = "https://script.google.com/macros/s/AKfycbzPL4h_niOall0a-
_ukIDkIM7gNPVHv434Mvz0rC94YL3KsbdHOKY10cTmztWxhkAKy4A/exec";
// Sensor instances
#define DHTTYPE DHT11
#define DHTPIN 4
#define REPORTING_PERIOD_MS 10000 // 10 seconds reporting interval
#define FINGER_DETECTION_TIMEOUT 20000 // 20 seconds in milliseconds
#define MOTOR_PIN 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
float temperature, humidity, BPM, SpO2;
uint32_t tsLastReport = 0;
bool fingerDetected = false;
bool fallDetected = false;
DHT dht(DHTPIN, DHTTYPE);
PulseOximeter pox;
MPU6050 mpu;
int16_t AcX, AcY, AcZ;
float ax = 0, ay = 0, az = 0;
#define MAX30100_INTERRUPT_PIN 19
#define FALL_THRESHOLD 100
#define ACCELERATION_HISTORY_LENGTH 100
#define ACCELERATION_CHANGE_THRESHOLD 3.0
float accelerationHistory[ACCELERATION_HISTORY_LENGTH] = {0};
int accelerationHistoryIndex = 0;
void setup() {
Serial.begin(115200);
pinMode(MOTOR_PIN, OUTPUT);
digitalWrite(MOTOR_PIN, LOW);
delay(400);
dht.begin();
// Initialize OLED display
if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
Serial.println(F("SSD1306 allocation failed"));
for (;;);
}
display.clearDisplay();
display.display();
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(1000);
Serial.println("Connecting to WiFi...");
}
Serial.println("");
Serial.println("WiFi connected!");
Serial.print("Got IP address: ");
Serial.println(WiFi.localIP());
display.setTextSize(1);
display.setTextColor(SSD1306_WHITE);
display.setCursor(0, 0);
display.println("WiFi connected!");
display.print("IP address: ");
display.println(WiFi.localIP());
display.display();
if (!pox.begin()) {
Serial.println("Failed to initialize pulse oximeter!");
while (1);
} else {
Serial.println("Pulse oximeter initialized successfully!");
pox.setOnBeatDetectedCallback(onBeatDetected);
}
pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
Wire.begin();
mpu.initialize();
pinMode(MAX30100_INTERRUPT_PIN, INPUT_PULLUP);
attachInterrupt(digitalPinToInterrupt(MAX30100_INTERRUPT_PIN), max30100InterruptHandler,
FALLING);
}
void loop() {
pox.update();
float t = dht.readTemperature();
float h = dht.readHumidity();
temperature = t;
humidity = h;
BPM = pox.getHeartRate();
SpO2 = pox.getSpO2();
fingerDetected = (BPM != 0 );
if (!fingerDetected && (millis() - tsLastReport) > FINGER_DETECTION_TIMEOUT) {
display.clearDisplay();
display.display();
}
mpu.getAcceleration(&AcX, &AcY, &AcZ);
ax = AcX / 16384.0; // Adjust scale based on your MPU6050 settings
ay = AcY / 16384.0; // Adjust scale based on your MPU6050 settings
az = AcZ / 16384.0; // Adjust scale based on your MPU6050 settings
float rawAccelerationMagnitude = sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
float accelerationMagnitude = rawAccelerationMagnitude 10; // Scale if needed
accelerationHistory[accelerationHistoryIndex] = accelerationMagnitude;
accelerationHistoryIndex = (accelerationHistoryIndex + 1) % ACCELERATION_HISTORY_LENGTH;
float accelerationChange = abs(accelerationHistory[accelerationHistoryIndex] -
accelerationHistory[(accelerationHistoryIndex - 1 + ACCELERATION_HISTORY_LENGTH) %
ACCELERATION_HISTORY_LENGTH]);
if (accelerationChange > ACCELERATION_CHANGE_THRESHOLD) {
fallDetected = true;
} else {
fallDetected = false;
}
updateDisplay();
if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
sendDataToGoogleSheet();
tsLastReport = millis();
}
}
void updateDisplay() {
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(SSD1306_WHITE);
display.setCursor(0, 0);
display.println("Temperature: " + String(temperature) + "C");
display.println("Humidity: " + String(humidity) + "%");
display.println("BPM: " + String( BPM));
display.println("SpO2: " + String( SpO2) + "%");
display.setCursor(0, 48);
if (fallDetected) {
display.println("Fall Status: Fallen");
} else {
display.println("Fall Status: Not Fallen");
}
display.display();
}
void sendDataToGoogleSheet() {
HTTPClient http;
http.begin(scriptUrl);
String payload = "{\"temperature\":\"" + String(temperature) + "\","
"\"humidity\":\"" + String(humidity) + "\","
"\"bpm\":\"" + String(BPM) + "\","
"\"spo2\":\"" + String(SpO2) + "\","
"\"fall\":\"" + String(fallDetected) + "\"}";
int httpResponseCode = http.POST(payload);
if (httpResponseCode > 0) {
Serial.print("HTTP Response code: ");
Serial.println(httpResponseCode);
} else {
Serial.print("Error code: ");
Serial.println(httpResponseCode);
}
http.end();
}
void onBeatDetected() {
Serial.println("Beat!");
}
void max30100InterruptHandler() {
// Handle interrupt from MAX30100 sensor
}