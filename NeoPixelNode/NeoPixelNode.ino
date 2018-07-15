/*
 * NeoPixelNode.ino
 * author: @r3dcrosse
 * Created on: 07.11.2018
 * based off of: https://github.com/Links2004/arduinoWebSockets/blob/master/examples/esp8266/WebSocketClientSocketIO/WebSocketClientSocketIO.ino
 */

#include <Arduino.h>
// #include <NeoPixelBus.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

//#include <Hash.h>
#include "secrets.h"

#define USE_SERIAL Serial

// Wifi and Websocket info
const char* ssid     = WLAN_SSID;
const char* password = WLAN_PASS;
char path[] = "/";
char host[] = "192.168.198.180";
int port = 3000;
int NODE_NUMBER = 1;
bool isConnected = false;

WiFiClient client;
WebSocketsClient webSocket;

// LED info
#define colorSaturation 128
const uint16_t PixelCount = 25; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 21;  // make sure to set this to the correct pin, ignored for Esp8266

// NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PixelCount, PixelPin, NEO_GRBW + NEO_KHZ800);
#define BRIGHTNESS 50
byte neopix_gamma[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

// Multi-core programming
// TaskHandle_t Task1, Task2;
// SemaphoreHandle_t batton;
// long start;

#define PING_INTERVAL 2000
#define HEARTBEAT_INTERVAL 1500

uint64_t pingTimestamp = 0;
uint64_t pingMeasurement = 0;
uint64_t heartbeatTimestamp = 0;

void handleFrame(uint8_t * state, size_t length) {
  int websocketNodeNumber = state[4] - '0';

  if (websocketNodeNumber == NODE_NUMBER) {
    // USE_SERIAL.println("Websocket node number correct");
    uint8_t pixelNumber = 0;
    uint8_t R, G, B, W = 0;
    uint8_t currentColor = 'R';

    // state looks like 42["1:250,255,000;000,250,255"]
    for (int i = 6; i < length - 2; i++) {
      // USE_SERIAL.println(char(state[i]));
      if (state[i] >= '0' && state[i] <= '9') {
        int hundred = state[i] - '0';
        int ten = state[i + 1] - '0';
        int one = state[i + 2] - '0';

        if (currentColor == 'R') {
          R = (hundred * 100) + (ten * 10) + (one * 1);
        } else if (currentColor == 'G') {
          G = (hundred * 100) + (ten * 10) + (one * 1);
        } else if (currentColor == 'B') {
          B = (hundred * 100) + (ten * 10) + (one * 1);
        } else if (currentColor == 'W') {
          W = (hundred * 100) + (ten * 10) + (one * 1);
        }

        // Increment to next token
        i = i + 2;
      } else if (state[i] - ',' == 0) {
        // Handle shifting to next color
        if (currentColor == 'R') {
          currentColor = 'G';
        } else if (currentColor == 'G') {
          currentColor = 'B';
        } else if (currentColor == 'B') {
          currentColor = 'W';
        }
      } else if (state[i] - ';' == 0) {
        // set neopixel color for pixel number (neopixel bus)
        // RgbwColor pixelColor = RgbwColor(R, G, B, W);
        // strip.SetPixelColor(pixelNumber, pixelColor);

        // Adafruit_NeoPixel
        strip.setPixelColor(pixelNumber, strip.Color(R, G, B, W));

        // reset currentColor back to parse red
        currentColor = 'R';

        // Increment pixel number
        pixelNumber++;
      }
    }
  }
}

char* intToStr(int value)
{
    int divisor = 1;
    int bufferLength = 1;
    int isNegative = 0;
    int bufferIndex = 0;

    // Handle the negative value case by remebering that the number is negative
    // and then setting it positive
    if(value < 0)
    {
        isNegative = 1;
        value *= -1;
        bufferIndex++; // move 1 place in the buffer so we don't overwrite the '-'
    }

    // Determine the length of the integer so we can allocate a string buffer
    while(value / divisor >= 10)
    {
        divisor *= 10;
        bufferLength++;
    }

    // Create the resulting char buffer that we'll return.
    // bufferLength + 1 because we need a terminating NULL character.
    // + isNegative because we need space for the negative sign, if necessary.
    char *result = new char[bufferLength + 1 + isNegative];

    // Set the first character to NULL or a negative sign
    result[0] = isNegative == false ? 0 : '-';

    while(bufferLength > 0)
    {
        // ASCII table has the number characters in sequence from 0-9 so use the
        // ASCII value of '0' as the base
        result[bufferIndex] = '0' + value / divisor;

        // This removes the most significant digit converting 1337 to 337 because
        // 1337 % 1000 = 337
        value = value % divisor;

        // Adjust the divisor to next lowest position
        divisor = divisor / 10;

        bufferIndex++;
        bufferLength--;
    }

    // NULL terminate the string
    result[bufferIndex] = 0;

    return result;
}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            USE_SERIAL.println("[WSc] Disconnected!");
            isConnected = false;
            break;
        case WStype_CONNECTED:
            {
                USE_SERIAL.printf("[WSc] Connected to url:\n", payload);
                isConnected = true;

			    // send message to server when Connected
                // socket.io upgrade confirmation message (required)
				        webSocket.sendTXT("5");
            }
            break;
        case WStype_TEXT:
            if (payload[4] - '0' == NODE_NUMBER) {
              uint64_t now = millis();
              // Check if we got a ping message from websocket server
              if (payload[5] - 'p' == 0) {
                pingMeasurement = now;
                // char pingMessage = printf("42[\"ping\",{\"node\":%d}]", NODE_NUMBER);
                webSocket.sendTXT("42[\"pong\",{\"node\":1}]");
              } else if (payload[5] - 'z' == 0) {
                uint64_t latency = now - pingMeasurement;
                // char latencyMessage = "42[\"latency\",{\"latency\":" + intToStr(latency) + "}]";

                char *pingMessage;
                asprintf(&pingMessage, "42[\"latency\",{\"latency\":%i,\"node\":%i}]", latency, NODE_NUMBER);
                webSocket.sendTXT(pingMessage);
                free(pingMessage); // release the memory allocated by asprintf.
              } else {
                handleFrame(payload, length);
                // strip.Show(); // NeoPixelBus
                strip.show(); // Adafruit_NeoPixel
                delay(100);
              }
            }
            // USE_SERIAL.printf("[WSc] get text: %s\n", payload);
            // start = millis();
            // USE_SERIAL.println("USING CORE:");
            // USE_SERIAL.println("handled frame");
            // USE_SERIAL.print("TIME: ");
            // USE_SERIAL.print(millis() - start);
            // USE_SERIAL.println();
            // USE_SERIAL.println();

			// send message to server
			// webSocket.sendTXT("message here");
            break;
        case WStype_BIN:
            // USE_SERIAL.println("[WSc] get binary length");
            // USE_SERIAL.println(length);
//            hexdump(payload, length);

            // send data to server
            // webSocket.sendBIN(payload, length);
            break;
    }

}

