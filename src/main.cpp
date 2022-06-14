#include <Arduino.h>
//networking
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>

//ntp
#include <NTPClient.h>
#include <WiFiUdp.h>

//googlescript
#include <HTTPSRedirect/HTTPSRedirect.h>
#include <HTTPSRedirect/DebugMacros.h>

//servo
#include <Servo.h>

//json
#include <ArduinoJson.h>

//timer0 ticker
#include <Ticker.h>

//scale
#include <HX711.h>

//mqtt
#include <PubSubClient.h>

// configuration
#include <EEPROM.h>
#define CONFIG_SIZE 1024
#define CONFCHK 123425321

struct config_st {
  int  confchk;

  char host[32];
  char ssid[32];
  char psk[64];

  char mqttserver[32];
  int  mqttport;
  char mqtttopic[16];

  float scale;
  float scale_offset;
  int  w_closed;
  int  w_precision;
  int  w_delta;
  float  w_stdd_threshold;
  float w_door;
  int w_delta_open;
  int w_delta_close;

  int dooropened;
  int doorclosed;
  int door_h_close;
  int door_m_close;
  int door_h_open;
  int door_m_open;

  int servo_neutral;
  int servo_rotation;
  int speed_slow;
  int speed_fast;

  int configured;
};

config_st config;

// default values
#ifndef HOST

  #define HOST "hensomedoor"
  #define SSID "myssid"
  #define PSK  "myssidpassword"

  #define NTPSERVER "fr.pool.ntp.org"

  #define MQTTSERVER "mymqttserver.somewhere.net"
  #define MQTTPORT 1883
  #define MQTTTOPIC "hensomedoor"

  #define W_ZERO 10
  #define W_PRECISION 10
  #define W_DELTA 100
  #define W_DELTA_OPEN 75
  #define W_DELTA_CLOSE 100
  #define SCALE 3800.0f
  #define SCALE_OFFSET 0.0f
  #define W_DOOR 400
  #define W_STDD_THRESHOLD 1000.0f

  #define DOOR_OPENED false
  #define DOOR_CLOSED false
  #define DOOR_H_CLOSE 19
  #define DOOR_M_CLOSE 00
  #define DOOR_H_OPEN 8
  #define DOOR_M_OPEN 0

  #define SERVO_STOP 92
  #define SERVO_ROTATION -1
  #define SPEED_SLOW 4
  #define SPEED_FAST 10

  #define CONFIGURED 0

  #define SCALE_PERIOD 100
  #define SCALE_MEASURES 10

#endif



//web server
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

//ntp
WiFiUDP ntpUDP;

//ntp
NTPClient timeClient(ntpUDP, NTPSERVER, 7200, 60000);

//led
#define LED 4

//motor params
#define SERVO_PIN 2
Servo servo;
Ticker ticker;

//interrupts
#define INTERRUPTPIN 00
volatile byte interrup = false;
int nbinterrupt = 0;

//load cell
// HX711 circuit wiring
#define LOADCELL_DOUT_PIN 13
#define LOADCELL_SCK_PIN 12
#define LOADCELL_SCALE 64
HX711 scale;
float scalevalue;
float scalemean;
float scalestdd;
float scalevalues[SCALE_MEASURES];
int   scaleincreases = false;
int   scaledecreases = false;
int   scaleevolution = 0;
byte scaleindex = 0;


//mqtt
WiFiClient netclient;
PubSubClient mqttclient(netclient);

//RTC
// json from google doc
String json;
int hour_ref;
int min_ref;
int sec_ref;
int hour_now = 0;
int min_now = 0;

//command control
int action_running = false;

//code

void slog(const char msg[]) {
  Serial.println(msg);
}

void save_config() {
  EEPROM.put(0, config);
  if (EEPROM.commit())  {
    slog("save_config done");
  }
  else {
    slog("save_config error");
  }
}


