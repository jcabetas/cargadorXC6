#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <stdio.h>
#include <string.h>
#include "sensor.h"

extern char topicCoche[40];    // lo envia cargador con el estado del coche
extern char topicPsp[40];			// para reportar cargador estado psp
extern char topicPspSet[40];   // lo envia homeassistant
extern char topicConfig[40];      // para configuracion de fases, imax,...
extern char topicMedidas[40];  // para enviar medidas en bloque

extern AsyncMqttClient mqttClient;
extern QueueHandle_t xQueuSendMqtt;
extern uint16_t segArranque; //  cuenta hasta 1000...

float Isp = 0.0f;
float Psp = 0.0f;
char estado[30] = "desconocido";

extern uint8_t numFases;  // lo actualizará STM32, hay que esperar
extern float iMin;
extern float iMax;
extern float pMin;
extern float pMax;
extern uint8_t numContactores;

/*
     Cuando arranca el STM32, le envía a Xiao el estado de sus variables, incluyendo el numero de fases.
     Es un json tipo "{"orden":"estado","numfases":"3","imin":"6.0","imax":"32","numcontactores":"1"}\n
     Hay que esperar a que nos lo defina STM32

     Si recibe otros json, si está conectado mqtt los envia por mqtt
*/

// recibe datos del STM32 y los envía por mqtt
void vLeerSerie(void* pvParametros) {
  String strJSON;
  JsonDocument doc;
  float stm32Isp;
  uint8_t numFasesSTM32;
  char buffer[50];
  struct msgPublish_t msg;

  while (true) {
    strJSON = Serial1.readStringUntil('\n');
    if (!strJSON || !strJSON[0])
      continue;
    Serial.printf("Rx STM32:'%s'\n", strJSON.c_str());
    DeserializationError error = deserializeJson(doc, strJSON);
    if (error) {
      Serial.printf("Error en cadena recibida de STM32: %s\n", error.c_str());
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    // a ver si he recibido configuracion desde el STM32 
    if (doc["orden"] && !strcmp(doc["orden"], "config"))
    {
      if (doc["numfases"])
        numFasesSTM32 = atoi(doc["numfases"]);
      if (doc["imin"])
        iMin = atof(doc["imin"]);
      if (doc["imax"])
        iMax = atof(doc["imax"]);
      if (doc["pmax"])
        pMax = atof(doc["pmax"]);
      if (doc["pmin"])
        pMin = atof(doc["pmin"]);
      if (doc["psp"])
        Psp = atof(doc["psp"]);
      if (doc["numcontactores"])
        numContactores = atoi(doc["numcontactores"]);
      // if (doc["pmax"])
      // {
      //   pMax = atof(doc["pmax"]);
      //   Serial.printf("Pido a homeassistant actualizar pMax:%.1f\n", pMax);
      //   configuraPmax();
      // }
      if (doc["imax"])
      {
        Serial.printf("Pido a homeassistant actualizar pMax:%.1f\n", pMax);
        conectaHA();
      }      // miro si hay que reconfigurar en homeassistant
      if (doc["numfases"])
      {
        uint8_t numFasesSTM32 = atoi(doc["numfases"]);
        if (numFasesSTM32 != 1 && numFasesSTM32 != 3)  // solo admito 1 o 3
          continue;
        numFases = numFasesSTM32;
        // ha cambiado numero de fases, despues de tiempo funcionando??
        if (numFases != numFasesSTM32 && segArranque>60)
        {
          Serial.printf("Actualizado numFases:%d del STM32, (re)conecto a homeassistant\n", numFases);
          conectaHA();
          Serial.printf("Pido estado a STM32\n");
	        Serial1.printf("{\"orden\":\"diestado\"}\n");
          continue;
        }
      }
    }
    // no sigas si no he recibido numFases del STM32
    if (numFases == 0)
      continue;
    // o no estamos conectados a mqtt
    if (!mqttClient.connected())
      continue;

    // a ver si he recibido medidas del STM32 
    if (doc["orden"] && !strcmp(doc["orden"], "medidas"))
    {
      Serial.printf("... son medidas, las reenvio a mqtt\n");
      mqttClient.publish(topicMedidas, 1, false, strJSON.c_str());
    }

    const char* strPsp = doc["psp"];
    if (strPsp) {
      snprintf(buffer, sizeof(buffer), "{\"psp\":\"%s\"}", strPsp);
      mqttClient.publish(topicPsp, 1, false, buffer);
    }
    const char* strCoche = doc["coche"];
    if (strCoche) {
      snprintf(buffer, sizeof(buffer), "{\"coche\":\"%s\"}", strCoche);
      mqttClient.publish(topicCoche, 1, false, buffer);
    }
    // const char* strIa = doc["ia"];
    // if (strIa) {
    //   snprintf(buffer, sizeof(buffer), "{\"ia\":\"%s\"}", strIa);
    //   mqttClient.publish(topicStatus, 1, false, buffer);
    // }
    // const char* strIb = doc["ib"];
    // if (strIb) {
    //   snprintf(buffer, sizeof(buffer), "{\"ia\":\"%s\"}", strIb);
    //   mqttClient.publish(topicStatus, 1, false, buffer);
    // }
    // const char* strIc = doc["ic"];
    // if (strIc) {
    //   snprintf(buffer, sizeof(buffer), "{\"ic\":\"%s\"}", strIc);
    //   mqttClient.publish(topicStatus, 1, false, buffer);
    // }
    // const char* strP = doc["p"];
    // if (strP) {
    //   snprintf(buffer, sizeof(buffer), "{\"p\":\"%s\"}", strP);
    //   mqttClient.publish(topicStatus, 1, false, buffer);
    // }
    // const char* strkwh = doc["kwh"];
    // if (strkwh) {
    //   snprintf(buffer, sizeof(buffer), "{\"kwh\":\"%s\"}", strkwh);
    //   mqttClient.publish(topicStatus, 1, false, buffer);
    // }
    // const char* strkwhcarga = doc["kwhcarga"];
    // if (strkwhcarga) {
    //   snprintf(buffer, sizeof(buffer), "{\"kwhcarga\":\"%s\"}", strkwhcarga);
    //   mqttClient.publish(topicStatus, 1, false, buffer);
    // }
  }
}
