# hensomedoor
An handsome door for any hen house.
## Object
This is an automated servo mecanism based on internal weight sensor. The prototype served as an chicken house door.

## Principle
The main concept here is the servo's frame as a scale load strain gauge cell based :
![hensomedoor v3 1_load_cell](https://user-images.githubusercontent.com/22861667/173616888-e8660be3-e299-443e-89c4-1c84f39ecf87.jpg)

## Description
A load is attached with a string on the axel of the servo to be driven up and down.
The servo is mounted on a load cell with strain-gauge sensor based frame.  
When apparent weight changes, the program reacts consequently.  

The code as the ability to sense the apparente weight of the servo's load (a wood door for instance) and decide what would be done :
- When the weight increases, the load arrives at maximum position, then stop the servo and set maximum position reached.
- When the weight decreases, the load arrives at minimum position, then stop the servo and set minimum position reached.
![proto](https://github.com/innofocus/hensomedoor/blob/master/Photos/20220614_153121.jpg?raw=true)

## Features
The demonstration code is able to do :
- Connect to Wifi
- Web configuration with web interface
- Save the configuration to eeprom
- MQTT: send weight measured at the load cell to an mqtt server with influxdb and grafana for consultation or survey.
- Auto Setup: the current time and date setup at start from a public google script (seen gs/today) - publish at https://script.google.com/macros/s/AKfycby4gVz61EkTZBC5z9TyoYlJeNqo9d11Pdx8pyHc9pY9mqzpYZr4/exec
- Future ability to use google calendar for long scheduling, or sun dawn/rise synchronisation
- OTA update : in order to update the firmware without any cable

## web interface
The main page allows to:
- open or close the door
- read the weight
- read the current status open/close
- set opening and closing time schedule
![pageroot](https://user-images.githubusercontent.com/22861667/173614755-403a22eb-2677-46d6-8b51-3baa6bd55188.jpg)

The config page let:
- fine tune parameters
- open or close by increment
- tare the load weight
- play with detector's parameters
![config](https://user-images.githubusercontent.com/22861667/173614800-05c1c658-c6eb-4a4b-ba23-bdf35bfde8f4.jpg)

## Building one
List of materials:
- ESP8266
- SG10 servo motor
- voltage regulators for ESP and servo
- HX711 module
- 4 strain gauge resistors
- cyanoacrylate to glue strain gauge resistors on load cell 3D printed frame.
- tiny wires for strain gauge soldering (ones from wired ear plug are perfect).
See 3D STL files to print the complete box.  

This is my chicken house's door at work :  
![opening](https://github.com/innofocus/hensomedoor/blob/master/Videos/opening.gif?raw=true)
![closing](https://github.com/innofocus/hensomedoor/blob/master/Videos/closing.gif?raw=true)


