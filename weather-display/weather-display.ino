#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include "Fonts/NotoSans_Bold16pt7b.h"
#include "Fonts/NotoSans_Bold8pt7b.h"
#include "credentials.h"

#define GFXFF 1

#define textColor TFT_BLACK
#define fillColor TFT_SKYBLUE

TFT_eSPI tft = TFT_eSPI();  // Initialize TFT
TFT_eSprite sprite = TFT_eSprite(&tft); // Create a sprite

// URLs for NOAA data
const char* airTempUrl = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?date=latest&station=8721604&product=air_temperature&units=english&time_zone=lst_ldt&format=json";
const char* waterTempUrl = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?date=latest&station=8721604&product=water_temperature&units=english&time_zone=lst_ldt&format=json";
const char* waterLevelUrl = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?date=latest&station=8721604&product=water_level&datum=mllw&units=english&time_zone=lst_ldt&format=json";
const char* windUrl = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?date=latest&station=8721604&product=wind&datum=mllw&units=english&time_zone=lst_ldt&format=json";

// Function to fetch data from a URL
String fetchData(const char* url) {
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();
      return payload;
    }
  }
  http.end();
  return "";
}

// Function to parse JSON and return the value for a specific key
String parseData(const char* data, const char* key) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (!error) {
    return doc["data"][0][key] | "N/A";
  } else {
    return "Error";
  }
}

void buildAirTempSprite() {
  Serial.println("Fetching air temperature data...");
  // Fetch NOAA Data
  String airTemp = parseData(fetchData(airTempUrl).c_str(), "v");

  Serial.println(airTemp);

  // Create the air temp sprite
  sprite.createSprite(120, 50);
  sprite.fillSprite(fillColor);          
  sprite.setTextColor(textColor);
  sprite.setFreeFont(&NotoSans_Bold8pt7b);
  sprite.drawString("Air Temp", 0, 0, GFXFF);
  sprite.setFreeFont(&NotoSans_Bold16pt7b);
  sprite.drawString(airTemp + " F", 0, 20, GFXFF);

  // Push the sprite to the display at (50, 50)
  sprite.pushSprite(45, 25);

  // Clean up when no longer needed
  sprite.deleteSprite();
}

void buildWaterTempSprite() {
  Serial.println("Fetching water temperature data...");
  // Fetch NOAA Data
  String waterTemp = parseData(fetchData(waterTempUrl).c_str(), "v");

  Serial.println(waterTemp);

  // Create the air temp sprite
  sprite.createSprite(120, 50);
  sprite.fillSprite(fillColor);          
  sprite.setTextColor(textColor);
  sprite.setFreeFont(&NotoSans_Bold8pt7b);
  sprite.drawString("Water Temp", 0, 0, GFXFF);
  sprite.setFreeFont(&NotoSans_Bold16pt7b);
  sprite.drawString(String(waterTemp) + " F", 0, 20, GFXFF);

  // Push the sprite to the display at (50, 50)
  sprite.pushSprite(175, 25);

  // Clean up when no longer needed
  sprite.deleteSprite();
}

void buildWaterLevelSprite() {
  Serial.println("Fetching water level data...");
  // Fetch NOAA Data
  String waterLevel = parseData(fetchData(waterLevelUrl).c_str(), "v");

  Serial.println(waterLevel);

  // Create the water level sprite
  sprite.createSprite(130, 50);
  sprite.fillSprite(fillColor);          
  sprite.setTextColor(textColor);      
  sprite.setFreeFont(&NotoSans_Bold8pt7b); 
  sprite.drawString("Water Level", 0, 0, GFXFF);
  sprite.setFreeFont(&NotoSans_Bold16pt7b); 
  sprite.drawString(waterLevel + " ft.", 0, 20, GFXFF);

  // Push the sprite to the display at (50, 50)
  sprite.pushSprite(100, 95);

  // Clean up when no longer needed
  sprite.deleteSprite();
}

void buildWindSprite() {
  Serial.println("Fetching wind data...");
  StaticJsonDocument<1024> windDoc;
  deserializeJson(windDoc, fetchData(windUrl).c_str());
  String windSpeed = windDoc["data"][0]["s"] | "N/A";
  String windDirection = windDoc["data"][0]["dr"] | "N/A";
  String windString = String(windSpeed) + "kn. " + String(windDirection);

  Serial.println(windSpeed);
  Serial.println(windDirection);

  // Create the wind data sprite
  sprite.createSprite(220, 50);
  sprite.fillSprite(fillColor);          
  sprite.setTextColor(textColor);
  sprite.setFreeFont(&NotoSans_Bold8pt7b);    
  sprite.drawString("Winds", 0, 0, GFXFF);
  sprite.setFreeFont(&NotoSans_Bold16pt7b);
  sprite.drawString(windString, 0, 20, GFXFF);  // Draw text on the sprite

  // Push the sprite to the display at (50, 50)
  sprite.pushSprite(60, 165);

  // Clean up when no longer needed
  sprite.deleteSprite();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // TFT Setup
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(fillColor);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  buildAirTempSprite();
  buildWaterTempSprite();
  buildWaterLevelSprite();
  buildWindSprite();
}

void loop() {
  delay(600000);
  buildAirTempSprite();
  delay(10000);
  buildWaterTempSprite();
  delay(10000);
  buildWaterLevelSprite();
  delay(10000);
  buildWindSprite();
}
