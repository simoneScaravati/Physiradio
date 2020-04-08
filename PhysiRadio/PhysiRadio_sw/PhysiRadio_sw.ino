/*
  MQTT remote control e status --> mqtt topics defined externally

  Wirings : see wirings.txt

  Dependencies:
  -VS1053 library by baldram (https://github.com/baldram/ESP_VS1053_Library)
  -ESP8266Wifi
  -https://github.com/spacehuhn/SimpleMap  -> dictionaryMap

  IDE Settings (Tools):
  - IwIP Variant: v2.0 Higher Bandwidth
  - CPU Frequency: 80Hz  --> at 160Hz Wemos D1 gets hot
  - BOARD: LOLIN(WEMOS) D1 mini PRO

  TODO : codice e documentazione in inglese

  BUG non funziona se attaccato da solo (alimentatore o powerpack) --> [simone] a me funziona correttamente

  RFE si riesce a stampare la canzone attuale? (ci sono metadati?) perché il la canzone all'interno del genere potrebbe introdurre variabilità
  --> [simone] In che senso che si può introdurre variabilità?

  RFE aggiungere OTA (quando sistemato bug "standalone")

  RFE risolvere questione DNS acquisento solo la prima volta l'IP e la porta del sito  --> in realtá lo fa di default, é solo molto lento a convertire certi indirizzi alla prima connessione

  NB: per fare i test/demo/questionari registrando la sessione:
  $ picocom -b 115200 /dev/ttyUSB0 -g pico.log

*/

#include "VS1053.h"
#include "SimpleMap.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include "wifi.h" //external wifi.h file where must be defined WiFi ssid and password
#include <FastLED.h>

#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D2
//#define VOLUME 75.0	  	//default Volume for the output jack
#define BUFF 32		     	  //buffer size for stream to VS1053 (32bytes is the maximum)
#define COUNT_FADE 0.3	  //constant for audio fade in/out
//led stripe defs
#define NUM_LEDS 5

//ifdef APA to use APA led stripes instead of WS2812. I suggest to put the definition in wifi.h file
#ifdef APA
// APA102
#define DATA_PIN D4
#define CLOCK_PIN D8
#else
// WS2812
#define DATA_PIN D3
#define CLOCK_PIN D4
#endif

//array of leds colors
CRGB leds[NUM_LEDS];

typedef struct radioStation {
    String name;
    char *host;
    char *path;
    int port;
    CRGB color;
} radioStation;

//radio stations
//HINT: at the moment, using some psecific symbolic names (in URL) as "host", it's hard for ESP8266, because DNS conversion has too big latency.
radioStation radioClassical = {"Chroma Classical - solo classical music", "148.251.184.14", "/;", 8008, CRGB::Magenta};
radioStation radioMetal = {"CHROMA METAL - solo metal", "148.251.184.14", "/;", 8022, CRGB::Red};
radioStation radioXmas = {"Chroma Christmas - solo Xmas songs", "148.251.184.14", "/;", 8044, CRGB::White};
radioStation radioSummer = {"Energy rsi Summer", "energysummer.ice.infomaniak.ch", "/energysummer-high.mp3", 80, CRGB::Yellow};
radioStation radioSmoothJazz = {"Chroma Smooth Jazz:", "148.251.184.14", "/;", 8036, CRGB::BlueViolet};
radioStation radioExtremeMetal = {"Radio Caprice - Brutal Death Metal", "79.111.119.111", "/;", 9075, CRGB::DarkGreen};
radioStation radioLoFi = {"Lofi hip hop radio(lfhh.org)", "hyades.shoutca.st", "/stream2", 8043, CRGB::DarkBlue};

//test -> it's a controlled bug to make things work  ---> debug this in future
radioStation radioTest = {"test", "test", "/stream2", 8043, CRGB::Black};

//mapping OpenWeather condition name -> WebRadio (as radioStation defined before)
SimpleMap<String, radioStation> *weatherMap;

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

//three different WiFi client to handle Api request, music streaming and Mqtt connetion separately
WiFiClient api;
WiFiClient music;
WiFiClient mqttClient;
PubSubClient psclient(mqttClient);

