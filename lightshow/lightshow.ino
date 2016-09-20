#include <WebSockets.h>
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Adafruit_NeoPixel.h>
#include <limits.h>

#define PIN1 14
#define PIN2 1
#define PIXELS 300
int pixels = PIXELS;

#define CLIP_COUNT 7

// WIFI Stuff
ESP8266WiFiMulti WiFiMulti;
WebSocketsServer webSocket = WebSocketsServer(8080);

// Strip stuff
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS, PIN1, NEO_GRB + NEO_KHZ800);

// Current state, contains all possible parameters.
int state[10] = {10, 0, 0, 127, 0, 0, 0, 0, 0};

// Global time counter, increments at each virtual tick, resets when clips change.
// Clips should use this to determine lights to display.
int time_counter = 0, clip = 0;

// HELPER FUNCTIONS
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

uint32_t HSV_to_RGB(double h, double s, double v) {
  int i;
  double f, p, q, t;
  
  byte r, g, b;
  
  h = max(0.0, min(360.0, h));
  s = max(0.0, min(100.0, s));
  v = max(0.0, min(100.0, v));
  
  s /= 100;
  v /= 100;
  
  if(s == 0) {
    // Achromatic (grey)
    r = g = b = round(v*255);
    return strip.Color(r, g, b);
  }

  h /= 60; // sector 0 to 5
  i = floor(h);
  f = h - i; // factorial part of h
  p = v * (1 - s);
  q = v * (1 - s * f);
  t = v * (1 - s * (1 - f));
  switch(i) {
    case 0:
      r = round(255*v);
      g = round(255*t);
      b = round(255*p);
      break;
    case 1:
      r = round(255*q);
      g = round(255*v);
      b = round(255*p);
      break;
    case 2:
      r = round(255*p);
      g = round(255*v);
      b = round(255*t);
      break;
    case 3:
      r = round(255*p);
      g = round(255*q);
      b = round(255*v);
      break;
    case 4:
      r = round(255*t);
      g = round(255*p);
      b = round(255*v);
      break;
    default: // case 5:
      r = round(255*v);
      g = round(255*p);
      b = round(255*q);
    }

    
    return strip.Color(r, g, b);
}

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
  Param params[12];
};

// Sets all the pixels to a specific color.
void setAllPixels(uint32_t color) {
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
}

// ROTATE SIMPLE
float rotate_counter = 0;
void rotate_simple(int time_counter) {
  float diff = 1 / (41 - (float)state[0]);
  if ((int)(rotate_counter + diff) > (int)rotate_counter) {
    int rc = ((int)rotate_counter + 1) % PIXELS;

    strip.setPixelColor((rc + PIXELS - 1) % PIXELS, strip.Color(0, 0, 0));
    strip.setPixelColor(rc, strip.Color(state[1], state[2], state[3]));
    strip.show();
  }
  rotate_counter += diff;
}

void flat(int time_counter) {
  setAllPixels(strip.Color(state[0], state[1], state[2]));
  strip.show();
}

float sine_counter = 0;
void sine_fade_hue(int time_counter) {
  sine_counter += state[0] / 4000.0;
  float hue = 180.0 + 180.0 * sin(sine_counter);
  uint32_t color = HSV_to_RGB(hue, 100.0, 100.0);

  setAllPixels(color);
  strip.show();
}

float fade_with_hue_hue_counter = 0;
float fade_with_hue_brightness_counter = 0;
void fade_with_hue(int time_counter) {
  fade_with_hue_hue_counter += state[0] / 4000.0;
  fade_with_hue_brightness_counter += state[1] / 400.0;

  float hue = 180.0 + 180.0 * sin(fade_with_hue_hue_counter);

  float brightness = 50.0 + 50.0 * sin(fade_with_hue_brightness_counter);
  uint32_t color = HSV_to_RGB(hue, 100.0, brightness);

  setAllPixels(color);
  strip.show();
}

float gradient_rainbow_counter = 0;
void gradient_rainbow(int time_counter) {
  gradient_rainbow_counter += state[0] / 4000.0;

  for (int i = 0; i < strip.numPixels(); i++) {
    float hue = 180.0 + 180.0 * sin(gradient_rainbow_counter + i / (float)state[1]);
    uint32_t color = HSV_to_RGB(hue, 100.0, 100.0);
    strip.setPixelColor(i, color);
  }

  strip.show();
}

