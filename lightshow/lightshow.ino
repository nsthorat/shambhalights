#include <WebSockets.h>
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Adafruit_NeoPixel.h>
#include <limits.h>

#define PIN 14
#define PIXELS 7

#define CLIP_COUNT 1

// WIFI Stuff
ESP8266WiFiMulti WiFiMulti;
WebSocketsServer webSocket = WebSocketsServer(8080);

// Strip stuff
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Current state, contains all possible parameters.
int state[10] = {10, 0, 0, 127, 0, 0, 0, 0, 0};

// Global time counter, increments at each virtual tick, resets when clips change.
// Clips should use this to determine lights to display.
int time_counter = 0, clip = 0;

/*
 * Clip definitions.
 */
struct Param {
  char* name;
  int type;  // 0: slider, 1: button
  int min_val;
  int max_val;
  int init_val;
};
struct Clip {
  char* name;
  void (*fn)(int);
  int param_count;
  Param params[10];
};

// ROTATE SIMPLE
int rotate_counter = 0;
void rotate_simple(int time_counter) {
  if (time_counter % (41 - state[0]) == 0) {
    rotate_counter = (rotate_counter + 1) % PIXELS;

    strip.setPixelColor((rotate_counter + PIXELS - 1) % PIXELS, strip.Color(0, 0, 0));
    strip.setPixelColor(rotate_counter, strip.Color(state[1], state[2], state[3]));
    strip.show();
  }
}

// Sets all the pixels to a specific color.
void setAllPixels(uint16_t color) {
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
}

/*
 * Structure containing clip metadata and function pointer.
 * To add a new clip:
 *   1. Update CLIP_COUNT.
 *   2. Write a function which takes a time_counter and writes pixel values to strip.
 *   3. Add an entry ot the Clips array below, naming your clip, pointing to the function,
 *      and defining the parameters.
 */
Clip clips[CLIP_COUNT] = {
  {
    "Rotate Simple", rotate_simple, 4 /* param_count */, {
        {"Speed", 0 /* slider */, 1 /* min_val */, 40 /* max_val */, 10 /* init_val */},
        {"R", 0, 0, 255, 0},
        {"G", 0, 0, 255, 127},
        {"B", 0, 0, 255, 0}}
  }
};

void loop() {
  clips[clip].fn(time_counter);

  strip.show();
  time_counter = (time_counter + 1) % INT_MAX;
  webSocket.loop();
  delay(5);
}

// Handles websocket events, writes to state object.
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
   switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
        {
            String init_message = String("");
            
            Serial.println("connected... ");

            init_message += "[";
            // Build a JSON array of clips.
            for (int i = 0; i < CLIP_COUNT; i++) {
              init_message += "{";
              init_message += "\"name\":\"" + String(clips[i].name) + "\",";
              init_message += "\"params\":[";

               
              for (int j = 0; j < clips[i].param_count; j++) {
                init_message += "{\"name\":\"" + String(clips[i].params[j].name) + "\",";
                init_message += "\"min_val\":" + String(clips[i].params[j].min_val) + ",";
                init_message += "\"max_val\":" + String(clips[i].params[j].max_val) + ",";
                init_message += "\"init_val\":" + String(clips[i].params[j].init_val) + ",";
                init_message += "\"type\":" + String(clips[i].params[j].type) + "}";

                if (j != clips[i].param_count - 1) {
                  init_message += ",";
                }
              }

              init_message += "]";
              init_message += "}";

              if (i != CLIP_COUNT - 1) {
                init_message += ",";
              }
            }
            init_message += "]";

            webSocket.sendTXT(num, init_message);
        }
            break;
        case WStype_TEXT: 
        {
            Serial.printf("[%u] Got payload: %s\n", num, payload);
            
            // Read each command pair
            int i = 0;
            char* command = strtok((char*) payload, ",");
            while (command != 0) {
              int value = atoi(command);
              if (i == 0) {
                // When we switch clips, reset the time counter and hide all the pixels.
                if (clip != value) {
                  clip = value;
                  time_counter = 0;
                  setAllPixels(strip.Color(0, 0, 0));
                  strip.show();
                }
              } else {
                state[i - 1] = value;
              }
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

void setup() {
  Serial.begin(9600);
  delay(10);

  strip.begin();
  strip.show();

  // Show red while connecting.
  setAllPixels(strip.Color(127, 0, 0));
  strip.show();
  
  // Connect to WiFi.
  WiFiMulti.addAP("!.!  ~~happy shambs~~");
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Show green when connected.
  setAllPixels(strip.Color(0, 127, 0));
  strip.show();
  delay(2000);

  // Turn all the pixels off to begin.
  setAllPixels(strip.Color(0, 0, 0));
  strip.show();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
