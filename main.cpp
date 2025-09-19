/*
 Leer estado de un radar de detección de personas HLK-LD2410B-P y publicar su estado en mqtt
 */
// Wifi, ver https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
// WiFiManager https://www.youtube.com/watch?v=VnfX9YJbaU8


#include <Arduino.h>
#include <WiFi.h>
//#include <ESP32Time.h>
#include <WiFiManager.h>
#include <cstdio>
//#include "FS.h"
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_task_wdt.h>

#include "sensor.h"

// Para watchdog, ver https://github.com/espressif/esp-idf/blob/master/examples/system/task_watchdog/main/task_watchdog_example_main.c

bool fechaAjustada = false;

#define USAR_WATCHDOG 0
#define TWDT_TIMEOUT_MS         40000
#define TASK_RESET_PERIOD_MS    30000
#define MAIN_DELAY_MS           10000
#define RX_PIN D7
#define TX_PIN D6
#define BAUD 115200

TaskHandle_t xHandleSerial = NULL;
static uint8_t parametroTaskSerial;
QueueHandle_t xQueuSendMqtt;
uint16_t segArranque = 0; //  cuenta hasta 1000...

void parpadeoLed(uint8_t numVeces)
{
  pinMode(LED_BUILTIN, OUTPUT);
  for (uint8_t i=1;i<=numVeces;i++)
  {
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED on
    vTaskDelay(pdMS_TO_TICKS(100));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED off
    vTaskDelay(pdMS_TO_TICKS(150));
  }
  // // ponemos antena exterior
  // pinMode(3, OUTPUT);    // RF switch power on
  // digitalWrite(3, LOW);
  // pinMode(14, OUTPUT);   // select external antenna
  // digitalWrite(14, HIGH);
}




void setup()
{
  // Serial0
  Serial.begin(115200);
//  while (!Serial)  {  }
  Serial.printf("Parpadeo leds\r\n");
  parpadeoLed(8);
  Serial1.begin(BAUD,SERIAL_8N1,RX_PIN,TX_PIN);
  while (!Serial1)  {  }
  if(!LittleFS.begin(true)){
    Serial.println("LITTLEFS Mount Failed");
    parpadeoLed(400);
  }
  xQueuSendMqtt = xQueueCreate(10, sizeof(msgPublish_t));
  setupWifi(false, LittleFS);
  xTaskCreate(vLeerSerie, "leerSerie", 20000, &parametroTaskSerial, 1, &xHandleSerial); 
  initMqtt();
  //initSensor();
  initBotonBoot();
#if USAR_WATCHDOG
    // If the TWDT was not initialized automatically on startup, manually intialize it now
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = TWDT_TIMEOUT_MS,
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,    // Bitmask of all cores
        .trigger_panic = false,
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
    Serial.printf("TWDT initialized\n");
        // Subscribe this task to TWDT, then check if it is subscribed
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_ERROR_CHECK(esp_task_wdt_status(NULL));
#endif // USAR_WATCHDOG
}

void loop()
{
  #if USAR_WATCHDOG
  esp_task_wdt_reset();
  #endif
  vTaskDelay(pdMS_TO_TICKS(5000));
  if (segArranque<1000)
      segArranque += 5;
}
