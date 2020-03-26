
#Topics and Parameters to interact with PhysiRadio

##HINT: all those MQTT calls must be executed from a device which is able to instantiate a MQTT publish command (i.e. Mosquitto client or similar)

General form:
mosquitto_pub -h "server_name" -t PhysiRadio/"topic" -m "parameter"

## Commands receivable 
### Volume
- Volume +1
- Volume -1
- Volume mute
- Volume unmute

- EXAMPLES : 
- mosquitto_pub -h "server_name" -t PhysiRadio/Volume -m "+1"
- mosquitto_pub -h "server_name" -t PhysiRadio/Volume -m "mute"

### City (watch out: Cities have to exist on OpenWeather system, and the name must be written, possibly, in English) 
- City "name_of_a_city"  (surprisingly NON-case sensitive , thanks to OpenWeather API)
- City "name_of_a_city, COUNTRY"  (for those cities which have the same name but are in different countries) 

- EXAMPLES : 
- mosquitto_pub -h "server_name" -t PhysiRadio/City -m "London"
- mosquitto_pub -h "server_name" -t PhysiRadio/City -m "New York"
- mosquitto_pub -h "server_name" -t PhysiRadio/City -m "Venice, IT" (there's also in US) 

### Radio Station (for testing/field testing reasons, it's useful to force the change of radio station)  

- Station enable	Ensable API task
- Station disable	Disable API task (by default is enabled)
- Station station1	( Classical -> Smoke/Mist )
- Station station2	( Metal -> Clear(sun) + High humidity )
- Station station3	( SmoothJazz -> Rain/Drizzle/Cloud/Fog  + low humidity )
- Station station4	( SummerHits -> Sunny + low humidity )
- Station station5	( ExtremeMetal -> Thunderstorm/Tornado... )
- Station station6	( LoFi -> Rain/Drizzle/Cloud/Fog  + High humidity )
- Station station7	( Xmas Songs -> Snow )

- EXAMPLES : 
- mosquitto_pub -h "server_name" -t PhysiRadio/Station -m "station3"
- mosquitto_pub -h "server_name" -t PhysiRadio/Station -m "enable"

#Topics with "receiver"client distinction

In the current state of development, the interaction between "publisher" clients (devices that use message brokers, like Mosquitto, or the Physiradio MQTT Interface app devepoled in this repo) and Physiradio(s) is full broadcast: Physiradio(s) are subscribed to all topics under "Physiradio/#" on the broker, and all messages published by the "publisher devices" under that topic are sent by the broker to all active Physiradios, concurrently and without filters of any kind. 
Another implemention could be to use an ACL (access control list) mechanism, for being able to select which end-device (aka which Physiradio) has to receive the specific message by simply adding one (or more) level of the topics, creating a division by classes. 
For example, we could have multiple Physiradios located in various rooms of a house. It this example we could create those topics:

- Physiradio/allrooms/... : to send the message to all Physiradios (which will all be subscribed to this topic) 
- Physiradio/room1/...	  : to send the message only to the Physiradios in room1	
- Physiradio/room2/...	  : to send the message only to the Physiradios in room2
- etc..

Specific examples: 
- mosquitto_pub -h "server_name" -t PhysiRadio/allClasses/City -m "Tokyo"
- mosquitto_pub -h "server_name" -t PhysiRadio/class1/City -m "New York"
- mosquitto_pub -h "server_name" -t PhysiRadio/class2/City -m "Venice, IT" 
- mosquitto_pub -h "server_name" -t PhysiRadio/class3/Volume -m "+1"
- mosquitto_pub -h "server_name" -t PhysiRadio/class4/Volume -m "mute"

(Best practice suggested here:  https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices/ )

# Todo / Future developements

- data2physical <json> : set up custom mapping (in JSON maybe?)

- map <data> <urlstream> : customize the output audio stream 
    
### Subscribe to: 

- currentVolume ...
- currentData ...
- currentUrl ...
- mem
- etc... 


