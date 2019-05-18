/*
 * NeoPixelNode.ino
 * author: @r3dcrosse
 * Created on: 04.18.2019
 * based off of: https://github.com/Links2004/arduinoWebSockets/blob/master/examples/esp8266/WebSocketClientSocketIO/WebSocketClientSocketIO.ino
 */

#include <Arduino.h>
//#include <NeoPixelBus.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include "secrets.h"

#define USE_SERIAL Serial

// Wifi and Websocket info
const uint8_t NODE_NUMBER = 1;
const char* ssid     = WLAN_SSID;
const char* password = WLAN_PASS;
char host[] = "192.168.155.248";
int port = 8000;
char path[] = "/socket.io/?EIO=3";

WiFiClient client;
WebSocketsClient webSocket;

// LED info
const uint16_t PixelCount = 200;
const uint8_t PixelPin = 21;  // make sure to set this to the correct pin, ignored for Esp8266

// FastLED declarations
#define BRIGHTNESS  64
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

#define PING_INTERVAL 1000
#define HEARTBEAT_INTERVAL 1700

uint64_t pingTimestamp = 0;
uint64_t pongTimestamp = 0;
const uint64_t MAX_PONG_TIMEOUT = 6000;
uint64_t pingMeasurement = 0;
uint64_t heartbeatTimestamp = 0;
uint64_t wifiCheckTimestamp = 0;

/**
 * makeColorNumber ---------------------------------------------
 * returns uint8_t that represents the
 * a color object value. This should be 0 - 255
 */
uint8_t makeColorNumber(uint8_t hudred, uint8_t ten, uint8_t one)
{
  return (hudred * 100) + (ten * 10) + (one * 1);
}

/**
 * handleFrame -------------------------------------------------
 * Parses the websocket message and sets each pixel
 * of the LED strip accordingly. Websocket messages come in the
 * format:
 * 42["1:250,255,000,030;000,250,255,000"]
 * node^ R^  G^  B^  W^  R^  G^  B^  W^ ...
 */
