/*
  TODO MQTT remote control e status --> mqtt topics defined externally

  Wirings : see wirings.txt

  Dependencies:
  -VS1053 library by baldram (https://github.com/baldram/ESP_VS1053_Library)
  -ESP8266Wifi
  -https://github.com/spacehuhn/SimpleMap  -> dictionaryMap


  IDE Settings (Tools):
  -IwIP Variant: v2.0 Higher Bandwidth
  -CPU Frequency: 160Hz
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
//#define VOLUME 75.0		//default Volume for the output jack
#define BUFF 32		  	//buffer size for stream to VS1053 (32bytes is the maximum)
#define COUNT_FADE 0.4	//constant for audio fade in/out
//led stripe defs
#define NUM_LEDS 5 //8
#define DATA_PIN D3
#define CLOCK_PIN D4
#define BRIGHTNESS 50

CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
int nled=0;

typedef struct radioStation{
  String name;
  char *host;
  char *path;
  int port;
}radioStation;

//hardcoding radio stations (temporary)
//AAH RADIO - solo classical/baroque:	http://78.129.251.150:10002/stream
//radioStation radio1 = {"AAH Radio - solo baroque" , "78.129.251.150" , "/stream" , 10002};  --> WebRadio DOWN :(
radioStation radio1 = {"Chroma Classical - solo classical music" , "148.251.184.14" , "/;" , 8008}; //http://148.251.184.14:8008/;
radioStation radio2 = {"CHROMA METAL - solo metal" , "148.251.184.14" , "/;" , 8022};
radioStation radio3 = {"Chroma Christmas - solo Xmas songs" , "148.251.184.14" , "/;" , 8044};
radioStation radio4 = {"Radio Summer Love | We Love Summer" , "s11.myradiostream.com" , "/stream/1/" , 13494};
radioStation radio5 = {"1.FM - Chillout Lounge" , "strm112.1.fm" , "/chilloutlounge_mobile_mp3" , 80};
radioStation radio6 = {"Chroma Smooth Jazz:	" , "148.251.184.14" , "/;" , 8036};

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
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
float VOLUME = 75.0;
String clientId = "Physiradio";  //clientId for MQTT client

//SCHEDULING TASKS
Scheduler scheduler;
//functions prototype
void task_wifiRSSI();
void task_apiCallback();
void task_audioStreamCallback();
void task_mqttCallback();
void task_ledsCallback();
void sinelon();
void callback(char* topic, byte* payload, unsigned int length); //callback function for MQTT
void connect_radio(radioStation *radio);
void fade_in_audio();
void fade_out_audio();

//Tasks
Task t_wifi(2 * TASK_SECOND, TASK_FOREVER, &task_wifiRSSI);     //wifiRSSI
Task t_api(15 * TASK_SECOND, TASK_FOREVER, &task_apiCallback);  //call api function every 15s
Task t_stream(0, TASK_FOREVER, &task_audioStreamCallback);      //call stream task always
Task t_mqtt(3 * TASK_SECOND, TASK_FOREVER, &task_mqttCallback); //
Task t_leds(100/*1* */, TASK_FOREVER , &sinelon); //led color stripe

//mapping OpenWeather -> WebRadio
SimpleMap<String, radioStation> *weatherMap = new SimpleMap<String, radioStation>([](String &a, String &b) -> int {
	if (a == b) return 0;      // a and b are equal
	else if (a > b) return 1;  // a is bigger than b
	else return -1;            // a is smaller than b
});


