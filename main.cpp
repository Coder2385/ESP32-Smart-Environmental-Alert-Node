#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "never.h"

// Hardware settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Global OLED and BME280 objects (hardware interfaces)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;

// Status flags and timing
bool sensorReady = false;
bool oledReady = false;
unsigned long lastUpdate = 0;

// How often the sensor is read and the OLED is refreshed (in ms)
const long UPDATE_INTERVAL = 2000;

// Pin assignments
const int BUTTON_PIN = 4;
const int LED_BUTTON = 5;

// Temperature threshold for the warning LED and "Warm!" message (in °C)
const float TEMP_DREMPEL = 28.00;

// One-shot flag set on a button press
bool buttonPressed = false;

// Tracks the previous button reading so we can detect a press transition
bool lastButtonState = HIGH;

// Web server object listening on port 80 (standard HTTP port)
WebServer server(80);

// Generates the HTML webpage with sensor values (temperature & humidity)
String createWebPage(float temperature, float humidity) {

  // Highlight the temperature in red when it is above the threshold
  String tempRegel;
  if (temperature >= 28.0) {
    tempRegel = "Temperature: <span style='color:red'>" + String(temperature, 2) + "</span> &deg;C";
  } else {
    tempRegel = "Temperature: " + String(temperature, 2) + " &deg;C";
  }

  // Build the HTML page as a single string
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32 Sensor</title>";
  html += "<style>body { font-family: monospace; }</style>";
  html += "</head><body>";

  // Display sensor values in a clean monospace layout
  html += "<h2>ESP32 Sensor Data</h2>";
  html += "<div style='font-family: monospace; white-space: pre; margin:0;'>";
  html += tempRegel;
  html += "</div>";
  html += "<div style='font-family: monospace; white-space: pre; margin:0;'>";
  html += "Humidity:    " + String(humidity, 2) + " %";
  html += "</div>";

  // Auto-refresh the page every 2 seconds so values stay up to date
  html += "<script>setTimeout(function(){location.reload();}, 2000);</script>";
  html += "</body></html>";

  return html;
}

// Handles HTTP requests to the root URL ("/")
void handleRoot() {
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();

  // Check if the sensor returned invalid data (NaN = Not a Number)
  if (isnan(temp) || isnan(hum)) {
    server.send(200, "text/html", "<html><body>Sensor not available</body></html>");
    return;
  }

  String html = createWebPage(temp, hum);
  server.send(200, "text/html", html);
}

// Renders the current readings (and a one-shot button message) on the OLED
void showSensorData(float temp, float hum) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Control the warning LED and show "Warm!" if above the threshold
  if (!isnan(temp) && temp > TEMP_DREMPEL) {
    digitalWrite(LED_BUTTON, HIGH);
    display.println("Warm!");
  } else {
    digitalWrite(LED_BUTTON, LOW);
  }

  if (buttonPressed) {
    display.println("Button pressed!"); // display ButtonPressed 
    buttonPressed = false;              // Reset flag so it only shows once
  } else {
    display.println("Measure Environment");

    // Show sensor readings if valid, otherwise show an error message
    if (!isnan(temp) && !isnan(hum)) {
      display.println("Temperature: " + String(temp, 2) + " C");
      display.println("Humidity:    " + String(hum, 2) + " %");
    } else {
      display.println("Sensor error");
    }
  }

  // Push the buffered content to the OLED
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUTTON, OUTPUT);

  // Start the WiFi access point (no password)
  WiFi.softAP("ESP32WiFi");

  Serial.println("Webserver started");
  Serial.println("ESP32WiFi is ready for use");
  Serial.println("Go on your telephone:");
  Serial.println("1. Go to wiFi settings");
  Serial.println("2. Connect to WiFi > MyESP32");
  Serial.println("3. If connected tab on the right side settings");
  Serial.println("4. Go to manage router");
  Serial.print("5. It opens browser > http://");

  server.on("/", handleRoot);
  server.begin();

  Serial.println(WiFi.softAPIP());
  Serial.println("See live sensor values!");

  // Initialize BME280 sensor on I2C address 0x76
  if (bme.begin(0x76)) {
    sensorReady = true;
  } else {
    Serial.println("Sensor not found!");
  }

  // Initialize OLED screen on I2C address 0x3C
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) {
    oledReady = true;
  } else {
    Serial.println("Oled not found!");
  }
}

void loop() {
  // Button edge detection (every loop)
  // INPUT_PULLUP: HIGH = released, LOW = pressed
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Detect the moment of pressing (HIGH → LOW transition only)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    buttonPressed = true;
  }
  lastButtonState = currentButtonState;

  // Sensor + screen update once per UPDATE_INTERVAL
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    if (sensorReady && oledReady) {
      float temp = bme.readTemperature();
      float hum = bme.readHumidity();

      // Check for invalid sensor values
      if (!isnan(temp) && !isnan(hum)) {
        Serial.println("Temperature: " + String(temp, 2) + " °C");
        Serial.println("Humidity:    " + String(hum, 2) + " %");
      } else {
        Serial.println("Sensor error");
        sensorReady = false; // Sensor fails > mark as not ready
      }

      // Refresh the OLED with the latest values
      // (LED is controlled inside showSensorData to avoid duplicate logic)
      showSensorData(temp, hum);
    }

    lastUpdate = millis();
  }

  // The web server must always listen for incoming requests
  server.handleClient();
}
