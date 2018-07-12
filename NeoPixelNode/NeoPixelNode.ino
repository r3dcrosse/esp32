/*
 * SocketIO_test.ino
 * author: @r3dcrosse
 * Created on: 07.11.2018
 * based off of: https://github.com/Links2004/arduinoWebSockets/blob/master/examples/esp8266/WebSocketClientSocketIO/WebSocketClientSocketIO.ino
 */

#include <Arduino.h>
#include <NeoPixelBus.h>
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
const int NODE_NUMBER = 1;
bool isConnected = false;

WiFiClient client;
WebSocketsClient webSocket;

// LED info
#define colorSaturation 128
const uint16_t PixelCount = 25; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 21;  // make sure to set this to the correct pin, ignored for Esp8266

NeoPixelBus<NeoRgbwFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

// #define MESSAGE_INTERVAL 30000
// #define HEARTBEAT_INTERVAL 25000
//
// uint64_t messageTimestamp = 0;
// uint64_t heartbeatTimestamp = 0;

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
        // set neopixel color for pixel number
        // USE_SERIAL.printf("Setting pixel: %d | %d,%d,%d,%d\n", pixelNumber, R, G, B, W);
        RgbwColor pixelColor = RgbwColor(R, G, B, W);
        strip.SetPixelColor(pixelNumber, pixelColor);
        // reset currentColor back to parse red
        currentColor = 'R';

        // Increment pixel number
        pixelNumber++;
      }
    }
  }
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
            // USE_SERIAL.printf("[WSc] get text: %s\n", payload);
            handleFrame(payload, length);
            // char *ptr = (const char *)&payload[0];

			// send message to server
			// webSocket.sendTXT("message here");
            break;
        case WStype_BIN:
            USE_SERIAL.println("[WSc] get binary length");
            USE_SERIAL.println(length);
//            hexdump(payload, length);

            // send data to server
            // webSocket.sendBIN(payload, length);
            break;
    }

}

void setup() {
    USE_SERIAL.begin(115200);

    // USE_SERIAL.setDebugOutput(true);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    // for (uint8_t t = 4; t > 0; t--) {
    //     USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    //     USE_SERIAL.flush();
    //     delay(1000);
    // }

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

    // Initialize LEDs
    strip.Begin();
}

void loop() {
    webSocket.loop();

    // if(isConnected) {
    //     uint64_t now = millis();
    //
    //     if(now - messageTimestamp > MESSAGE_INTERVAL) {
    //         messageTimestamp = now;
    //         // example socket.io message with type "messageType" and JSON payload
    //         webSocket.sendTXT("42[\"messageType\",{\"greeting\":\"hello\"}]");
    //     }
    //     if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
    //         heartbeatTimestamp = now;
    //         // socket.io heartbeat message
    //         // webSocket.sendTXT("2");
    //     }
    // }

    strip.Show();
}