// void codeForTask1(void * parameter) {
//   for (;;) {
//     xSemaphoreTake(batton, portMAX_DELAY);
//     strip.Show();
//     xSemaphoreGive(batton);
//     // USE_SERIAL.print("Websocket running on core ");
//     // USE_SERIAL.print(xPortGetCoreID());
//     // USE_SERIAL.println(xPortGetCoreID());
//     // delay(100);
//   }
// }

void setup() {
    USE_SERIAL.begin(115200);



    USE_SERIAL.setDebugOutput(true);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      USE_SERIAL.printf(".");
    }

    USE_SERIAL.println();
    USE_SERIAL.println("WiFi connected");
    USE_SERIAL.println("IP address: ");
    USE_SERIAL.println(WiFi.localIP());

    delay(5000);

    // Connect to socket io server
    if (client.connect(host, port)) {
      USE_SERIAL.println("Connected");
    } else {
      USE_SERIAL.println("Connection failed");
    }

    // Handshake with the server
    webSocket.beginSocketIO(host, port);
    //webSocket.setAuthorization("user", "Password"); // HTTP Basic Authorization
    webSocket.onEvent(webSocketEvent);

    // Initialize LEDs (NeoPixelBus)
    // strip.Begin();

    // Initial LEDs (Adafruit_NeoPixel)
    strip.setBrightness(BRIGHTNESS);
    strip.begin();
    // xTaskCreatePinnedToCore(
    //   codeForTask1, /* Task function */
    //   "Task1",      /* Task name */
    //   10000,         /* Stack size of task */
    //   NULL,         /* Parameter of the task */
    //   1,            /* Task priority */
    //   &Task1,       /* Task handle to track task */
    //   0            /* Core to run task on */
    // );
}

char pingMessage = printf("42[\"ping\",{\"node\":%d}]", NODE_NUMBER);
void loop() {
    // webSocket.loop();
    webSocket.loop();

    if(isConnected) {
        uint64_t now = millis();

        // if(now - pingTimestamp > PING_INTERVAL) {
        //     pingTimestamp = now;
        //     // example socket.io message with type "messageType" and JSON payload
        //     char *pingMessage;
        //     asprintf(&pingMessage, "42[\"ping\",{\"node\":%i}]", NODE_NUMBER);
        //     webSocket.sendTXT(pingMessage);
        //     free(pingMessage);
        //     // webSocket.sendTXT(*pingMessage);
        // }
        if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
            heartbeatTimestamp = now;
            // socket.io heartbeat message
            webSocket.sendTXT("2");
        }
    }
}
