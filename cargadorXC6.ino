/*
  WiFiManager with saved textboxes Demo
  Saves data in JSON file on ESP32
  Uses SPIFFS

  DroneBot Workshop 2022
  https://dronebotworkshop.com

  Functions based upon sketch by Brian Lough
  https://github.com/witnessmenow/ESP32-WiFi-Manager-Examples
*/

#define ESP_DRD_USE_SPIFFS true

// Include Libraries

// WiFi Library
#include <WiFi.h>
// File System Library
//#include <FS.h>
#include <LittleFS.h>
// WiFiManager Library
#include <WiFiManager.h>
// Arduino JSON library
#include <ArduinoJson.h>
#include "sensor.h"


// Flag for saving data
bool shouldSaveConfig = true;

char strBrokerIP[30] = "192.168.1.48";
char userMqtt[30] = "joaquin";
char passwMqtt[30] = "JeW31ZT9Rdx";
char nombreWB[30] = "Cargador";

uint8_t numFases = 0;  // lo actualizará STM32, hay que esperar
float iMin = 0.0f;
float iMax = 16.0f;
float pMin = 0.0f;
float pMax = 3600.0f;;
uint8_t numContactores;


// Define WiFiManager Object
WiFiManager wm;
int errorWifi;

void saveConfigFile(fs::FS &fs)
// Save Config in JSON format. Guardamos tanto los datos de modbus como de mqtt, aunque los usaremos cuando convenga
{
  Serial.println(F("Saving configuration..."));

  // Create a JSON document
  JsonDocument json;
  json["strBrokerIP"] = strBrokerIP;
  json["userMqtt"] = userMqtt;
  json["passwMqtt"] = passwMqtt;
  json["nombreWB"] = nombreWB;
   
  Serial.printf("JSON a guardar:\r\n");
  serializeJsonPretty(json, Serial);

  File configFile = fs.open(JSON_CONFIG_FILE, "w");
  if(!configFile){
    Serial.println("- failed to open file for writing");
    return;
  }

  // Serialize JSON data to write to file
  // serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    // Error writing file
    Serial.println(F("Failed to write to file"));
    return;
  }
  // Close file
  configFile.close();
  Serial.println("Almacenada configuracion");
}


// Load existing configuration file
bool loadConfigFile(fs::FS &fs)
{
  JsonDocument json;
  Serial.println("Leo SPIFFS");
  vTaskDelay(pdMS_TO_TICKS(200));

  Serial.printf("Leyendo configuracion: %s\r\n", JSON_CONFIG_FILE);

  File configFile = fs.open(JSON_CONFIG_FILE, "r");
  if(!configFile || configFile.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return false;
  } 
  vTaskDelay(pdMS_TO_TICKS(100));
  DeserializationError error = deserializeJson(json, configFile);
  Serial.print("Leido en loadConfigFile:");
  configFile.close();
  serializeJsonPretty(json, Serial);
  Serial.println("------------------");
  if (!json["strBrokerIP"] || !json["userMqtt"] || !json["passwMqtt"] || !json["nombreWB"] )
  {
      Serial.println("  FALTAN DATOS!!");
      vTaskDelay(pdMS_TO_TICKS(100));
      return false;
  }
  strncpy(strBrokerIP, json["strBrokerIP"], sizeof(strBrokerIP));
  strncpy(userMqtt, json["userMqtt"], sizeof(userMqtt));
  strncpy(passwMqtt, json["passwMqtt"], sizeof(passwMqtt));
  strncpy(nombreWB, json["nombreWB"], sizeof(nombreWB));
  Serial.printf("Leido. IP broker:%s MQTT user:%s passwd:%s nombreWB:%s\r\n", strBrokerIP, userMqtt, passwMqtt, nombreWB);
  vTaskDelay(pdMS_TO_TICKS(100));
  return true;
}

void saveConfigCallback()
// Callback notifying us of the neeNEWTEST_APd to save configuration
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback(WiFiManager *myWiFiManager)
// Called when config mode launched
{
  Serial.println("Entered Configuration Mode");
  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void WiFiStationDisconnected(WiFiEvent_t e, WiFiEventInfo_t info)
{
  int datoTipoLCD;
  Serial.printf("sta_disconnected - %d\r\n", info.wifi_sta_disconnected.reason);
  errorWifi = info.wifi_sta_disconnected.reason;
  WiFi.reconnect();
}

void setupWifi(bool forzar, fs::FS &fs)
{
  Serial.printf("Inicializando wifi\r\n");
  vTaskDelay(pdMS_TO_TICKS(200));
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  // Change to true when testing to force configuration every time we run
  bool forceConfig = false;

  bool heLeidoConfig = loadConfigFile(fs);
  vTaskDelay(pdMS_TO_TICKS(200));
  if (!heLeidoConfig)
  {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }
  else if (forzar)
  {
    Serial.println(F("Asked for forcing config mode"));
    forceConfig = true;
  }

  // Explicitly set WiFi mode
  Serial.println("Iniciando modo STA");
  vTaskDelay(pdMS_TO_TICKS(200));
  WiFi.mode(WIFI_STA);

  // Set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);
  // Custom elements

  WiFiManagerParameter custom_text_box_strIP("strBrokerIP", "Broker IP", strBrokerIP, sizeof(strBrokerIP));
  WiFiManagerParameter custom_text_box_userMqtt("userMqtt", "MQTT user", userMqtt, sizeof(userMqtt));
  WiFiManagerParameter custom_text_box_passwMqtt("passwMqtt", "MQTT passwd", passwMqtt, sizeof(passwMqtt));
  WiFiManagerParameter custom_text_box_nombreWB("nombreWB", "nombreWB", nombreWB, sizeof(nombreWB));

  wm.addParameter(&custom_text_box_strIP);
  wm.addParameter(&custom_text_box_userMqtt);
  wm.addParameter(&custom_text_box_passwMqtt);
  wm.addParameter(&custom_text_box_nombreWB);

  wm.setConfigPortalTimeout(120);  // para que no se quede pillado en el portal si no hay nadie atento (lo normal)
  // Run if we need a configuration
  if (forceConfig)
  {
    if (wm.startConfigPortal("Config AP"))
    {
      strncpy(strBrokerIP, custom_text_box_strIP.getValue(), sizeof(strBrokerIP));
      strncpy(userMqtt, custom_text_box_userMqtt.getValue(), sizeof(userMqtt));
      strncpy(passwMqtt, custom_text_box_passwMqtt.getValue(), sizeof(passwMqtt));
      strncpy(nombreWB, custom_text_box_nombreWB.getValue(), sizeof(nombreWB));
      Serial.printf("strBrokerIP:%s userMqtt:%s passwMqtt:%s nombreWB:%s\r\n", strBrokerIP, userMqtt, passwMqtt, nombreWB);
      saveConfigFile(fs);
    }
    else
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // reset and try again, or maybe put it to deep sleep
      ESP.restart();  // si no hay conexion wifi, hay que volver a intentarlo forever
      delay(5000);
    }
  }
  else
  {
    if (!wm.autoConnect("Config AP"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(1000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }
  // If we get here, we are connected to the WiFi
  Serial.println("");
  Serial.printf("WiFi connected. IP: %s\r\n", WiFi.localIP().toString().c_str());
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

