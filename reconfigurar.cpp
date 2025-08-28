#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// File System Library
//#include <FS.h>
// SPI Flash Syetem Library
#include "sensor.h"


TaskHandle_t xHandleBotonBoot = NULL;
static uint8_t parametroBotonBoot;
//void setupWifi(bool forzar);
//static fs::FS &fs

void vBotonBoot(void *pvParametros)
{
    while (true)
    {
        // debe pulsarse por dos segundos (se pone a bajo)
        bool pulsado = true;
        for (uint8_t t = 0; t < 20; t++)
        {
            if (digitalRead(GPIO_NUM_9) != LOW)
            {
                pulsado = false;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (pulsado)
        {
            Serial.printf("Boton Boot pulsado, reconfiguro Wifi\r\n");
            Serial.printf("Borro archivo config. %s\r\n", JSON_CONFIG_FILE);
            if(LittleFS.remove(JSON_CONFIG_FILE)){
                Serial.println("- file deleted");
            } else {
                Serial.println("- delete failed");
            }
            // espera a que deje de ser pulsado
            vTaskDelay(pdMS_TO_TICKS(50));
            while (digitalRead(GPIO_NUM_9) == LOW)
                vTaskDelay(pdMS_TO_TICKS(100));
            //mataSensor();
            mataMqtt();
            vTaskDelay(pdMS_TO_TICKS(100));
            setupWifi(true, LittleFS);
            initMqtt();
            //initSensor();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void initBotonBoot(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    pinMode(GPIO_NUM_9, INPUT);     // set pin to input
    digitalWrite(GPIO_NUM_9, HIGH); // turn on pullup resistors
    xTaskCreate(vBotonBoot, "botonBoot", 2048, &parametroBotonBoot, 1, &xHandleBotonBoot);
}