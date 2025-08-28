#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "sensor.h"

EventGroupHandle_t evt;//Se crea el objeto de grupo de eventos
#define EV_1SEG (1<<0)//Op1 Definir el BIT del evento 1
TaskHandle_t xHandleSensor = NULL;


void PublishMqtt(uint8_t presencia);


void onDetectInterrupt()
{
    xEventGroupSetBits(evt, EV_1SEG);//Configura el BIT (EV_1SEG) en 1
}



void esperaSensor(void*z)
{
    EventBits_t x;//Crea la variable que recibe el valor de los eventos
    uint8_t estado = digitalRead(SENSOROUT);
    while (1)
    {
        //Op1 Se espera por el evento (EV_1SEG) por un maximo de 10000ms
        x = xEventGroupWaitBits(evt, EV_1SEG, true, true, pdMS_TO_TICKS(10000));
        if (x & EV_1SEG) //Si X & EV_1SEG (True), significa que el evento ocurrio
        {
            Serial.print("Cambio\r\n");
            bool hayCambio = true;
            // compruebo estado estable por 100ms
            for (uint8_t i=0;i<10;i++)
                if (digitalRead(SENSOROUT) == estado)  // falsa alarma
                {
                    hayCambio = false;
                    break;
                }
            if (hayCambio)
            {
                estado = digitalRead(SENSOROUT);
                Serial.printf("Detectado cambio, presencia:%d\r\n", estado);
                PublishMqtt(estado);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else
        {
            estado = digitalRead(SENSOROUT);
            Serial.printf("Timeout detectando, presencia:%d\r\n", estado);
            PublishMqtt(estado);
        }
    }
}

void mataSensor(void)
{
    if (xHandleSensor!=NULL)
    {
        vTaskDelete(xHandleSensor);
        xHandleSensor = NULL;
    }
}

void initSensor(void)
{
    pinMode(SENSOROUT,INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SENSOROUT), onDetectInterrupt, CHANGE);

    evt = xEventGroupCreate();//Crea grupo de evento
    xTaskCreate(esperaSensor, "sensorEsp", 4048, NULL, 1, &xHandleSensor);//crea tarea que espera los eventos
    Serial.printf("Creado tarea vigilancia de presencia:(%d)\r\n", digitalRead(SENSOROUT));
    vTaskDelay(pdMS_TO_TICKS(100));//Espera el timeout para mostrar el error
}