void ConfigPage(const char message[]) {
  digitalWrite(LED, 1);
  const int sizebuf = 2000;
  char temp[sizebuf];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  const char* pageroot = "<html>\
  <head>\
    <meta name=\"viewport\" content=\"width=device-width\" />\
    <title>HSD</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
     <h1>Hen Some Door!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    %s\
    <form name=\"config\" method=\"post\" action=\"/config\">\
    <table><tbody>\
      <tr><td>w_stdd_threshold </td><td><input type=\"text\" name=\"w_stdd_threshold\" value=\"%.2f\"></td></tr>\
      <tr><td>servo_neutral </td><td><input type=\"text\" name=\"servo_neutral\" value=\"%d\"></td></tr>\
      <tr><td>w_delta_open </td><td><input type=\"text\" name=\"w_delta_open\" value=\"%d\"></td></tr>\
      <tr><td>w_delta_close </td><td><input type=\"text\" name=\"w_delta_close\" value=\"%d\"></td></tr>\
      <tr><td>w_precision </td><td><input type=\"text\" name=\"w_precision\" value=\"%d\"></td></tr>\
      <tr><td>scale </td><td><input type=\"text\" name=\"scale\" value=\"%.2f\"></td></tr>\
      <tr><td>servo_rotation </td><td><input type=\"text\" name=\"servo_rotation\" value=\"%d\"></td></tr>\
      <tr><td>w_door </td><td><input type=\"text\" name=\"w_door\" value=\"%.0f\"></td></tr>\
      <tr><td>mqttserver </td><td><input type=\"text\" name=\"mqttserver\" value=\"%s\"></td></tr>\
    </tbody></table>\
    <tr><td><input type=\"submit\" value=\"update\">\
    </form>\
    <br>\
    <table><tbody>\
    <tr><td>\
    <form name=\"restore\" method=\"post\" action=\"/restore\"><input type=\"submit\" value=\"restore\"></form>\
    </td><td>\
    <form name=\"tare\" method=\"post\" action=\"/tare\"><input type=\"submit\" value=\"tare\"></form>\
    </td><td>\
    <form name=\"reset\" method=\"post\" action=\"/reset\"><input type=\"submit\" value=\"reset\"></form>\
    </td></tr>\
    <tr><td><form method=\"post\" action=\"/up\"><input type=\"submit\" value=\"UP\"></form>\
    </td><td>\
    <form method=\"post\" action=\"/down\"><input type=\"submit\" value=\"DOWN\"></form>\
    </td></tr>\
    <tr><td>\
    <a href=\"/\">Main Page</a>\
    </td></tr></tbody></table>\
  </body>\
  </html>";
  snprintf(temp, sizebuf, pageroot, hr, min % 60, sec % 60, message, config.w_stdd_threshold,
          config.servo_neutral, config.w_delta_open, config.w_delta_close, config.w_precision,
          config.scale, config.servo_rotation, config.w_door, config.mqttserver);
  server.send(200, "text/html", temp);
  digitalWrite(LED, 0);
}

void handleConfig() {
  if ( server.args() > 0) {
    config.w_stdd_threshold = server.arg("w_stdd_threshold").toInt();
    config.servo_neutral = server.arg("servo_neutral").toInt();
    config.w_closed = server.arg("w_closed").toFloat();
    config.w_door = server.arg("w_door").toFloat();
    config.w_delta = server.arg("w_delta").toInt();
    config.w_delta_open = server.arg("w_delta_open").toInt();
    config.w_delta_close = server.arg("w_delta_close").toInt();
    config.w_precision = server.arg("w_precision").toInt();
    config.scale = server.arg("scale").toFloat();
    config.servo_rotation = server.arg("servo_rotation").toInt();
    config.w_door = server.arg("w_door").toInt();
    strcpy(config.mqttserver, (const char*) server.arg("mqttserver").c_str());
    save_config();
    ConfigPage("Configuration saved");
  }
  else {
    ConfigPage("Welcome to configuration page");
  }
  slog("handleConfig");
}


void setup_config();

void handleRestore () {
  config.confchk = 0;
  save_config();
  setup_config();
  ConfigPage("Configuration defaults restored");
}

void handleReset () {
  server.send(200, "text/html", "reset command commiting");
  delay(2000);
  slog("handleReset");
  ESP.reset();
}

