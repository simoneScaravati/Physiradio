# Physiradio
IoT project related to my Computer Science Degree Thesis.
Specifically, it is an IoT device (based on Wemos D1 mini ESP8266 MCU, developed on Arduino), which takes the name of Physiradio, which reproduces specific musical flows according to a mapping that translates the weather conditions, accessible through open APIs (open data from OpenWeatherMap), into musical genres and colors; a process which is named musicalization, which is based on a whole study on data physicalization.

## THESIS FOLDER
LaTex, csv, images and all files related to the theoric/descriptive part.

## SOFTWARE FOLDER
There are 2 main sections: the Arduino software (with wirings and MQTT commands) and the Unity Standalone software to interact with the device.

Main features of Arduino script: 
- JSON file interpretation
- Implementation of Task Scheduler library
- Interface with VS1053 codec audio
- Interrogation of remote APIs and webradio streams
- Implementation of MQTT protocol

### Unity App Install Tips
**Win/Linux and Mac Os Standalone software developed totally not exported just like the Android version.**

To run the program:

Windows: just double-click the .exe file

Linux: make the .x86_64 file as executable (On Ubuntu: Right click the file, Choose 'Properties', Go to the 'Permissions' tab, Tick 'Allow executing this file as a program',  Close the dialog.)

Mac OS X : if the Mac doesn't run the .app file:
	 First reach at same folder at which your mac build exist. Then within terminal program execute this link of code:  chmod a+x MAC\ OS\ X\ Standalone.app/Contents/MacOS/*

Android: Install the .apk file on your smartphone. Google may ask you to trust third party software, please trust me :) 