void handleFrame(uint8_t * msg, size_t length)
{
  uint8_t pixelNumber = 0;
  uint8_t R, G, B, W = 0;
  uint8_t currentColor = 'R';

  for (int i = 6; i < length - 2; i++)
  {
    if (msg[i] >= '0' && msg[i] <= '9')
    {
      uint8_t hundred = msg[i] - '0';
      uint8_t ten = msg[i + 1] - '0';
      uint8_t one = msg[i + 2] - '0';

      switch (currentColor)
      {
        case 'R':
          R = makeColorNumber(hundred, ten, one);
          break;
        case 'G':
          G = makeColorNumber(hundred, ten, one);
          break;
        case 'B':
          B = makeColorNumber(hundred, ten, one);
          break;
        case 'W':
          W = makeColorNumber(hundred, ten, one);
      }

      // Increment to next token
      i = i + 2;
    } else if (msg[i] - ',' == 0)
    {
      // Handle shifting to next color
      switch (currentColor)
      {
        case 'R':
          currentColor = 'G';
          break;
        case 'G':
          currentColor = 'B';
          break;
        case 'B':
          currentColor = 'W';
          break;
      }
    } else if (msg[i] - ';' == 0)
    {
      // set neopixel color for pixel number
      // if (R != 0 || G != 0 || B != 0 || W != 0) {
      //   USE_SERIAL.printf("Drawing pixel %d:\n", pixelNumber);
      // }
      RgbwColor pixelColor = RgbwColor(R, G, B, W);
      strip.SetPixelColor(pixelNumber, pixelColor);

      // reset currentColor back to parse red
      currentColor = 'R';

      // Increment pixel number
      pixelNumber++;
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type)
  {
    case WStype_DISCONNECTED:
      USE_SERIAL.println("[WSc] Disconnected!");
      break;
    case WStype_CONNECTED:
      {
        USE_SERIAL.printf("[WSc] Connected to url:\n", payload);

        // send message to server when Connected
        // socket.io upgrade confirmation message (required)
        webSocket.sendTXT("5");
      }
      break;
    case WStype_TEXT:
      // You're gonna want to check the payload length to double
      // check what message you're actually expecting
      if (payload[4] - '0' == NODE_NUMBER)
      {
        uint64_t now = millis();
        // Check if we got a ping message from websocket server
        if (payload[5] - 'p' == 0)
        {
          pingMeasurement = now;
          char *pingMessage;
          asprintf(&pingMessage, "42[\"pong\",{\"node\":%i}]", NODE_NUMBER);
          webSocket.sendTXT(pingMessage);
          free(pingMessage);
          pongTimestamp = now;
        }
        else if (payload[5] - 'z' == 0)
        {
          // We got a pong from the server, so let's record the pong timestamp
          pongTimestamp = now;
          // uint64_t latency = now - pingMeasurement;
          //
          // char *latencyMessage;
          // asprintf(&latencyMessage, "42[\"latency\",{\"latency\":%i,\"node\":%i}]", latency, NODE_NUMBER);
          // webSocket.sendTXT(latencyMessage);
          // free(latencyMessage); // release the memory allocated by asprintf.
        }
        else if (payload[5] - 'a' == 0)
        {
          webSocket.sendPing();
        }
        else
        {
          handleFrame(payload, length);
          // USE_SERIAL.println("Got a websocket message and handled frameeee");
          strip.Show();
        }
      }
      break;
    case WStype_BIN:
      // USE_SERIAL.println("[WSc] get binary length");
      // USE_SERIAL.println(length);
      // hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
  }
}

bool didConfigureWebsocket = false;

void handleWiFiConnection() {
  WiFi.begin(ssid, password);

  for (uint8_t count = 0; WiFi.status() != WL_CONNECTED && count < 10; count++)
  {
    delay(500);
    USE_SERIAL.print(WiFi.status());
    USE_SERIAL.print(" ");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    USE_SERIAL.println("COULD NOT CONNECT TO WIFI.... giving up");
    return;
  }

  USE_SERIAL.println();
  USE_SERIAL.println("WiFi connected");
  USE_SERIAL.println("IP address: ");
  USE_SERIAL.println(WiFi.localIP());

  // Handshake with the server
  if (!didConfigureWebsocket)
  {
    webSocket.beginSocketIO(host, port, path);
    //webSocket.setAuthorization("user", "Password"); // HTTP Basic Authorization
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(1337);
    didConfigureWebsocket = true;
  }

  // Initialize pong timestamp with current time
  pongTimestamp = millis();
}

void setup()
{
  USE_SERIAL.begin(115200);
  USE_SERIAL.setDebugOutput(true);

  WiFi.setAutoReconnect(true);
  //handleWiFiConnection();

  // Initialize LEDs
  strip.Begin();
}

int disconnectedCount = 0;
void loop()
{
  webSocket.loop();

  if (WiFi.status() != WL_CONNECTED)
  {
    if (++disconnectedCount == 1000)
    {
      USE_SERIAL.println("WIFI is disconnected...");
      disconnectedCount = 0;
      handleWiFiConnection();
    }
    return;
    // Ideally tell websocket to disconnect here
    // instead of exiting out of the main loop
  }
  disconnectedCount = 0;

  uint64_t now = millis();
  if(now - pingTimestamp > PING_INTERVAL)
  {
      pingTimestamp = now;
      // example socket.io message with type "messageType" and JSON payload
      char *pingMessage;
      asprintf(&pingMessage, "42[\"nodeConnected\",{\"node\":%i}]", NODE_NUMBER);
      webSocket.sendTXT(pingMessage);
      free(pingMessage);
  }
  if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL)
  {
    heartbeatTimestamp = now;
    // socket.io heartbeat message
    webSocket.sendTXT("2");
  }
}