void handleMain(const char message[]) {
  digitalWrite(LED, 1);
  const int sizebuf = 2000;
  char temp[sizebuf];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  char door_status[30];
  if (config.dooropened){
    if (config.doorclosed) {
      strcpy(door_status, "Error");
    }
    else {
      strcpy(door_status, "Opened");
    }
  }
  else {
    if (config.doorclosed) {
      strcpy(door_status, "Closed");
    }
    else {
      strcpy(door_status, "Init");
    }
  }

  const char* pageroot = "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <meta name=\"viewport\" content=\"width=device-width\" />\
    <title>HSD</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hen Some Door!</h1>\
    <p>Time: %02d:%02d</p>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Scale: %d</p>\
    <p>Door: %s</p>\
    %s\
    <tr>\
      <td><form method=\"post\" action=\"/open\"><input type=\"submit\" value=\"Open\"></form></td>\
      <td><form method=\"post\" action=\"/close\"><input type=\"submit\" value=\"Close\"></form></td>\
    </tr>\
    <form name=\"schedule\" method=\"post\" action=\"/schedule\">\
    <table><tbody>\
      <tr><td>Open at </td><td><input type=\"text\" size=\"2\" name=\"door_h_open\" value=\"%02d\">h</td><td><input type=\"text\" size=\"2\" name=\"door_m_open\" value=\"%02d\">m</td></tr>\
      <tr><td>Close at </td><td><input type=\"text\" size=\"2\" name=\"door_h_close\" value=\"%02d\">h</td><td><input type=\"text\" size=\"2\" name=\"door_m_close\" value=\"%02d\">m</td></tr>\
    </tboody></table>\
    <input type=\"submit\" value=\"Schedule!\">\
    </form>\
    <br>\
    <a href=\"/config\">Config</a>\
  </body>\
  </html>";
  snprintf(temp, sizebuf, pageroot, hour_now, min_now, hr, min % 60, sec % 60, (int) scalevalue, door_status, message,
          config.door_h_open, config.door_m_open, config.door_h_close, config.door_m_close);
  server.send(200, "text/html", temp);
  digitalWrite(LED, 0);
}

void handleRoot() {
  handleMain("Welcome");
}

void handleSchedule () {
    if ( server.args() > 0) {
    config.door_h_close = server.arg("door_h_close").toInt();
    config.door_m_close = server.arg("door_m_close").toInt();
    config.door_h_open = server.arg("door_h_open").toInt();
    config.door_m_open = server.arg("door_m_open").toInt();
    save_config();
    handleMain("Schedule saved");
  }
  else {
    handleMain("No data to schedule");
  }
  slog("handleSchedule");
}

// scale functions

float scale_mean() {
  int i;
  float mean = 0;
  for (i=0; i < SCALE_MEASURES; i++) {
    mean = mean + scalevalues[i];
  }
  scalemean = mean / SCALE_MEASURES;;
  return scalemean;
}

float scale_stdd() {
  int i;
  float dev = 0.0f;
  for (i=0; i < SCALE_MEASURES; i++) {
    dev = dev + (scalevalues[i] - scalemean) * (scalevalues[i] - scalemean);
  }
  scalestdd = dev/SCALE_MEASURES;
  return scalestdd;
}

int scale_changes() {
  if (scalestdd > config.w_stdd_threshold) {
    return true;
  }
  else {
    return false;
  }
}

void scale_logger() {
  char str[200];
  snprintf(str, 200, "{ \"scale\": %.0f, \"mean\": %.0f, \"dev\": %.4f, \"action\": %d, \"evol\": %d }", scalevalue, scalemean, scalestdd, action_running, scaleevolution);
  mqttclient.publish(config.mqtttopic,str);
}

float scale_read() {
  if (scale.is_ready()) {
     scalevalue = scale.get_units(1);
  }
  scalevalues[scaleindex] = scalevalue;
  scaleindex++;
  if (scaleindex == SCALE_MEASURES) {
    scaleindex = 0;
  }
  scale_mean();
  scale_stdd();
  scale_logger();
  return scalevalue;
}

int scale_increases() {
  scale_read();
  if (scaleincreases) {
    return true;
  }
  if (scale_changes()) {
    if ( scalevalue > (scalemean + config.w_delta + config.w_precision))  {
      scaleincreases = true;
      return true;
    }
  }
  scaleincreases = false;
  return false;
}

int scale_decreases() {
  scale_read();
  if (scaledecreases) {
    return true;
  }
  if (scale_changes()) {
    if ( scalevalue < (scalemean - config.w_delta - config.w_precision)) {
      scaledecreases = true;
      return true;
    }
  }
  scaledecreases = false;
  return false;
}

int scale_evolution() {
  scale_read();
  if (scaleevolution != 0) {
    return scaleevolution;
  }
  if (scale_changes()) {
    // -1 if weight decreases when closing
    if ( scalevalue < (scalemean - config.w_delta_close - config.w_precision)) {
      scaleevolution = -1;
      return scaleevolution;
    }
    // +1 if weight increases when opening
    if ( scalevalue > (scalemean + config.w_delta_open + config.w_precision))  {
      scaleevolution = 1;
      return scaleevolution;
    }
  }
  return scaleevolution;
}


// door functions

