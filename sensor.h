#ifndef SENSOR_H
#define SENSOR_H

#include <LittleFS.h>
#define SENSOROUT D3
#define TRIGGER_PIN 9
#define JSON_CONFIG_FILE "/config.txt"
#define FORMAT_LITTLEFS_IF_FAILED true

#define STACK_SIZE 4048
#define STACK_SIZE_LOW 1048

void initSensor(void);
void initBotonBoot(void);
void setupWifi(bool forzar, fs::FS &fs);
void initMqtt(void);
void mataMqtt(void);
void mataSensor(void);
void initSensor();
void conectaHA(void);
void vLeerSerie(void *pvParametros);
void configuraPmax(void);
//void PublishMqtt(char *topicStatus, char *msg, bool retain);

struct msgPublish_t
{
    char topic[15]; 
    char msg[ 15 ];
};


#endif