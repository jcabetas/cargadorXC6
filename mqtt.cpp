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
extern uint16_t segArranque; //  cuenta hasta 1000...

uint64_t chipid;
char strChipId[5];
char topicAvail[40];
char topicCoche[40];    // lo envia cargador con el estado del coche
char topicPsp[40];			// para reportar cargador estado psp
char topicPspSet[40];   // lo envia homeassistant
char topicConfig[40];      // para configuracion de fases, imax,...
char topicMedidas[40];  // para enviar medidas en bloque

extern char strBrokerIP[];
extern char userMqtt[];
extern char passwMqtt[];
extern char nombreWB[];

extern uint8_t numFases; // lo actualizará STM32, hay que esperar a que sea != 0
extern float Psp;

void ponTopics(void)
{
  chipid = ESP.getEfuseMac();
  Serial.printf("Chip MAC = %X\n", chipid);
  snprintf(strChipId,sizeof(strChipId),"%04X",(uint16_t)(chipid >> 32));
  snprintf(topicAvail,sizeof(topicAvail),"%s/availability",nombreWB);
  snprintf(topicCoche,sizeof(topicCoche),"%s/coche",nombreWB);
  snprintf(topicPsp,sizeof(topicPsp),"%s/psp",nombreWB);
  snprintf(topicPspSet,sizeof(topicPspSet),"%s/psp/set",nombreWB);
  snprintf(topicConfig,sizeof(topicConfig),"%s/config",nombreWB);
  snprintf(topicMedidas,sizeof(topicMedidas),"%s/medidas",nombreWB);
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
	// me suscribo a kona/+/set para detectar ajustes (de intensidad u otros)
	uint16_t packetIdSub = mqttClient.subscribe(topicConfig,2);
	Serial.printf("suscrito a %s, qoS 2, packetId:%d\r\n",topicConfig,packetIdSub);
	// // ajustes varios, deben venir en formato JSON
	// packetIdSub = mqttClient.subscribe(topicIspSet,2);
	// Serial.printf("suscrito a %s, qoS 2, packetId:%d\r\n",topicIspSet,packetIdSub);
	packetIdSub = mqttClient.subscribe(topicPspSet,2);
	Serial.printf("suscrito a %s, qoS 2, packetId:%d\r\n",topicPspSet,packetIdSub);

	// tambien me suscribo a homeassistant/status para detectar "online" y conectar a homeassistant
	packetIdSub = mqttClient.subscribe("homeassistant/status",2);
	Serial.printf("suscrito a homeassistant/status, qoS 2, packetId:%d\r\n",packetIdSub);
}


// void PublishMqtt(char *topicStatus, char *msg, bool retain)
// {
// 	if (!mqttClient.connected())
// 		return;
// 	mqttClient.publish(topicStatus, 1, retain, msg);
// 	Serial.printf("Publicado topic:%s msg:%s\r\n",topicStatus,msg);
// }

void OnMqttReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
	float medRecibida;
	String content = GetPayloadContent(payload, len);
	Serial.printf("Recibido topic %s,content:%s\r\n",topic, content.c_str());
	if (!strcmp(topic,"homeassistant/status") && content.equals("online") && segArranque>100)
	{
		// se ha reiniciado home assistant, volvemos a conectar
		Serial.printf("Reiniciado home assistant, volvemos a conectar\r\n");
		conectaHA();
		return;
	}
	if (!strcmp(topic,topicPspSet))
	{
		Serial.printf("Recibido ajuste de Psp\r\n");
		Serial1.printf("{\"psp\":\"%s\"}\n",content.c_str());  // lo paso a JSON porque homeassistant lo envia raw
	}
	if (!strcmp(topic,topicConfig))
	{
		Serial.printf("Recibido ajustes\r\n");
		Serial1.printf("%s\n",content.c_str());
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
//	conectaHA();

	//mqttClient.setWill(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
}

void OnMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
	Serial.printf("Disconnected from MQTT, estado:%d Wifi conectada:%d\n", mqttClient.connected(), WiFi.isConnected());
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
		vTaskDelay(pdMS_TO_TICKS(20000));
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
				Serial.printf("Envio topic:%s msg:%s\n",msg.topic,msg.msg);
				mqttClient.publish(msg.topic, 1, false, msg.msg);
		}
	}
}


void mataMqtt(void)
{
	Serial.printf("Matando a mqtt\n");
	vTaskDelay(pdMS_TO_TICKS(100));
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

// espera a que STM32 nos diga numero de fases
void esperaNumFases(void)
{
	uint16_t ms = 2000;
	while (numFases==0)
	{
			if (ms >= 2000) // pide envio de numFases
			{
				Serial1.printf("{\"orden\":\"diconfig\"}\n"); //"{\"isp\":\"%s\"}"
				Serial.printf("Envio {\"orden\":\"diconfig\"}\n");
				ms = 0;
			}
			else
			{
		  	vTaskDelay(pdMS_TO_TICKS(50));
				ms += 50;
			}
	}
}

void initMqtt(void)
{
	char buffer[20];
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

	// conecta a homeAssistant
	Serial.printf("En initMqtt, esperando numFases!=0\n");
	esperaNumFases();
	Serial.printf("En initMqtt numFases:%d, conecto a homeassistant\n",numFases);
	conectaHA();
	// digo valor actual de psp
	snprintf(buffer, sizeof(buffer), "{\"psp\":\"%d\"}", (uint16_t) Psp);
	mqttClient.publish(topicPsp, 1, false, buffer);
	Serial.printf("Pido estado a STM32\n");
	Serial1.printf("{\"orden\":\"diestado\"}\n");

}