void stopDoor() {
  servo.write(config.servo_neutral);
  delay(100);
  servo.detach();
  interrup = false;
  slog("shutdoor");
}

void closeDoor() {
  action_running = true;
  scaleevolution = 0;
  servo.attach(SERVO_PIN);
  servo.write(config.servo_neutral+config.speed_fast*config.servo_rotation);
  while ((scale_evolution() == 0) && !interrup) {
    delay(SCALE_PERIOD);
  }
  //wrong direction
  if ( scaleevolution == 1 ) {
    config.servo_rotation = - config.servo_rotation;
    delay(1000);
    closeDoor();
  }

  servo.write(config.servo_neutral-config.speed_slow*config.servo_rotation);
  delay(1500);
  stopDoor();
  config.doorclosed = true;
  save_config();
  slog("closeDoor");
  action_running = false;
}

void openDoor() {
  action_running = true;
  scaleevolution = 0;
  servo.attach(SERVO_PIN);
  servo.write(config.servo_neutral-config.speed_slow*config.servo_rotation);
  delay(1500);
  servo.write(config.servo_neutral-config.speed_fast*config.servo_rotation);
  while ((scale_evolution() == 1) && !interrup) {
    delay(SCALE_PERIOD);
  }
  while ((scale_evolution() == 0) && !interrup) {
    delay(SCALE_PERIOD);
  }
  
  //string tension
  servo.write(config.servo_neutral+config.speed_slow*config.servo_rotation);
  delay(500);
  stopDoor();

  config.dooropened = true;
  save_config();
  slog("openDoor");
  action_running = false;
}

void handleClose() {
  if (! config.doorclosed || config.dooropened) {
    closeDoor();
    server.send(200,"text/html","<body>Door Closed<br><a href=\"/\">Main</a>");
  }
  else {
    server.send(200,"text/html","<body>Door was already Closed !<br><a href=\"/\">Main</a>");
  }
  config.dooropened = false;
  save_config();
  slog("handleClose");
}

void handleOpen() {
  if (! config.dooropened || config.doorclosed) {
    openDoor();
    server.send(200,"text/html","<body>Door Opened<br><a href=\"/\">Main</a>");
  }
  else {
    server.send(200,"text/html","<body>Door was already Opened !<br><a href=\"/\">Main</a>");
  }
  config.doorclosed = false;
  save_config();
  slog("handleOpen");
}

void timeClose() {
  if (! config.doorclosed || config.dooropened) {
    closeDoor();
  }
  config.dooropened = false;
  save_config();
  slog("timeClose");
}

void timeOpen() {
  if (! config.dooropened || config.doorclosed) {
    openDoor();
  }
  config.doorclosed = false;
  save_config();
  slog("timeOpen");
}

void handleDown() {
  action_running = true;
  config.dooropened = false;
  int waitloop = 2000;
  servo.attach(SERVO_PIN);
  servo.write(config.servo_neutral+config.speed_fast*config.servo_rotation);
  while ((!scale_decreases()) || (waitloop < 0)) {
    delay(SCALE_PERIOD);
    waitloop -= 2000/50;
  }
  if (scaledecreases) {
    config.doorclosed = true;
    save_config();
  }
  stopDoor();
  server.send(200, "text/plain", "<body>Down done for 2s.<br><a href=\"/\">Main page</a></body>");
  slog("handleDown");
  action_running = false;
}

void handleUp() {
  action_running = true;
  config.doorclosed = false;
  int waitloop = 2000;
  servo.attach(SERVO_PIN);
  servo.write(config.servo_neutral-config.speed_fast*config.servo_rotation);
  while ((!scale_increases()) || (waitloop < 0)) {
    delay(SCALE_PERIOD);
    waitloop -= 2000/50;
  }
  if (scaleincreases) {
    config.dooropened = true;
    save_config();
  }
  stopDoor();
  server.send(200, "text/plain", "<body>Up done for 2s.<br><a href=\"/\">Main page</a></body>");
  slog("handleUp");
  action_running = false;
}

//interruption

void ICACHE_RAM_ATTR handleInterrupt() {
  interrup = true;
  scaledecreases = true;
  scaleincreases = true;
  scaleevolution = 0;
  config.dooropened = false;
  config.doorclosed = false;
  save_config();
  nbinterrupt++;
}

// handles web

void handleAngle() {
  config.servo_neutral = server.arg("angle").toInt();
  if (servo.attached()) {
    servo.write(config.servo_neutral);
    delay(100);
    servo.detach();
  }
  save_config();
  handleMain("Angle servo neutral saved");
  slog("handleAngle");
}

