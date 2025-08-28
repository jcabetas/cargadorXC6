Características del sensor:
- Escucha topic de ajustes (p.e. Kona/set y cuando se reciban, en formato json se envían por puerto serie en formato Isp: 13.2
- Recibe por puerta serie estado coche y lo envía por topicEstadoCoche 

Protocolo XiaoC6-STM32
- Los ajustes que recibe xiao (p.e. isp) se envía a stm32 en formato json
- El STM32 debe enviar a xiao cada cambio de ajuste, que lo enviará a xiao para que lo traslade a mqtt

DISCOVERY
Convenio topic cofiguracion: <discovery_prefix>/<component>/[<node_id>/]<object_id>/config
En topic homeassistant/device/kona/cargmqtt/config
Formateado:
{
   "dev":{
      "ids":"cargmqtt",
      "name":"Kona",
      "mf":"jcf",
      "mdl":"cargador MB mqtt",
      "sw":"1.0",
      "sn":"B5FE",
      "hw":"cargV2"
   },
   "o":{
      "name":"Kona"
   },
   "cmps":{
      "estado_coche":{
         "unique_id":"statB5FE",
         "name":"estado",
         "icon":"mdi:car",
         "p":"sensor",
         "availability_topic":"Kona/availability",
         "payload_available":"online",
         "payload_not_available":"offline",
         "device_class":"enum",
         "value_template":"{{ value_json.estado }}",
         "options":[
            "desconocido",
            "desconectado",
            "conectado",
            "pide",
            "pide vent."
         ],
      },
      "isp":{
         "unique_id":"ispB5FE",
         "name":"Isetpoint",
         "icon":"mdi:sine-wave",
         "~":"Kona",
         "p":"number",
         "availability_topic":"~/availability",
         "payload_available":"online",
         "payload_not_available":"offline",
         "min":"0",
         "max":"32",
         "command_topic":"~/set/isp",
      },
      "state_topic":"Kona/state",
   }
}

Cargador notifica estado: mosquitto_pub -h 10.244.168.149 -t "kona/state" -u joaquin -P JeW31ZT9Rdx -m '{"status":"conectado"}'
Cargador notifica Isp con mosquitto_pub -h 10.244.168.149 -t "kona/state" -u joaquin -P JeW31ZT9Rdx -m '{"isp":"13.2"}'
Para ajustar Isp: mosquitto_pub -h 10.244.168.149 -t "kona/set/isp" -u joaquin -P JeW31ZT9Rdx -m '{13'

