# CargadorXC6

## Descripción
El módulo cargadorMBV2 dispone de un ESP32 modelo XiaoC6 que comunica al cargador con el mundo real, a través de un interface mqtt.

En su primer arranque abre un punto de acceso "ConfigAP" con contraseña **password** para definir la red wifi, el broker mqtt y el nombre del cargador (p.e. Kona). Una vez bien definido, se conectará a la wifi sin usar el punto de acceso

Se comunica con el cargador (cpu STM32) por puerto serie, usando normalmente JSON en los dos sentidos

## Topics que escucha
*  Configuracion, topic "**Kona**/config". La configuración del cargador se hace enviando a este topic un JSON con los siguientes valores (puede haber cambios parciales):
    * imax, imin, numfases, numcontactores, paroconcontactores, modelomedidor, idmodbus, baudmodbus
*  Topic "homeassistant/status". Se recibe rearranques de home assistant cuando reciba "online". En este caso Xiao enviará el archivo de configuración de home assistant al topic "homeassistant/device/**Kona**/cargmqtt/config". La configuración dependerá del número de fases
*  Topic "Kona/isp/set", cambio de setpoint, puede ser por:
    * Isp (enviando un json {"isp":"13.2"})
    * Psp (idem con json {"psp":"7500.1"}

## Procedimiento de arranque
*  Cuando Xiao arranque, enviará por puerto serie un json al STM32 indicando {"stm32":"arranque"}
*  El STM32 contestará con otro JSON indicando numero de fases {"xiao":"arranque","numfases":"1"}
* Si en 2 segundos STM32 no ha contestado, vuelve a enviar peticion de arranque

## Envio de status a homeassistant
*  Cada vez que cambie un estado o medida, STM32 enviara un json que, si no contiene "xiao", se enviará al topic "Kona/status"

# DISCOVERY
Homeassistant tiene un método de autoconfiguración de un dispositivo mqtt. 

El topic de cofiguracion: discovery_prefix/component/node_id/object_id/config, en nuestro caso "homeassistant/device/Kona/cargmqtt/config

Formateado:
```json
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
```
Cargador notifica estado: mosquitto_pub -h 10.244.168.149 -t "kona/state" -u joaquin -P JeW31ZT9Rdx -m '{"status":"conectado"}'

Cargador notifica Isp con mosquitto_pub -h 10.244.168.149 -t "kona/state" -u joaquin -P JeW31ZT9Rdx -m '{"isp":"13.2"}'

Para ajustar Isp: mosquitto_pub -h 10.244.168.149 -t "kona/set/isp" -u joaquin -P JeW31ZT9Rdx -m '{13'

Tambien psp de forma similar