void handleDelta() {
  config.w_delta = server.arg("delta").toInt();
  save_config();
  handleMain("Delta weight save");
  slog("handleDelta");
}

void setup_scale();

void handleTare() {
  scale.tare(10);
  config.scale_offset = scale.get_offset();
  save_config();
  ConfigPage("HSD Tared !");
  slog("handleTare");
}

void handleTime() {
  digitalWrite(LED, 1);
  server.send(200, "text/plain", (char *) timeClient.getFormattedTime().c_str());
  digitalWrite(LED, 0);
}

void handleNotFound() {
  digitalWrite(LED, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(LED, 0);
}



void serial_update(void) {
  if (!mqttclient.connected()) {
    mqttclient.connect(config.mqtttopic);
  }
  char str[100];
  snprintf(str, 100,"servo: %03d, int: %02d",servo.read(), nbinterrupt);
  slog(str);
  snprintf(str, 100,"pin: %02d",digitalRead(INTERRUPTPIN));
  slog(str);
  snprintf(str, 100,"{ \"dooropen\": %d, \"doorclosed\": %d, \"rotation\": %d }", config.dooropened, config.doorclosed, config.servo_rotation);
  mqttclient.publish(config.mqtttopic,str);
  slog(str);
  scale_read();
  snprintf(str, 100,"{ \"scale_evolution\": %d, \"scale_changes\": %d }", scale_evolution(), scale_changes());
  mqttclient.publish(config.mqtttopic,str);
  slog(str);
  snprintf(str, 100,"{ \"scale\": %.0f, \"mean\": %.0f, \"dev\": %.8f }", scalevalue, scalemean, scalestdd);
  mqttclient.publish(config.mqtttopic,str);
  slog(str);
}

//time mgmt

const char* google = "script.google.com";
// Replace with your own script id to make server side changes
const char *GScriptId = "AKfycby4gVz61EkTZBC5z9TyoYlJeNqo9d11Pdx8pyHc9pY9mqzpYZr4";

String url = String("/macros/s/") + GScriptId + "/exec";
HTTPSRedirect* client;

void get_google_time() {
  client = new HTTPSRedirect();
  client->setInsecure();
  client->setContentTypeHeader("application/json");
  bool flag = false;
  for (int i=0; i<5; i++){
    int retval = client->connect(google, 443);
    if (retval == 1) {
       flag = true;
       break;
    }
  }
  if ( flag ) { slog("setup_google done"); }
  else { slog("setup_google ,ot connected"); }

  if (client->GET(url, google)) {
    json = client->getResponseBody();
  }
  else {
    json = "";
  }
  slog((char*) json.c_str());
}

void setup_time(void) {
  get_google_time();
  //StaticJsonDocument<500> doc;
  DynamicJsonDocument doc(2048);

  DeserializationError error = deserializeJson(doc, json);

  // Test if parsing succeeds.
  if (error) {
    slog("Json error!");
    server.send(500, "text/plain", (char *)error.c_str());
    return;
  }
  hour_ref = doc["hour"].as<int16_t>();
  min_ref = doc["minute"].as<int16_t>();
  sec_ref = millis() / 1000;
}

void update_time() {
  int sec = ((millis() / 1000) - sec_ref);
  int min = sec / 60 + min_ref + hour_ref*60;
  if ( (min / 60 ) > 24 ) {
    setup_time();
    sec = ((millis() / 1000) - sec_ref);
    min = sec / 60 + min_ref + hour_ref*60;
  }

  hour_now = (min / 60) % 24;
  min_now = min % 60;
}

void setup_wifi(void) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.psk);
  slog("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  Serial.print("Connected to ");
  slog(config.ssid);
  Serial.print("IP address: ");
  char temp[16];
  snprintf(temp, 16, "%s", WiFi.localIP().toString().c_str());
  slog(temp);

  if (MDNS.begin(config.host)) {
    slog("MDNS responder started");
  }

}

void setup_ota(void) {

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    slog("OTA Start updating ");
  });
  ArduinoOTA.onEnd([]() {
    slog("\nOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.print(".");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      slog("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      slog("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      slog("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      slog("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      slog("End Failed");
    }
  });
  ArduinoOTA.begin();
  slog("setup_ota done");
}

void setup_interrupt() {
  pinMode(INTERRUPTPIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPTPIN), handleInterrupt, FALLING);
  slog("setup_interrupt");
}

