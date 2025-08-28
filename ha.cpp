#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <cstdio>
#include "sensor.h"

extern char nombreWB[30];
extern char iMax[10];

/*
{"name":"cargador","state_topic":"cargador/garage","unique_id":"carg895","payload_on":"ON","payload_off":"OFF","device_class":"presence"}'
*/
extern AsyncMqttClient mqttClient;

extern char strChipId[5];
extern char topicAvail[30];



void conectaHA(void)
{
    char topicConfig[100];
    char buffer[1000];

    JsonDocument doc;

    JsonObject dev = doc["dev"].to<JsonObject>();
    dev["ids"] = "cargmqtt";
    dev["name"] = nombreWB;
    dev["mf"] = "jcf";
    dev["mdl"] = "cargador MB mqtt";
    dev["sw"] = "1.0";
    dev["sn"] = strChipId;
    dev["hw"] = "cargV2";
    doc["o"]["name"] = nombreWB;

    JsonObject cmps = doc["cmps"].to<JsonObject>();
    JsonObject cmps_estado_coche = cmps["estado_coche"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"stat%s",strChipId);
    cmps_estado_coche["unique_id"] = buffer;
    cmps_estado_coche["name"] = "estado";
    cmps_estado_coche["icon"] = "mdi:car";
    cmps_estado_coche["p"] = "sensor";
    cmps_estado_coche["avty_t"] = topicAvail;
    cmps_estado_coche["pl_avail"] = "online";
    cmps_estado_coche["pl_not_avail"] = "offline";
    snprintf(buffer,sizeof(buffer),"%s/state",nombreWB);
    cmps_estado_coche["state_topic"] = buffer;
    cmps_estado_coche["device_class"] = "enum";
    cmps_estado_coche["value_template"] = "{{ value_json.coche }}";

    JsonArray cmps_estado_coche_options = cmps_estado_coche["options"].to<JsonArray>();
    cmps_estado_coche_options.add("desconocido");
    cmps_estado_coche_options.add("sin coche");
    cmps_estado_coche_options.add("conectado");
    cmps_estado_coche_options.add("pide");
    cmps_estado_coche_options.add("pide vent.");

    JsonObject cmps_isp = cmps["isp"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"isp%s",strChipId);
    cmps_isp["unique_id"] = buffer;
    cmps_isp["name"] = "Isetpoint";
    cmps_isp["icon"] = "mdi:sine-wave";
    cmps_isp["avty_t"] = topicAvail;
    cmps_isp["pl_avail"] = "online";
    cmps_isp["pl_not_avail"] = "offline";
    cmps_isp["~"] = nombreWB;
    cmps_isp["p"] = "number";
    cmps_isp["min"] = "0";
    cmps_isp["max"] = iMax;
    cmps_isp["cmd_t"] = "~/isp/set";
    cmps_isp["stat_t"] = "~/state";
    cmps_isp["value_template"] = "{{ value_json.isp }}";

    doc.shrinkToFit();  // optional
    Serial.println("JSON de configuracion:");
    serializeJson(doc, Serial);

    serializeJson(doc, buffer);
    // topic homeassistant/device/kona/cargmqtt/config
    snprintf(topicConfig,sizeof(topicConfig),"homeassistant/device/%s/cargmqtt/config",nombreWB);
    mqttClient.publish(topicConfig, 1, true, buffer);
    Serial.printf("Enviado configuración a homeassistant\r\n");
}