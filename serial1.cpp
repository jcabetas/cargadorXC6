#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <stdio.h>
#include <string.h>
#include "sensor.h"

extern char topicStatus[30];
extern AsyncMqttClient mqttClient;
extern QueueHandle_t xQueuSendMqtt;

float Isp = 0.0f;
char estado[30] = "desconocido";

// recibe datos del STM32 y los envía por mqtt
void vLeerSerie(void *pvParametros)
{
  String strJSON;
  JsonDocument doc;
  float stm32Isp;
  char buffer[50];
  struct msgPublish_t msg;

  while (true)
  {
    strJSON = Serial1.readStringUntil('\n');
    if (!strJSON || !strJSON[0])
      continue;
    Serial.printf("Rx STM32:'%s'\n",strJSON.c_str());
    DeserializationError error = deserializeJson(doc, strJSON);
    if (error) {
      Serial.printf("Error en cadena recibida de STM32: %s\n",error.c_str());
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    if (!mqttClient.connected())
      continue;
    const char* strIsp = doc["isp"];
    if (strIsp)
    {
      snprintf(buffer,sizeof(buffer),"{\"isp\":\"%s\"}",strIsp);
      mqttClient.publish(topicStatus, 1, false, buffer);
    }
    const char* strCoche = doc["coche"];
    if (strCoche)
    {
      snprintf(buffer,sizeof(buffer),"{\"coche\":\"%s\"}",strCoche);
      mqttClient.publish(topicStatus, 1, false, buffer);
    }
  }
}