float dot_counter[10] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
float dot_mult[10] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
int8_t dot_colors[10] = {strip.Color(200, 100, 100), strip.Color(100, 160, 200), strip.Color(200, 0, 0), strip.Color(0, 200, 0), strip.Color(0, 0, 200), strip.Color(100, 0, 100), strip.Color(255, 255, 0), strip.Color(0, 255, 255), strip.Color(255, 255, 255), strip.Color(10, 50, 100)};

void bouncing_dots(int time_counter) {
  float dot_diff[10];
  for (int i = 0; i < 10; i++) {
    dot_diff[i] = dot_mult[i] * (float)state[i + 1] / 150.0;

    if ((int)(dot_counter[i] + dot_diff[i]) >= (int)dot_counter[i]) {
      strip.setPixelColor((int)(dot_counter[i] + PIXELS) % PIXELS, strip.Color(0, 0, 0));
      strip.setPixelColor(((int)(dot_counter[i] + dot_diff[i]) + PIXELS) % PIXELS, dot_colors[i]);
    }

    dot_counter[i] += dot_diff[i];
  }
  
  strip.show();
}


double starz_brightness[PIXELS];
int starz_hue[PIXELS];

void starz(int time_counter) {
  if (time_counter % (150 - state[0]) == 0) {
    int ind = (int)(random(PIXELS));

    starz_brightness[ind] = 100.0;
    starz_hue[ind] = (int)(360.0 * (time_counter % 17) / 17.0);
    //(int)random(360);

    Serial.printf("time counter updated... %d => %d", ind, starz_hue[ind]);
  }

  for (int i = 0; i < PIXELS; i++) {
    starz_brightness[i] -= 10.0 / state[1];
    if (starz_brightness[i] <= 0.0000) { starz_brightness[i] = 0.0; }
    //Serial.printf("Updating pixel value %d =>  %d \n", i, starz_hue[i]);
    strip.setPixelColor(i, HSV_to_RGB((double)(starz_hue[i]), 100.0, starz_brightness[i]));
  }
  strip.show();
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
  {"rotate pixel", rotate_simple, 4 /* param_count */, {
        {"Speed", 0 /* slider */, 1 /* min_val */, 40 /* max_val */, 10 /* init_val */},
        {"R", 0, 0, 255, 0},
        {"G", 0, 0, 255, 127},
        {"B", 0, 0, 255, 0}}},
   {"rotate hue", sine_fade_hue, 1 /* param_count */, {
        {"Speed", 0, 1, 200, 10}}},
   {"slow color strobe", fade_with_hue, 2 /* param_count */, {
        {"hue speed", 0, 1, 100, 10},
        {"fade speed", 0, 1, 100, 10}}},
   {"gradient rainbow", gradient_rainbow, 2 /* param_count */, {
        {"speed", 0, 1, 1000, 10},
        {"width", 0, 1, 1000, 10}}},
   {"bouncing dots", bouncing_dots, 11 /* param_count */, {
        {"count", 0, 1, 10, 10},
        {"ball1", 0, 1, 100, 10},
        {"ball2", 0, 1, 100, 10},
        {"ball3", 0, 1, 100, 10},
        {"ball4", 0, 1, 100, 10},
        {"ball5", 0, 1, 100, 10},
        {"ball6", 0, 1, 100, 10},
        {"ball7", 0, 1, 100, 10},
        {"ball8", 0, 1, 100, 10},
        {"ball9", 0, 1, 100, 10},
        {"ball10", 0, 1, 100, 10}}},
   {"starz", starz, 2 /* param_count */, {
        {"density", 0, 1, 150, 148},
        {"lifetime", 0, 1, 100, 4}}},
   {"flat", flat, 3 /* param_count */, {
        {"R", 0, 0, 255, 0},
        {"G", 0, 0, 255, 127},
        {"B", 0, 0, 255, 0}}},
};

void loop() {
  clips[clip].fn(time_counter);

  strip.show();
  time_counter = (time_counter + 1) % INT_MAX;
  webSocket.loop();
  delay(10);
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

            // TODO(nikhil): Convert this to a packed bytestring, text is slow af!
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
  setAllPixels(strip.Color(255, 0, 0));
  strip.show();
  
  // Connect to WiFi.
  WiFiMulti.addAP("!.!  ~~happy shambs~~");
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Show green when connected.
  setAllPixels(strip.Color(0, 255, 0));
  strip.show();
  delay(2000);

  // Turn all the pixels off to begin.
  setAllPixels(strip.Color(0, 0, 0));
  strip.show();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  randomSeed(analogRead(1));

  for (int i = 0; i < PIXELS; i++) {
    starz_brightness[i] = 0.0;
    starz_hue[i] = 0.0;
  }
}