void setup_scale() {
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN, LOADCELL_SCALE);
    scale.power_down();
    scale.power_up();
  // scale.power_up();
  // scale.set_gain(64);
  // the setup function runs once when you press reset or power the board
  while ( ! scale.is_ready() ) {
    yield();
  }
  scale.set_scale(config.scale);
  scale.set_offset(config.scale_offset);
  int i;
  for (i=0; i < SCALE_MEASURES; i++) {
    scalevalues[i] = 0.0f;
  }
  slog("setup_scale done");
}

void setup_motor() {
  servo.attach(SERVO_PIN);
  servo.write(config.servo_neutral);
  delay(100);
  servo.detach();
  slog("setup_motor done");
}

void setup_mqtt() {
  mqttclient.setServer(config.mqttserver,config.mqttport);
  mqttclient.connect(config.mqtttopic);
  slog("setup_mqtt done");
}

void handleNeutral() {
  servo.attach(SERVO_PIN);
  servo.write(config.servo_neutral);
  delay(4000);
  servo.detach();
    server.send(500, "text/plain", "handle neutral done");
  slog("handleNEutral done");
}


void setup_webserver() {
  server.on("/", handleRoot);
  server.on("/time", handleTime);
  server.on("/close", handleClose);
  server.on("/open", handleOpen);
  server.on("/up", handleUp);
  server.on("/down", handleDown);
  server.on("/angle", handleAngle);
  server.on("/delta", handleDelta);
  server.on("/tare", handleTare);
  server.on("/config", handleConfig);
  server.on("/restore", handleRestore);
  server.on("/neutral", handleNeutral);
  server.on("/reset", handleReset);
  server.on("/schedule", handleSchedule);

  server.onNotFound(handleNotFound);

  httpUpdater.setup(&server);
  server.begin();

  MDNS.addService("http", "tcp", 80);
  slog("HTTPUpdateServer ready!");
}

void setup_config() {
  EEPROM.begin(CONFIG_SIZE);
  EEPROM.get(0,config);
  slog("setup_config start");
  if (config.confchk != CONFCHK) {
    slog("setup_config no config found");

    config.confchk = CONFCHK;

    strcpy(config.host, HOST);
    strcpy(config.ssid, SSID);
    strcpy(config.psk, PSK);

    strcpy(config.mqttserver, MQTTSERVER);
    config.mqttport = MQTTPORT;
    strcpy(config.mqtttopic, MQTTTOPIC);

    config.scale = SCALE;
    config.scale_offset = SCALE_OFFSET;
    config.w_precision = W_PRECISION;
    config.w_stdd_threshold = W_STDD_THRESHOLD;
    config.w_delta = W_DELTA;
    config.w_door = W_DOOR;
    config.w_delta_open = W_DELTA_OPEN;
    config.w_delta_close = W_DELTA_CLOSE;
    config.w_closed = W_ZERO;
    config.w_delta_close = W_DELTA;

    config.dooropened = DOOR_OPENED;
    config.doorclosed = DOOR_CLOSED;
    config.door_h_close = DOOR_H_CLOSE;
    config.door_m_close = DOOR_M_CLOSE;
    config.door_h_open = DOOR_H_OPEN;
    config.door_m_open = DOOR_M_OPEN;

    config.servo_neutral = SERVO_STOP;
    config.servo_rotation = SERVO_ROTATION;
    config.speed_slow = SPEED_SLOW;
    config.speed_fast = SPEED_FAST;

    config.configured = CONFIGURED;

    save_config();
  }
  else {
    slog("Config found !");
  }
  slog("setup_config done");
}


void setup(void) {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 0);
  Serial.begin(115200);

  setup_config();

  setup_wifi();

  setup_webserver();

  setup_ota();

  timeClient.begin();
  setup_time();
  setup_motor();

  setup_interrupt();
  setup_scale();
  setup_mqtt();
}

void loop(void) {
  // no ntp finally
  // timeClient.update();
  if ( ! action_running ) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(config.ssid, config.psk);
      delay(1000);
    }
    ArduinoOTA.handle();
    server.handleClient();
    MDNS.update();
    update_time();
    if ( (hour_now == config.door_h_open) and (min_now == config.door_m_open)) {
      timeOpen();
    }
    if ( (hour_now == config.door_h_close) and (min_now == config.door_m_close)) {
      timeClose();
    }
    if (!mqttclient.connected()) {
      mqttclient.connect(config.mqtttopic);
    }
    serial_update();
  }
  delay(1000);
}
