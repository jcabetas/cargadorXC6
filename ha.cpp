#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <cstdio>
#include "sensor.h"

extern char nombreWB[30];

extern AsyncMqttClient mqttClient;

extern char strChipId[5];
extern char topicAvail[40];
extern char topicCoche[40];    // lo envia cargador con el estado del coche
extern char topicPsp[40];			// para reportar cargador estado psp
extern char topicPspSet[40];   // lo envia homeassistant
extern char topicConfig[40];      // para configuracion de fases, imax,...
extern char topicMedidas[40];  // para enviar medidas en bloque

extern uint8_t numFases;  // lo actualizará STM32, hay que esperar
extern float iMin;
extern float iMax;
extern float pMin;
extern float pMax;
extern uint8_t numContactores;
extern uint16_t segArranque; //  cuenta hasta 1000...

void conectaHA(void)
{
    char topicConfig[100];
    char buffer[2500];

    JsonDocument doc;

    JsonObject dev = doc["dev"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"carg%s",strChipId);
    dev["ids"] = buffer;    //"cargKona";
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
//    snprintf(buffer,sizeof(buffer),"%s/coche",nombreWB);
    cmps_estado_coche["state_topic"] = topicCoche;
    cmps_estado_coche["device_class"] = "enum";
    cmps_estado_coche["value_template"] = "{{ value_json.coche }}";

    JsonArray cmps_estado_coche_options = cmps_estado_coche["options"].to<JsonArray>();
    //{"Desconoc", "Desconect", "Conectado","Pide","Pide vent"};
    cmps_estado_coche_options.add("Desconoc");
    cmps_estado_coche_options.add("Desconect");
    cmps_estado_coche_options.add("Conectado");
    cmps_estado_coche_options.add("Pide");
    cmps_estado_coche_options.add("Pide vent");


    JsonObject cmps_p = cmps["p"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"p_%s",strChipId);
    cmps_p["unique_id"] = buffer;
    cmps_p["name"] = "potencia";
    cmps_p["icon"] = "mdi:car";
    cmps_p["p"] = "sensor";
    cmps_p["avty_t"] = topicAvail;
    cmps_p["pl_avail"] = "online";
    cmps_p["pl_not_avail"] = "offline";
    cmps_p["expire_after"] = 30;
    //snprintf(buffer,sizeof(buffer),"%s/medidas",nombreWB);
    cmps_p["state_topic"] = topicMedidas;
    cmps_p["device_class"] = "power";
    cmps_p["unit_of_measurement"] = "W";
    cmps_p["value_template"] = "{{ value_json.p }}";

    JsonObject cmps_kwh = cmps["kwh"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"kwh_%s",strChipId);
    cmps_kwh["unique_id"] = buffer;
    cmps_kwh["name"] = "kWh";
    cmps_kwh["icon"] = "mdi:car";
    cmps_kwh["p"] = "sensor";
    cmps_kwh["avty_t"] = topicAvail;
    cmps_kwh["pl_avail"] = "online";
    cmps_kwh["pl_not_avail"] = "offline";
    cmps_kwh["expire_after"] = 30;
//    snprintf(buffer,sizeof(buffer),"%s/medidas",nombreWB);
    cmps_kwh["state_topic"] = topicMedidas;
    cmps_kwh["device_class"] = "energy";
    cmps_kwh["unit_of_measurement"] = "kWh";
    cmps_kwh["value_template"] = "{{ value_json.kwh }}";

    JsonObject cmps_kwhcarga = cmps["kwhcarga"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"kwhcarga_%s",strChipId);
    cmps_kwhcarga["unique_id"] = buffer;
    cmps_kwhcarga["name"] = "kWh carga";
    cmps_kwhcarga["icon"] = "mdi:car";
    cmps_kwhcarga["p"] = "sensor";
    cmps_kwhcarga["avty_t"] = topicAvail;
    cmps_kwhcarga["pl_avail"] = "online";
    cmps_kwhcarga["pl_not_avail"] = "offline";
    cmps_kwhcarga["expire_after"] = 30;
//    snprintf(buffer,sizeof(buffer),"%s/medidas",nombreWB);
    cmps_kwhcarga["state_topic"] = topicMedidas;
    cmps_kwhcarga["device_class"] = "energy";
    cmps_kwhcarga["unit_of_measurement"] = "kWh";
    cmps_kwhcarga["value_template"] = "{{ value_json.kwhcarga }}";

    JsonObject cmps_ia = cmps["ia"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"ia_%s",strChipId);
    cmps_ia["unique_id"] = buffer;
    cmps_ia["name"] = "Ia";
    cmps_ia["icon"] = "mdi:car";
    cmps_ia["p"] = "sensor";
    cmps_ia["avty_t"] = topicAvail;
    cmps_ia["pl_avail"] = "online";
    cmps_ia["pl_not_avail"] = "offline";
    cmps_ia["expire_after"] = 30;
//    snprintf(buffer,sizeof(buffer),"%s/medidas",nombreWB);
    cmps_ia["state_topic"] = topicMedidas;
    cmps_ia["device_class"] = "current";
    cmps_ia["unit_of_measurement"] = "A";
    cmps_ia["value_template"] = "{{ value_json.ia }}";

    if (numFases>1)
    {
        JsonObject cmps_ib = cmps["ib"].to<JsonObject>();
        snprintf(buffer,sizeof(buffer),"ib_%s",strChipId);
        cmps_ib["unique_id"] = buffer;
        cmps_ib["name"] = "Ib";
        cmps_ib["icon"] = "mdi:car";
        cmps_ib["p"] = "sensor";
        cmps_ib["avty_t"] = topicAvail;
        cmps_ib["pl_avail"] = "online";
        cmps_ib["pl_not_avail"] = "offline";
        cmps_ib["expire_after"] = 30;
//        snprintf(buffer,sizeof(buffer),"%s/medidas",nombreWB);
        cmps_ib["state_topic"] = topicMedidas;
        cmps_ib["device_class"] = "current";
        cmps_ib["unit_of_measurement"] = "A";
        cmps_ib["value_template"] = "{{ value_json.ib }}";

        JsonObject cmps_ic = cmps["ic"].to<JsonObject>();
        snprintf(buffer,sizeof(buffer),"ic_%s",strChipId);
        cmps_ic["unique_id"] = buffer;
        cmps_ic["name"] = "Ic";
        cmps_ic["icon"] = "mdi:car";
        cmps_ic["p"] = "sensor";
        cmps_ic["avty_t"] = topicAvail;
        cmps_ic["pl_avail"] = "online";
        cmps_ic["pl_not_avail"] = "offline";
        cmps_ic["expire_after"] = 30;
//        snprintf(buffer,sizeof(buffer),"%s/medidas",nombreWB);
        cmps_ic["state_topic"] = topicMedidas;
        cmps_ic["device_class"] = "current";
        cmps_ic["unit_of_measurement"] = "A";
        cmps_ic["value_template"] = "{{ value_json.ic }}";
    }

    JsonObject cmps_psp = cmps["psp"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"psp%s",strChipId);
    cmps_psp["unique_id"] = buffer;
    cmps_psp["name"] = "Psetpoint";
    cmps_psp["icon"] = "mdi:sine-wave";
    cmps_psp["avty_t"] = topicAvail;
    cmps_psp["pl_avail"] = "online";
    cmps_psp["pl_not_avail"] = "offline";
//    cmps_psp["~"] = nombreWB;
    cmps_psp["p"] = "number";
    cmps_psp["mode"] = "slider";
    snprintf(buffer,sizeof(buffer),"%d",(uint32_t) pMin);
    cmps_psp["min"] = buffer;
    snprintf(buffer,sizeof(buffer),"%d",(uint32_t) pMax);
    cmps_psp["max"] = buffer;
    cmps_psp["step"] = "100.0";
    cmps_psp["cmd_t"] = topicPspSet;
    cmps_psp["stat_t"] = topicPsp;
    cmps_psp["value_template"] = "{{ value_json.psp }}";

    doc.shrinkToFit();  // optional
    Serial.println("JSON de configuracion:");
    serializeJson(doc, Serial);
    Serial.printf("\n");

    serializeJson(doc, buffer);
    // topic homeassistant/device/kona/cargmqtt/config
    // ANTESsnprintf(topicConfig,sizeof(topicConfig),"homeassistant/device/%s/cargmqtt/config",nombreWB);
    snprintf(topicConfig,sizeof(topicConfig),"homeassistant/device/%s/config",nombreWB);
    mqttClient.publish(topicConfig, 1, false, buffer);
    Serial.printf("Enviado configuración a homeassistant\r\n");
    mqttClient.publish(topicAvail, 1, true, "online");
}








void configuraPmax(void)
{
    char topicConfig[100];
    char buffer[2000];

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
    JsonObject cmps_psp = cmps["psp"].to<JsonObject>();
    snprintf(buffer,sizeof(buffer),"psp%s",strChipId);
    cmps_psp["unique_id"] = buffer;
    cmps_psp["name"] = "Psetpoint";
    snprintf(buffer,sizeof(buffer),"%d",(uint32_t) pMax);
    cmps_psp["max"] = buffer;

    doc.shrinkToFit();  // optional
    Serial.println("JSON cambio Pmax:");
    serializeJson(doc, Serial);
    Serial.printf("\n");

    serializeJson(doc, buffer);
    snprintf(topicConfig,sizeof(topicConfig),"homeassistant/device/%s/cargmqtt/config",nombreWB);
    mqttClient.publish(topicConfig, 1, false, buffer);
    Serial.printf("Enviado configuración a homeassistant\r\n");
    mqttClient.publish(topicAvail, 1, true, "online");
}