void setup () {

    delay(1000);
    //Initialize leds
    FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
    leds[0] = CRGB::Black;
    leds[1] = CRGB::Black;
    leds[2] = CRGB::Black;
    leds[3] = CRGB::Black;
    leds[4] = CRGB::Black;
    leds[5] = CRGB::Black;
    leds[6] = CRGB::Black;
    leds[7] = CRGB::Black;
    FastLED.show();
    //SPI
    SPI.begin();  //requires few seconds to start
    //SERIAL
    Serial.begin(115200);
    Serial.println("\n\nWelcome to PhysiRadio!!\n");

    //WIFI STARTING
    Serial.print("Connecting to SSID "); Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi connected");
    Serial.println("IP address: ");  Serial.println(WiFi.localIP());

    //MQTT SETUP
    psclient.setServer("mqtt.atrent.it", 1883);
    psclient.setCallback(callback);
    reconnect();

    //set NO_DELAY --> disable Nagle Algorithm
    api.setNoDelay(true);
    api.setTimeout(500);      //timeout api connection 0.5 s
    //music.setNoDelay(true);
    music.setTimeout(500);    //timeout music connection 0.5 s

    //SCHEDULING TASKS
    scheduler.init();
    Serial.println("Initialized scheduler");

    scheduler.addTask(t_api);
    Serial.println("added api task");
    scheduler.addTask(t_stream);
    Serial.println("added stream task");
    //scheduler.addTask(t_wifi);
    //Serial.println("added wifi task");
    scheduler.addTask(t_mqtt);
    Serial.println("added mqtt task");
    scheduler.addTask(t_leds);
    Serial.println("added led task");

    t_api.enable();
    Serial.println("Enabled api task");
    t_stream.enable();
    Serial.println("Enabled stream task");
//    t_wifi.enable();
//    Serial.println("Enabled wifi task");
    t_mqtt.enable();
    Serial.println("Enabled mqtt task");
    t_leds.enable();
    Serial.println("Enabled led task");

    //PLAYER START UP
    player.begin();
    player.switchToMp3Mode();
    player.setVolume(VOLUME);

    //delay(1000);
    Serial.println("Connecting to Web radio...");

    // default radio: AAH RADIO - Baroque  --> see mappingOpenWeather_WebRadio.txt for details
    connect_radio(&radio1);
}

void loop() {

    scheduler.execute();

}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = beatsin16( 30, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue , 255, 192);
  if(nled == 0) gHue += 1;
  FastLED.show();
}

//led stripe function on start
void task_ledsCallback(){
  leds[nled] = CRGB::Red;
  nled = (nled + 1)%NUM_LEDS;
  FastLED.show();
}

//for http get : Openweather API -> returns JSON
//main_api = 'http://api.openweathermap.org/data/2.5/weather?q=Milan&APPID=caaaf9c4e90fc42fe0baa94b8f1f6928&units=metric'

void task_wifiRSSI(){
  Serial.print(WiFi.RSSI()); Serial.println(" dBm");
}

void task_apiCallback(){
  if (!api.connect("api.openweathermap.org", 80)) {
    Serial.println("Connection failed");
    api.stop();
    return;
  }

  Serial.println("Connected to API domain!");
  // Send HTTP request
  api.println("GET /data/2.5/weather?q=Milan,it&APPID=caaaf9c4e90fc42fe0baa94b8f1f6928&units=metric HTTP/1.0");
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
  const char* weather_0_main = weather_0["main"]; // "Clouds"
  const char* weather_0_description = weather_0["description"]; // "broken clouds"
  const char* city_name = doc["name"]; // "Milan"

  Serial.println("Response:");
  Serial.println(city_name);
  Serial.println(weather_0_main);
}

void task_audioStreamCallback(){
	if(!music.connected()){
      Serial.println("Reconnecting for audio stream...");
      if(music.connect(httpHost, httpPort)){
        music.print(String("GET ") + httpPath + " HTTP/1.1\r\n" +
                  "Host: " + httpHost + "\r\n" +
                  "Connection: close\r\n\r\n");
        Serial.println("connected to stream source");
      }
  }

  //play stream
  if(music.available() > 0){
    uint8_t bytesread = music.readBytes(mp3buff, BUFF);
    player.playChunk(mp3buff, bytesread);
  }

  //code to turn on/off the volume and dynamic radio station change
  if (Serial.available() > 0) {
    // read the incoming byte for serial input control;
    incomingByte = Serial.read();
    switch(incomingByte){
      case 115: // ascii 's' = turn off volume
        player.setVolume(0);
        Serial.println("VOLUME : 0");
      break;

      case 97:  //ascii 'a' = turn on volume
        player.setVolume(VOLUME);
        Serial.print("VOLUME : "); Serial.println(VOLUME);
      break;

      case 49: // ascii '1'
        Serial.println("Station 1");
        connect_radio(&radio1);
      break;

      case 50: // ascii '2'
        Serial.println("Station 2");
        connect_radio(&radio2);
      break;

      case 51: // ascii '3'
        Serial.println("Station 3");
        connect_radio(&radio3);
      break;

      case 52: // ascii '4'
        Serial.println("Station 4");
        connect_radio(&radio4);
      break;

      case 53: // ascii '5'
        Serial.println("Station 5");
        connect_radio(&radio5);
      break;

      case 54: // ascii '6'
        Serial.println("Station 6");
        connect_radio(&radio6);
      break;
      }
  }
}

