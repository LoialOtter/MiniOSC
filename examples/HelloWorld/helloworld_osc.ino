// WARNING !!!!!!

// THIS IS UNTESTED - i just wrote it to show how to do things and haven't had time to test yet.

// WARNING !!!!!!
// TODO: remove this warning after testing


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "OSC.h"

#include "UDP_OSC.h"
#include "SLP_OSC.h"



const char* ssid = "esp_osc_test";
const char* password = "esp_osc_test";

SLP_OSC_Class SLP_OSC = SLP_OSC_Class(115400);
UDPOSC_Class UDP_OSC = UDPOSC_Class(10005, 10006);

void respond(const char* path, void* args) {
  osc_on_message_args* on_message_args = (osc_on_message_args*) args;
  osc_message* message;
  
  // make sure we get the parts we need
  if (on_message_args && on_message_args->message) {
    // set up the helper variable
    message = on_message_args->message;

    // if the type of the first argument is a string, reply using it
    if (message->typetag[1] == 's') {
      // parse the data out
      OSC.parseData(message);
      OSC.Send("/hello/world", ",ss", "Hello", message->data[0].value_string);
      // free the memory
      OSC.freeData(message);
    }
    else {
      OSC.Send("/hello/world", ",ss", "Hello", "someone");
    }
  }
}


static int percent, lastPercent; // note, scaled to 5% marks

void setup(void){
  // Serial
  //Serial.begin(115400); // this should be handled with the baudrate setting above
  
  WiFi.softAP(ssid, password);
  
  // OTA Update
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp_osc_test");
  ArduinoOTA.setPassword("Password");
  ArduinoOTA.onStart([]() { OSC.Send("/OTA/Start", ","); });
  ArduinoOTA.onEnd([]() { OSC.Send("/OTA/End", ","); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      // This is just so it'll update every 5% instead of all the time
      percent = (progress / (total / 20));
      if (lastPercent != percent) {
        lastPercent = percent;
        OSC.Send("/OTA/progress", ",ii", percent * 5, total);
      }
    });
  ArduinoOTA.onError([](ota_error_t error) {
      if (error == OTA_AUTH_ERROR)         OSC.Send("/OTA/Error", ",is", error, "Auth Failed");
      else if (error == OTA_CONNECT_ERROR) OSC.Send("/OTA/Error", ",is", error, "Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) OSC.Send("/OTA/Error", ",is", error, "Receive Failed");
      else if (error == OTA_END_ERROR)     OSC.Send("/OTA/Error", ",is", error, "End Failed");
    });
  ArduinoOTA.begin();
  

  // OSC
  OSC.Start();
  SLP_OSC.Start();
  UDP_OSC.Start();

  OSC.addInterface(&SLP_OSC);
  OSC.addInterface(&UDP_OSC);

  OSC.onMessage("**", &osc_repeat);
  OSC.onMessage("/hello", &respond);
}



static int testDelayCount = 0;
static int itteration = 0;

void loop(void){
  ArduinoOTA.handle();
  if (testDelayCount++ > 100000) {
    testDelayCount = 0;
    OSC.Send("/test", ",is", itteration++, "this is a test");
  }
}
