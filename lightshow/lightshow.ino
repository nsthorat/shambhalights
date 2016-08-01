#include <WebSockets.h>
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Adafruit_NeoPixel.h>

#define PIN 14
#define PIXELS 10

// WIFI Stuff
ESP8266WiFiMulti WiFiMulti;
WebSocketsServer webSocket = WebSocketsServer(8080);

// Strip stuff
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Current state, contains all possible parameters.
int state[10] = {10, 0, 0, 127, /* color */ 0, 0, 0, 0, 0, 0};

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
   switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
           Serial.println("connected... ");
            break;
        case WStype_TEXT: 
        {
            Serial.printf("[%u] Got payload: %s\n", num, payload);
            
            // Read each command pair
            int i = 0;
            char* command = strtok((char*) payload, ",");
            while (command != 0) {
              int value = atoi(command);
              state[i] = value;
              Serial.printf("value[%d] = [%d]\n", i, value);

              command = strtok(0, ",");
              i++;
            }
        }
            break;
        case WStype_BIN:
            break;
    }
}

int time_counter;
void setup() {
  Serial.begin(9600);
  delay(10);

  strip.begin();
  strip.show();

  // Show all red while connecting..
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(127, 0, 0));
  }
  strip.show();
  
  // Connect to WiFi
  WiFiMulti.addAP("!.!  ~~happy shambs~~");
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Show green when connected.
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 127, 0));
  }
  strip.show();

  delay(2000);

  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  time_counter = 0;
}



// Clip definitions.
int rotate_counter = 0;
void rotate_clip(int time_counter) {
  if (time_counter % state[0] == 0) {
    rotate_counter = (rotate_counter + 1) % PIXELS;

    strip.setPixelColor((rotate_counter + PIXELS - 1) % PIXELS, strip.Color(0, 0, 0));
    strip.setPixelColor(rotate_counter, strip.Color(state[1], state[2], state[3]));
    strip.show();
  }
}

int clip = 0;
void loop() {
  if (clip == 0) {
    rotate_clip(time_counter);
  }

  strip.show();
  time_counter++;
  webSocket.loop();
  delay(5);
}