char *httpHost;
char *httpPath;
int httpPort;
uint8_t mp3buff[BUFF];
int incomingByte = 0; 				   // for incoming Serial data
float TMP_VOLUME = 0.0;     	   //temporary volume for output jack in fade_in_audio fade_out_audio
float VOLUME = 85.0;
String clientId = "Physiradio";  //clientId for MQTT client
String cityRequested = "Milan";  //city requested for API via MQTT -> default query

String weather_0_main ; //eg "Clouds"
String city_name ; //eg "Milan"
int humidity; //0-100%

//SCHEDULING TASKS
Scheduler scheduler;
//functions prototype
void task_apiCallback();
void task_audioStreamCallback();
void task_mqttCallback();
void mqttCallback(char* topic, byte* payload, unsigned int length); //callback function for MQTT
void connect_radio(radioStation *radio);
void fade_in_audio();
void fade_out_audio();
void setLedsColor(CRGB color);
void doMapping(String weather);
//void task_ledsCallback();         //led color stripe --> in the case you want animate leds

//Tasks
Task t_api(10 * TASK_SECOND, TASK_FOREVER, &task_apiCallback);  //call weather api function every 15s
Task t_stream(0, TASK_FOREVER, &task_audioStreamCallback);      //call stream task always
Task t_mqtt(100, TASK_FOREVER, &task_mqttCallback);             //call mqtt callback to get mqtt messages
//Task t_leds(500, TASK_FOREVER , &task_ledsCallback);          //led color stripe --> in the case you want animate leds

void setup () {
    //SERIAL
    Serial.begin(115200);
    Serial.println("\n\nWelcome to PhysiRadio!!\n");
    delay(1000);

    //SPI
    SPI.begin();  //requires few seconds to start
    delay(1000);

    //Initialize leds
    #ifdef APA
        FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
    #else
        FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
    #endif
        setLedsColor(CRGB::Black);

    //Weather Map
    weatherMap = new SimpleMap<String, radioStation>([](String &a, String &b) -> int {
        if (a == b) return 0;      // a and b are equal
        else if (a > b) return 1;  // a is bigger than b
        else return -1;            // a is smaller than b
    });

    weatherMap->put("Clear", radioSummer );
    weatherMap->put("Snow", radioXmas);
    weatherMap->put("Thunderstorm", radioExtremeMetal);
    weatherMap->put("Tornado", radioExtremeMetal);
    weatherMap->put("Squall", radioExtremeMetal);
    weatherMap->put("Ash", radioExtremeMetal);
    weatherMap->put("Dust", radioExtremeMetal);
    weatherMap->put("Rain", radioSmoothJazz);
    weatherMap->put("Drizzle", radioSmoothJazz);
    weatherMap->put("Fog", radioSmoothJazz);  // ---> 100% humidity
    weatherMap->put("Clouds", radioSmoothJazz);
    weatherMap->put("Mist", radioSmoothJazz);
    weatherMap->put("Smoke", radioClassical);
    weatherMap->put("Haze", radioClassical);
    weatherMap->put("Sand", radioClassical);

    //WIFI STARTING
    Serial.print("Connecting to SSID ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    //MQTT SETUP
    psclient.setServer("mqtt.atrent.it", 1883);
    psclient.setCallback(mqttCallback);
    reconnect();

    //set NO_DELAY --> disable Nagle Algorithm
    api.setNoDelay(true);     //we want the whole message
    api.setTimeout(500);      //timeout api connection 0.5 s
    music.setTimeout(500);    //timeout music connection 0.5 s

    //SCHEDULING TASKS
    scheduler.init();
    Serial.println("Initialized scheduler");

    scheduler.addTask(t_api);
    Serial.println("added api task");
    scheduler.addTask(t_stream);
    Serial.println("added stream task");
    scheduler.addTask(t_mqtt);
    Serial.println("added mqtt task");
    //scheduler.addTask(t_leds);
    //Serial.println("added led task");

    t_api.enable();
    Serial.println("Enabled api task");
    t_stream.enable();
    Serial.println("Enabled stream task");
    t_mqtt.enable();
    Serial.println("Enabled mqtt task");
    //t_leds.enable();  --> for solid color it's not necessary
    //Serial.println("Enabled led task");

    //PLAYER START UP
    player.begin();
    player.switchToMp3Mode();
    player.setVolume(VOLUME);

    Serial.println("Connecting to Web radio...");

    connect_radio(&radioTest);
}

void loop() {

    scheduler.execute();

}

void setLedsColor(CRGB color) {
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
}


//for http get : Openweather API -> returns JSON
//main_api = 'http://api.openweathermap.org/data/2.5/weather?q=Milan&APPID=caaaf9c4e90fc42fe0baa94b8f1f6928&units=metric'
void task_apiCallback() {
    if (!api.connect("api.openweathermap.org", 80)) {
        Serial.println("Connection failed");
        api.stop();
        return;
    }
    Serial.println("Connected to API domain!");
    // Send HTTP request
    String httpMsg = String("GET /data/2.5/weather?q=" + cityRequested + "&APPID=caaaf9c4e90fc42fe0baa94b8f1f6928&units=metric HTTP/1.0" );
    //api.println("GET /data/2.5/weather?q=Milan&APPID=caaaf9c4e90fc42fe0baa94b8f1f6928&units=metric HTTP/1.0");
    Serial.println(httpMsg);
    api.println(httpMsg);
    api.println("Host: api.openweathermap.org");
    api.println("Connection: close");
    if (api.println() == 0) {
        Serial.println("Failed to send request");
        api.stop();
        return;
    }

    // Check HTTP status
    char status[32] = {0};
    api.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
        Serial.print("Unexpected response: ");
        Serial.println(status);
        api.stop();
        return;
    }

    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!api.find(endOfHeaders)) {
        Serial.println("Invalid response");
        api.stop();
        return;
    }

    // Allocate the JSON document
    // Use arduinojson.org/v6/assistant to compute the capacity.
    const size_t capacity = JSON_ARRAY_SIZE(4) + 2*JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + 4*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(14) + 1816; //should be 816, but u know...more is better
    DynamicJsonDocument doc(capacity);

    // Parse JSON object
    DeserializationError error = deserializeJson(doc, api);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        api.stop();
        return;
    }

    // Extract values
    JsonObject weather_0 = doc["weather"][0];
    JsonObject main_0 = doc["main"];

    const char* weather_0_main_char = weather_0["main"]; //eg "Clouds"
    const char* city_name_char = doc["name"]; //eg "Milan"

    city_name = String(city_name_char);
    weather_0_main = String(weather_0_main_char);
    humidity = main_0["humidity"];

    Serial.println("Response:");
    Serial.println(city_name);
    Serial.println(weather_0_main);
    Serial.print(humidity);
    Serial.println(" % humidity");

    //Extra mapping goes here
    doMapping(weather_0_main);
}

