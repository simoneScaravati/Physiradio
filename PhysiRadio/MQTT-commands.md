# Topics and Parameters to pass to PhysiRadio

##HINT : all those MQTT calls must be made from a device, which is able to use a mosquitto MQTT publish.

General form:
mosquitto_pub -h "server_name" -t PhysiRadio/"topic" -m "parameter"

TODO: 
---> indirizzare il host name per indirizzare il singolo  -> publish sull'mqtt sorgente una stringa con l'host name per indirizzare i msg all'unico device . E fare un messaggio invece per indirizzarli a tutti


## Commands receivable : topic parameter
### Volume
- Volume +1
- Volume -1
- Volume mute
- Volume unmute

- EXAMPLES : 
- mosquitto_pub -h "server name" -t PhysiRadio/Volume -m "+5"
- mosquitto_pub -h "server name" -t PhysiRadio/Volume -m "mute"

### City (watch out: Cities have to exist on OpenWeather system, and the name must be written in English) 
- City "name_of_a_city"  (surprisingly NON-case sensitive , thanks to OpenWeather API)
- City "name_of_a_city, COUNTRY" = for those cities which have the same name but are in different countries 

- EXAMPLES : 
- mosquitto_pub -h "server name" -t PhysiRadio/City -m "London"
- mosquitto_pub -h "server name" -t PhysiRadio/City -m "New York"
- mosquitto_pub -h "server name" -t PhysiRadio/City -m "Venice, IT" (there's also in US) 

### Radio Station (for testing reasons, it's useful to force the change of radio station)  

- Station enable	Ensable API task
- Station disable	Disable API task (by default is enabled
- Station station1	( Classical -> Smoke/Mist )
- Station station2	( Metal -> Clear(sun) + High humidity )
- Station station3	( SmoothJazz -> Rain/Drizzle/Cloud/Fog  + low humidity )
- Station station4	( SummerHits -> Sunny + low humidity )
- Station station5	( ExtremeMetal -> Thunderstorm/Tornado... )
- Station station6	( LoFi -> Rain/Drizzle/Cloud/Fog  + High humidity )
- Station station7	( Xmas Songs -> Snow )

- EXAMPLES : 
- mosquitto_pub -h "server name" -t PhysiRadio/Station -m "station3"
- mosquitto_pub -h "server name" -t PhysiRadio/Station -m "enable"

# Todo / Future developements

- data2physical <json>
    prende dalla mappa lo stream corrispondente al dato e lo playa

- map <data> <urlstream>
    configura mappatura tra dato e stream
    
### Subscribe to: 

- currentVolume ...
- currentData ...
- currentUrl ...
- mem
- etc... 


