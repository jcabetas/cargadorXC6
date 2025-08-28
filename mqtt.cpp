#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include "sensor.h"

TimerHandle_t mqttReconnectTimer;
extern QueueHandle_t msg_queue_LCD;

TaskHandle_t xHandleMqtt = NULL;
TaskHandle_t xHandleColaMqtt = NULL;
static uint8_t parametroTaskMqtt;

//const IPAddress MQTT_HOST(192, 168, 1, 40);
const int MQTT_PORT = 1883;
AsyncMqttClient mqttClient;
bool mqttConectado;
extern QueueHandle_t xQueuSendMqtt;

extern struct tm fechaTM;
uint16_t setPointW = 0;

uint64_t chipid;
char strChipId[5];
char topicAvail[30];
char topicIspSet[40];
char topicStatus[40];

extern char strBrokerIP[];
extern char userMqtt[];
extern char passwMqtt[];
extern char nombreWB[];

void ponTopics(void)
{
  chipid = ESP.getEfuseMac();
  Serial.printf("Chip MAC = %X\n", chipid);
  snprintf(strChipId,sizeof(strChipId),"%04X",(uint16_t)(chipid >> 32));
  snprintf(topicAvail,sizeof(topicAvail),"%s/availability",nombreWB);
  snprintf(topicStatus,sizeof(topicStatus),"%s/state",nombreWB);
  Serial.printf("Chip ID:%s topicAvail:%s\n", strChipId,topicAvail);
  vTaskDelay(pdMS_TO_TICKS(200));
}

String GetPayloadContent(char* data, size_t len)
{
	String content = "";
	for(size_t i = 0; i < len; i++)
	{
		content.concat(data[i]);
	}
	return content;
}

void SuscribeMqtt()
{
	// me suscribo a kona/isp/set para detectar ajustes de intensidad
	snprintf(topicIspSet,sizeof(topicIspSet),"%s/isp/set",nombreWB);
	uint16_t packetIdSub = mqttClient.subscribe(topicIspSet,2);
	Serial.printf("suscrito a %s, qoS 2, packetId:%d\r\n",topicIspSet,packetIdSub);
	// tambien me suscribo a homeassistant/status para detectar "online" y conectar a homeassistant
	packetIdSub = mqttClient.subscribe("homeassistant/status",2);
	Serial.printf("suscrito a homeassistant/status, qoS 2, packetId:%d\r\n",packetIdSub);
//	uint16_t packetIdSub = mqttClient.subscribe(topicbalkW,0); 
//	packetIdSub = mqttClient.subscribe(topickWhImp,0);
	// Serial.print("Subscribing at QoS 2, packetId: ");
	// Serial.println(packetIdSub);
}


void PublishMqtt(char *topicStatus, char *msg, bool retain)
{
	if (!mqttClient.connected())
		return;
	mqttClient.publish(topicStatus, 1, retain, msg);
	Serial.printf("Publicado topic:%s msg:%s\r\n",topicStatus,msg);
}
// #include <stdio.h>
// #include <string.h>
void OnMqttReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
	float medRecibida;
	String content = GetPayloadContent(payload, len);
	Serial.printf("Recibido topic %s,content:%s\r\n",topic, content);
	if (!strcmp(topic,"homeassistant/status") && content.equals("online"))
	{
		// se ha reiniciado home assistant, volvemos a conectar
		Serial.printf("Reiniciado home assistant, volvemos a conectar\r\n");
		conectaHA();
	}
	if (!strcmp(topic,topicIspSet))
	{
		// se ha reiniciado home assistant, volvemos a conectar
		Serial.printf("Recibido ajuste de Isp a %s A\r\n",content);
		Serial1.printf("{\"isp\":\"%s\"}\n",content);
	}

}


void ConnectToMqtt()
{
	Serial.println("Connecting to MQTT...");
	mqttClient.connect();
}


void OnMqttConnect(bool sessionPresent)
{
	Serial.printf("Connected to MQTT. Session present: %d\r\n", sessionPresent);
	SuscribeMqtt();
	conectaHA();
	PublishMqtt(topicAvail,"online",true);
	//mqttClient.setWill(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
}

void OnMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
	Serial.println("Disconnected from MQTT.");
}

void OnMqttSubscribe(uint16_t packetId, uint8_t qos)
{
	Serial.printf("Subscribe acknowledged, packetId:%d qos:%d\r\n", packetId, qos);
}

void OnMqttUnsubscribe(uint16_t packetId)
{
	Serial.printf("Unsubscribe acknowledged, packetId:%d\r\n",packetId);
}

void OnMqttPublish(uint16_t packetId)
{
	Serial.printf("Publish acknowledged. PacketId:%d\r\n",packetId);
}


void vWatchMqtt(void *pvParametros)
{
	// deja un tiempo para estabilizar conexiones antes de vigilar
  vTaskDelay(pdMS_TO_TICKS(20000));
  while (true)
  {
	// no esta conectado mqtt?
	if (!mqttClient.connected())
	{
		// si esta conectada la wifi, reintenta conectar a mqtt...
		if(WiFi.isConnected())
		{
			// intenta reconectarimpMedkWh
			Serial.printf("Mqtt desconectada, intentare conectar...\r\n");
			ConnectToMqtt();
		}
	}
  vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void vSendMqtt(void *pvParametros)
{
  struct msgPublish_t msg;
	char buffer[40];
  while (true)
  {
		BaseType_t xReturned = xQueueReceive(xQueuSendMqtt, &msg, pdMS_TO_TICKS(2000));
		if (xReturned != pdTRUE)
				continue;
		Serial.printf("en vSend, recibi un msg\n");
    vTaskDelay(pdMS_TO_TICKS(100));
		if (mqttClient.connected())
		{
			if (msg.topic=='i')
			{
					snprintf(buffer,sizeof(buffer),"{\"isp\":\"%s\"}",msg.msg);
					Serial.printf("Envio topic:%s msg:%s\n",topicStatus,buffer);
					mqttClient.publish(topicStatus, 1, false, buffer);
			}
			if (msg.topic=='c')
			{
					snprintf(buffer,sizeof(buffer),"{\"coche\":\"%s\"}",msg.msg);
					Serial.printf("Envio topic:%s msg:%s\n",topicStatus,buffer);
					mqttClient.publish(topicStatus, 1, false, buffer);
		}
		}
	}
}


void mataMqtt(void)
{
	if (mqttClient.connected())
	{
		mqttClient.disconnect();
		while (mqttClient.connected())
		{
	  	vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
	if (xHandleMqtt!=NULL)
	{
		vTaskDelete(xHandleMqtt);
		xHandleMqtt = NULL;
	}
}

// static const char userMqtt[] = "joaquin";
// static const char passwMqtt[] = "JeW31ZT9Rdx";


void initMqtt(void)
{
	ponTopics();

	mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(ConnectToMqtt));

  mqttClient.setWill(topicAvail, 1, true, "offline");
	
	mqttClient.onConnect(OnMqttConnect);
	mqttClient.onDisconnect(OnMqttDisconnect);

	mqttClient.onSubscribe(OnMqttSubscribe);
	mqttClient.onUnsubscribe(OnMqttUnsubscribe);

	mqttClient.onMessage(OnMqttReceived);
	mqttClient.onPublish(OnMqttPublish);

	mqttClient.setCredentials(userMqtt,passwMqtt);

	IPAddress ipBroker = IPAddress();
	ipBroker.fromString(strBrokerIP);
	mqttClient.setServer(ipBroker, MQTT_PORT);

	Serial.printf("Pido conexion a mqtt... IP:%s, user:%s, passwd:%s\r\n",strBrokerIP, userMqtt, passwMqtt);
	mqttClient.connect();
	xTaskCreate(vWatchMqtt, "vigilaMqtt", 8000, &parametroTaskMqtt, 1, &xHandleMqtt);
	xTaskCreate(vSendMqtt, "colaMqttRx",  8000, &parametroTaskMqtt, 1, &xHandleColaMqtt);
	
}