void task_audioStreamCallback() {
    if(!music.connected()) {
        Serial.println("Reconnecting for audio stream...");
        if(music.connect(httpHost, httpPort)) {
            music.print(String("GET ") + httpPath + " HTTP/1.1\r\n" +
                        "Host: " + httpHost + "\r\n" +
                        "Connection: close\r\n\r\n");
            Serial.println("connected to stream source");
        }
    }

    //play stream
    if(music.available() > 0) {
        uint8_t bytesread = music.readBytes(mp3buff, BUFF);
        player.playChunk(mp3buff, bytesread);
    }

    //code to control Physiradio easily by Serial input
    if (Serial.available() > 0) {
        // read the incoming byte for serial input control;
        incomingByte = Serial.read();
        switch(incomingByte) {
        case 's':
            player.setVolume(0);
            Serial.println("VOLUME : 0");
            break;

        case 'a':
            player.setVolume(VOLUME);
            Serial.print("VOLUME : ");
            Serial.println(VOLUME);
            break;

        case '+':
            Serial.println("Volume +1");
            TMP_VOLUME+=1.0;
            if (TMP_VOLUME <= 100.0) {
                player.setVolume(TMP_VOLUME);
            } else {
                TMP_VOLUME = 100.0;
            }
            VOLUME = TMP_VOLUME;
            Serial.print("VOLUME : ");
            Serial.println(TMP_VOLUME);
            break;


        case '-':
            Serial.println("Volume -1");
            TMP_VOLUME-= 1.0;
            if(TMP_VOLUME >= 0.0) {
                player.setVolume(TMP_VOLUME);
            } else {
                TMP_VOLUME = 0.0;
            }
            VOLUME = TMP_VOLUME;
            Serial.print("VOLUME : ");
            Serial.println(TMP_VOLUME);
            break;

        //case that STOPS THE API task
        case 'r':
            t_api.disable();
            Serial.println("API task Disabled");
            break;

        //CASE THAT ENABLE THE API task
        case 'e':
            t_api.enable();
            Serial.println("API task Enabled");
            break;

        case '1':
            Serial.println("Radio 1 -> Smoke/Mist -> ");
            connect_radio(&radioClassical);
            break;

        case '2':
            Serial.println("Radio 2 -> Clear(sun) + High humidity -> ");
            connect_radio(&radioMetal);
            break;

        case '3':
            Serial.println("Radio 3 -> Rain/Drizzle/Cloud/Fog  + low humidity -> ");
            connect_radio(&radioSmoothJazz);
            break;

        case '4':
            Serial.println("Radio 4 -> Clear(sun) + low humidity -> ");
            connect_radio(&radioSummer);
            break;

        case '5':
            Serial.println("Radio 5 -> Thunderstorm/Tornado... -> ");
            connect_radio(&radioExtremeMetal);
            break;

        case '6':
            Serial.println("Radio 6 -> Rain/Drizzle/Cloud/Fog  + High humidity -> ");
            connect_radio(&radioLoFi);
            break;

        case '7':
            Serial.println("Radio 7 -> Snow -> ");
            connect_radio(&radioXmas);
            break;

        case 'R': // RESET
            Serial.println("RESET!");
            ESP.reset();
            break;

        }
    }
}

