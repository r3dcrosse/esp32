#include <WiFi.h>
#include <WebSocketClient.h>

#include "secrets.h";
const char* ssid     = WLAN_SSID;
const char* password = WLAN_PASS;
char path[] = "/echo";
char host[] = "demos.kaazing.com";

WebSocketClient webSocketClient;

// Use WiFiClient class to create TCP connections
WiFiClient client;

void setup() {
  Serial.begin(115200);
  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(5000);


  // Connect to the websocket server
  if (client.connect(host, 80)) {
    Serial.println("Connected");
  } else {
    Serial.println("Connection failed.");
  }

  // Handshake with the server
  webSocketClient.path = path;
  webSocketClient.host = host;
  if (webSocketClient.handshake(client)) {
    Serial.println("Handshake successful");
  } else {
    Serial.println("Handshake failed.");
  }
}


void loop() {
  String data;

  if (client.connected()) {
    webSocketClient.sendData("Info to be echoed back");

    webSocketClient.getData(data);
    if (data.length() > 0) {
      Serial.print("Received data: ");
      Serial.println(data);
    }
  } else {
    Serial.println("Client disconnected.");
  }

  // wait to fully let the client disconnect
  delay(3000);
}