void task_mqttCallback(){
  // Connect if we're not already connected.
  if (!psclient.connected())
    reconnect();

  // process any events.
  psclient.loop();
}

//callback assigned to all mqtt client
void callback(char* topic, byte* payload, unsigned int length) {

  //parsing the payload
  char msg[length];
  for (int i = 0; i < length; i++) {
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';

  if(strcmp(topic, "PhysiRadio/Volume") == 0){
    if(strcmp(msg, "mute") == 0){
      Serial.println("Muting volume");
      player.setVolume(0);
      Serial.println("VOLUME : 0");
    }

    if(strcmp(msg, "unmute") == 0){
      Serial.println("UnMuting volume");
      player.setVolume(TMP_VOLUME);
      Serial.print("VOLUME : "); Serial.println(TMP_VOLUME);
    }

    if(strcmp(msg, "+5") == 0){
      Serial.println("Volume +5");
      TMP_VOLUME+=5.0;
      if (TMP_VOLUME <= 100.0){
        player.setVolume(TMP_VOLUME);
      }else{
        TMP_VOLUME = 100.0;
      }
      VOLUME = TMP_VOLUME;
      Serial.print("VOLUME : "); Serial.println(TMP_VOLUME);
    }

    if(strcmp(msg, "-5") == 0){
      Serial.println("Volume -5");
      TMP_VOLUME-= 5.0;
      if(TMP_VOLUME >= 0.0){
        player.setVolume(TMP_VOLUME);
      }else{
        TMP_VOLUME = 0.0;
      }
      VOLUME = TMP_VOLUME;
      Serial.print("VOLUME : "); Serial.println(TMP_VOLUME);
    }
  }

  if(strcmp(topic, "PhysiRadio/Station") == 0){
    if(strcmp(msg , "station1") == 0){
      Serial.println(msg);
      Serial.println("Station 1");
      connect_radio(&radio1);
    }

    if(strcmp(msg , "station2") == 0){
      Serial.println(msg);
      Serial.println("Station 2");
      connect_radio(&radio2);
    }
  }
}

void reconnect() {

  //it was a while before
  if (!psclient.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (psclient.connect(clientId.c_str())) {
      Serial.println(clientId + " connected to MQTT server");

      // Once connected, publish an announcement...
      //client.publish("meta", "We're connected");

      // subscribe to all topics
      //psclient.subscribe("#");
      // all subtopic
      psclient.subscribe("PhysiRadio/#");

    } else {
      Serial.print("failed, rc=");
      Serial.print(psclient.state());
    }
  }
}


void fade_in_audio(){
  TMP_VOLUME = 0.0;
  while (TMP_VOLUME < VOLUME) {
    if(music.available() > 0){
        uint8_t bytesread = music.readBytes(mp3buff, BUFF);
        player.playChunk(mp3buff, bytesread);
        player.setVolume(TMP_VOLUME);
        TMP_VOLUME += COUNT_FADE;
        Serial.println(TMP_VOLUME);
      }
  }
}

void fade_out_audio(){

  while(TMP_VOLUME >= 0.3){
  if(music.available() > 0){
      uint8_t bytesread = music.readBytes(mp3buff, BUFF);
      player.playChunk(mp3buff, bytesread);
      player.setVolume(TMP_VOLUME);
      TMP_VOLUME -= COUNT_FADE;
      Serial.println(TMP_VOLUME);
    }
  }
}


void connect_radio(radioStation *radio){
	fade_out_audio();

	httpHost = radio->host;
	httpPath = radio->path;
	httpPort = radio->port;

	if (!music.connect(httpHost, httpPort)) {
	  Serial.println("Radio Connection failed");
	  fade_in_audio();
	  return;
	}
	Serial.print("Requesting stream: ");
	Serial.println(httpPath);
	music.print(String("GET ") + httpPath + " HTTP/1.1\r\n" +
				"Host: " + httpHost + "\r\n" +
				"Connection: close\r\n\r\n");

	
	fade_in_audio();
	Serial.println(radio->name);

}