void task_mqttCallback() {
    // Connect if we're not already connected.
    if (!psclient.connected())
        reconnect();

    // process any events.
    psclient.loop();
}

//callback assigned to all mqtt clients
void mqttCallback(char* topic, byte* payload, unsigned int length) {

    //parsing the payload
    char msg[length];
    for (int i = 0; i < length; i++) {
        msg[i] = (char)payload[i];
    }
    //adding EOF to cast msg to a string
    msg[length] = '\0';

    if(strcmp(topic, "PhysiRadio/Volume") == 0) {
        if(strcmp(msg, "mute") == 0) {
            Serial.println("Muting volume");
            player.setVolume(0);
            Serial.println("VOLUME : 0");
        }

        if(strcmp(msg, "unmute") == 0) {
            Serial.println("UnMuting volume");
            player.setVolume(TMP_VOLUME);
            Serial.print("VOLUME : ");
            Serial.println(TMP_VOLUME);
        }

        if(strcmp(msg, "+1") == 0) {
            Serial.println("Volume +1");
            TMP_VOLUME+=1.0;
            if (TMP_VOLUME <= 100.0) {
                player.setVolume(TMP_VOLUME);
            } else {
                TMP_VOLUME = 100.0;
            }
            VOLUME = TMP_VOLUME;
            Serial.print("VOLUME : ");
            Serial.println(TMP_VOLUME);
        }

        if(strcmp(msg, "-1") == 0) {
            Serial.println("Volume -1");
            TMP_VOLUME-= 1.0;
            if(TMP_VOLUME >= 0.0) {
                player.setVolume(TMP_VOLUME);
            } else {
                TMP_VOLUME = 0.0;
            }
            VOLUME = TMP_VOLUME;
            Serial.print("VOLUME : ");
            Serial.println(TMP_VOLUME);
        }
    }

    if(strcmp(topic, "PhysiRadio/Station") == 0) {

        if(strcmp(msg, "enable") == 0) {
            t_api.enable();
            Serial.println("API task Enabled");
        }

        if(strcmp(msg, "disable") == 0) {
            t_api.disable();
            Serial.println("API task Disabled");
        }

        if(strcmp(msg, "station1") == 0) {
            Serial.println(msg);
            Serial.println("Radio 1 - Classical -> Smoke/Haze");
            connect_radio(&radioClassical);
        }

        if(strcmp(msg, "station2") == 0) {
            Serial.println(msg);
            Serial.println("Radio 2 - Metal -> Clear(sun) + High humidity");
            connect_radio(&radioMetal);
        }

        if(strcmp(msg, "station3") == 0) {
            Serial.println(msg);
            Serial.println("Radio 3 - SmoothJazz -> Rain/Drizzle/Cloud/Fog  + low humidity");
            connect_radio(&radioSmoothJazz);
        }

        if(strcmp(msg, "station4") == 0) {
            Serial.println(msg);
            Serial.println("Radio 4 - SummerHits -> Sunny + low humidity");
            connect_radio(&radioSummer);
        }

        if(strcmp(msg, "station5") == 0) {
            Serial.println(msg);
            Serial.println("Radio 5 - ExtremeMetal -> Thunderstorm/Tornado...");
            connect_radio(&radioExtremeMetal);
        }

        if(strcmp(msg, "station6") == 0) {
            Serial.println(msg);
            Serial.println("Radio 6 - LoFi -> Rain/Drizzle/Cloud/Fog  + High humidity");
            connect_radio(&radioLoFi);
        }

        if(strcmp(msg, "station7") == 0) {
            Serial.println(msg);
            Serial.println("Radio 7 - Xmas Songs -> Snow");
            connect_radio(&radioXmas);
        }
    }

    //mqtt dynamic change city for API
    if(strcmp(topic, "PhysiRadio/City") == 0) {
        cityRequested = String(msg);
    }
}

