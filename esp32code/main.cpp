#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Wi-Fi Credentials
#define WIFI_SSID "realme"
#define WIFI_PASSWORD "11111111"

// Firebase Credentials
#define API_KEY "AIzaSyC9tpeIbMWG0UOW0CE4CBdlJETANdqDdQ0"
#define DATABASE_URL "https://esp32-31831-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "tgunachandru@gmail.com"
#define USER_PASSWORD ""

// Sensor Pins
#define POWER_PIN 17
#define SIGNAL_PIN 36
#define ONE_WIRE_BUS 23
#define FLOW_SENSOR_PIN 2

// Sensor Libraries
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
WiFiClientSecure client;

volatile int pulseCount = 0;
unsigned long lastFlowMeasureTime = 0;
unsigned long lastSendTime = 0;

float lastFlowRate = 0.0;
float lastTemperature = 0.0;
int lastWaterValue = 0;
int intValue = 0;
String stringValue = "";

// Callback for Firebase token
void tokenStatusCallback(TokenInfo info) {
  Serial.print("Token status changed: ");

  switch (info.status) {
    case token_status_uninitialized:
      Serial.println("Uninitialized");
      break;
    case token_status_on_signing:
      Serial.println("Signing in");
      break;
    case token_status_on_request:
      Serial.println("Requesting token");
      break;
    case token_status_on_refresh:
      Serial.println("Refreshing token");
      break;
    case token_status_ready:
      Serial.println("Ready");
      break;
    case token_status_error:
      Serial.println("Error");
      break;
    default:
      Serial.println("Unknown");
      break;
  }
}


// Flow sensor interrupt
void IRAM_ATTR flowRatePulse() {
  pulseCount++;
}

void connectToWiFi() {
  Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
  }
}

void initializeFirebase() {
  client.setInsecure();

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void readSensors() {
  // Water level sensor
  digitalWrite(POWER_PIN, HIGH);
  delay(500);  // Sensor stabilization
  int waterValue = analogRead(SIGNAL_PIN);
  delay(50);  // Sensor stabilization
  digitalWrite(POWER_PIN, LOW);
  
   lastWaterValue = waterValue/10;

  // Flow sensor
  noInterrupts();
  int count = pulseCount;
  pulseCount = 0;
  interrupts();
  float flowRate = (count / 18.0f);
  if (count > 0) lastFlowRate = flowRate;

  // Temperature
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  if (temp > -50 && temp < 125) lastTemperature = temp;

  // Debug
  Serial.printf("Water: %d, Flow: %.2f L/min, Temp: %.2f°C\n",
                lastWaterValue, lastFlowRate, lastTemperature);
}

void sendToFirebase() {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready");
    return;
  }

  stringValue = "value_" + String(lastWaterValue);
  String tempStr = String(round(lastTemperature));
  String flowStr = String(lastFlowRate, 2);
  String intStr = String(intValue++);

  // All values stored as strings to match your previous structure
  Firebase.RTDB.setString(&fbdo, "/test/string", stringValue);
  Firebase.RTDB.setString(&fbdo, "/test/int", intStr);
  Firebase.RTDB.setString(&fbdo, "/test/temp", tempStr);
  Firebase.RTDB.setString(&fbdo, "/test/flowread", flowStr);

  Serial.println("Firebase updated");
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowRatePulse, RISING);

  analogSetAttenuation(ADC_11db);
  sensors.begin();

  connectToWiFi();
  initializeFirebase();
}

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (now - lastFlowMeasureTime >= 1000) {
    readSensors();
    lastFlowMeasureTime = now;
  }

  if (now - lastSendTime >= 10000) {
    sendToFirebase();
    lastSendTime = now;
  }
} 
