#include <stdio.h>
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

// Declare global OLED and BME280 objects (hardware interfaces)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;

// Status flags and timing
bool sensorReady = false;
bool oledReady = false;
unsigned long lastUpdate = 0;

const long UPDATE_INTERVAL = 2000;
const int BUTTON_PIN = 4;
bool buttonPressed = false;
const int LED_BUTTON = 5;

int ledState = LOW;
unsigned long previousMillis = 0;
const long interval = 1000;
const int TEMP_DREMPEL = 28.00;

// Create a web server object that listens on port 80 (standard HTTP port)
WebServer server(80);

// Generates the HTML webpage with sensor values (temperature & humidity)
String createWebPage(float temperature, float humidity) {

  String tempRegel;
  if (temperature >= 28.0) {
    tempRegel = "Temperature: <span style='color:red'>" + String(temperature, 2) + "</span> &deg;C";
  } else {
       tempRegel = "Temperature: " + String(temperature, 2) + " &deg;C";
     }
     
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32 Sensor</title>";
  html += "<style>body { font-family: monospace; }</style>";
  html += "</head><body>";

  // Display sensor values in a clean layout
  html += "<h2>ESP32 Sensor Data</h2>";
  html += "<div style='font-family: monospace; white-space: pre; margin:0;'>";
  html += tempRegel;
  html += "</div>";
  html += "<div style='font-family: monospace; white-space: pre; margin:0;'>";
  html += "Humidity:    " + String(humidity, 2) + " %";
  html += "</div>";
  html += "<script>setTimeout(function(){location.reload();}, 2000);</script>";
  html += "</body></html>";

  return html; // Return the full HTML string to the browser
}

void handleRoot() { 
    // Reads the temperature and humidity values
    float temp = bme.readTemperature();
    float hum = bme.readHumidity();

     // Check if sensor returned invalid data (NaN = Not a Number)
     if (isnan(temp) || isnan(hum))
     {
       // Show error page and stop
       server.send(200, "text/html", "<html><body>Sensor not available</body></html>");
       return;
     }

     String html = createWebPage(temp, hum);
     
     // Send the HTML webpage to the browser (HTTP 200 = OK)
     server.send(200, "text/html", html);
}

void showSensorData(float temp, float hum) {
     display.clearDisplay();
     display.setCursor(0, 0);
     display.setTextSize(1);
     display.setTextColor(SSD1306_WHITE);
     
     if (temp > TEMP_DREMPEL) {
       digitalWrite(LED_BUTTON, HIGH);
       display.println("Warm!");
     } else {
       digitalWrite(LED_BUTTON, LOW);
     }
     
     if(buttonPressed) {
       display.println("Button pressed!");
       buttonPressed = false;
     } else { 
       display.println("Measure Enviroment");
       if (!isnan(temp) && !isnan(hum)) {
          // Print sensor readings to Serial Monitor for debugging
          display.println("Temperature: " + String(temp, 2) + " C");
          display.println("Humidity:    " + String(hum, 2) + " %");
       } else {
          display.println("Sensor error");
        }
     }  

     display.display(); // display.update() is here 
}

void setup() {
    Serial.begin(115200);
    Wire.begin();

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_BUTTON, OUTPUT);

    // Start WiFi Access point
    WiFi.softAP("ESP32WiFi"); 
    
    // instructions for connecting MyESP32-WiFi on your phone
    Serial.println("Webserver started");
    Serial.println("ESP32 is ready for use");
    Serial.println("Go on your telephone:");
    Serial.println("1. Go to wiFi settings");
    Serial.println("2. Connect to WiFi > MyESP32");
    Serial.println("3. If connected tab on the right side settings");
    Serial.println("4. Go to manage router");
    Serial.print("5. It opens browser > http://");

    server.on("/", handleRoot); // link the URL (main page) to the handleRoot function
    server.begin(); // Start webServer

    Serial.println(WiFi.softAPIP()); // Give me the IP address of my own network
    Serial.println("See live sensor values!");

    // Initialize BME280 sensor
    if(bme.begin(0x76)) {
        sensorReady = true;
    } else {
        Serial.print("Sensor not found!");
    }
    
    // Initialize OLED screen
    if(display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) {
        oledReady = true;
    } else {
        Serial.print("Oled not found!");
    }
}

void loop() {

     // reads the button always
     if (digitalRead(BUTTON_PIN) == LOW) {
       buttonPressed = true;
       delay(50);
     }

    // Checks if it's time for an update
     if(millis() - lastUpdate >= UPDATE_INTERVAL) { 
        if (sensorReady && oledReady) {
         float temp = bme.readTemperature();
         float hum = bme.readHumidity();

        if (temp > TEMP_DREMPEL) {
           digitalWrite(LED_BUTTON, HIGH);
         } else {
           digitalWrite(LED_BUTTON, LOW);
        }
    
         
         // Checks for incorrect values
         if (!isnan(temp) && !isnan(hum)) {
            
            // Voeg tijdelijk toe in loop():
            Serial.println(digitalRead(BUTTON_PIN));
            // Print sensor readings to Serial Monitor for debugging
            Serial.println("Temperature: " + String(temp, 2) + " °C");
            Serial.println("Humidity:    " + String(hum, 2) + " %");
          } else {
            Serial.println("Sensor error");
            sensorReady = false; // sensor fails > turn off
          }

          showSensorData(temp, hum);
        } 

    // remember when we did the last update
    lastUpdate = millis();
  }

  delay(2000);

  // Web server must always listen
  server.handleClient();
}