void reconnect() {

    //it was a while before
    if (!psclient.connected()) {
        Serial.print("Attempting MQTT connection...");

        // Attempt to connect
        if (psclient.connect(clientId.c_str())) {
            Serial.println(clientId + " connected to MQTT server");

            // TODO inviare messaggio di "saluto" (con IP e altro status) per avere un mini debug anche via MQTT

            // subscribe to all topics
            // psclient.subscribe("#");
            // all subtopics:
            psclient.subscribe("PhysiRadio/#");

        } else {
            Serial.print("failed, rc=");
            Serial.print(psclient.state());
        }
    }
}

void doMapping(String weather) {
    if(weatherMap->has(weather)) {

        Serial.println("Doing mapping for " + weather + " weather");

        radioStation tempRadio = weatherMap->get(weather);

        //doing cases for high humidity levels  --> https://it.wikipedia.org/wiki/Umidit%C3%A0
        if(humidity >= 85) { //
            if ( (tempRadio.name).equals(radioSummer.name) ) {
                tempRadio = radioMetal;
            }

            if( (tempRadio.name).equals(radioSmoothJazz.name) ) {
                tempRadio = radioLoFi;
            }

        }

        Serial.print("Got: " + tempRadio.name + " --> " );
        //Serial.print(tempRadio.host);Serial.println(httpHost);

        //if new radio is different from the actual then connect
        //look host OR port, because some radios use same host for multiple web radio switching on ports
        if((strcmp((tempRadio.host), httpHost) != 0) | (tempRadio.port != httpPort) ) {
            connect_radio(&tempRadio);
        } else {
            Serial.println("Already on the same station");
        }

    } else {
        Serial.println("Weather not mapped");
    }
}


void fade_in_audio() {
    TMP_VOLUME = 0.0;
    Serial.println("Fading Volume....");
    while (TMP_VOLUME < VOLUME) {
        if(music.available() > 0) {
            uint8_t bytesread = music.readBytes(mp3buff, BUFF);
            player.playChunk(mp3buff, bytesread);
            player.setVolume(TMP_VOLUME);
            TMP_VOLUME += COUNT_FADE;
            //Serial.println(TMP_VOLUME);
        }
    }
}

void fade_out_audio() {
    while(TMP_VOLUME >= 0.3) {
        if(music.available() > 0) {
            uint8_t bytesread = music.readBytes(mp3buff, BUFF);
            player.playChunk(mp3buff, bytesread);
            player.setVolume(TMP_VOLUME);
            TMP_VOLUME -= COUNT_FADE;
            //Serial.println(TMP_VOLUME);
        }
    }
}


void connect_radio(radioStation *radio) {
    fade_out_audio();

    Serial.print("+++ rssi:");
    Serial.println(WiFi.RSSI());

    httpHost = radio->host;
    httpPath = radio->path;
    httpPort = radio->port;

    if (!music.connect(httpHost, httpPort)) {
        Serial.println("Radio Connection failed");
        return;
    }

    Serial.print("Requesting stream: ");
    Serial.println(httpPath);
    music.print(String("GET ") + httpPath + " HTTP/1.1\r\n" +
                "Host: " + httpHost + "\r\n" +
                "Connection: close\r\n\r\n");

    setLedsColor(radio->color);
    fade_in_audio();
    Serial.println(radio->name);

}
