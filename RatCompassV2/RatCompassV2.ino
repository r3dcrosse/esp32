#include "painlessMesh.h"
#include <Adafruit_LSM303DLH_Mag.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>

/**
 * Analog read from pin A1 does not work when using WiFi
 * https://github.com/espressif/arduino-esp32/issues/102
 *
 * PIN_SLIDE_MODE was originally A1 but after a couple hours
 * of it not working and debugging...
 *
 * A3 seems to work fine though ¯\_(ツ)_/¯
 */
#define PIN_SLIDE_MODE A3
#define PIN_SLIDE_BRIGHTNESS A2
#define PIN_BUTTON A0
#define PIN_LED 27
#define LED_COUNT 31

#define MESH_PREFIX "MyCoolWifiNetworkName"
#define MESH_PASSWORD "someSuperSecurePassword"
#define MESH_PORT 5555

uint16_t value_slide_mode = 0;
uint16_t value_slide_brightness = 0;
boolean talk_button_pressed = false;
boolean has_message = false;
boolean has_new_connection = false;
boolean has_changed_connection = false;

float compass_heading = 0;
float Pi = 3.14159;

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;

Adafruit_LSM303DLH_Mag_Unified mag = Adafruit_LSM303DLH_Mag_Unified(12345);
Adafruit_DotStar strip(LED_COUNT, DOTSTAR_BGR);

// User stub
// Callback methods prototypes
void sendMessage();
void onReadBrightnessPin();
void onReadModePin();
void onReadCompass();
void onReadButton();
void onMessage();
void requestAnimationFrame();
void onChangeConnection();
void onNewConnection();
void requestOverlayFrame();

// Tasks
Task taskSendMessage(TASK_MILLISECOND * 100, TASK_FOREVER, &sendMessage);
Task taskReadMode(TASK_MILLISECOND * 500, TASK_FOREVER, &onReadModePin);
Task taskReadBrightness(TASK_MILLISECOND * 100, TASK_FOREVER, &onReadBrightnessPin);
Task taskReadCompass(TASK_MILLISECOND * 500, TASK_FOREVER, &onReadCompass);
Task taskReadButton(TASK_MILLISECOND * 1, TASK_FOREVER, &onReadButton);

Task listenForMessage(TASK_MILLISECOND * 1, TASK_FOREVER, &onMessage);

Task taskChangeConnection(TASK_MILLISECOND * 100, TASK_FOREVER, &onChangeConnection);
Task taskNewConnection(TASK_MILLISECOND * 100, TASK_FOREVER, &onNewConnection);

Task animateLights(TASK_MILLISECOND * 1, TASK_FOREVER, &requestAnimationFrame);
Task animateOverlay(TASK_MILLISECOND * 20, TASK_FOREVER, &requestOverlayFrame);

void setup()
{
  Serial.begin(115200);

  // Initialize the mag/compass sensor
  mag.begin();

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  pinMode(PIN_SLIDE_BRIGHTNESS, INPUT);
  pinMode(PIN_SLIDE_MODE, INPUT);

  userScheduler.addTask(taskSendMessage);
  userScheduler.addTask(taskReadMode);
  userScheduler.addTask(taskReadBrightness);
  userScheduler.addTask(taskReadCompass);
  userScheduler.addTask(taskReadButton);
  userScheduler.addTask(listenForMessage);

  userScheduler.addTask(animateLights);
  userScheduler.addTask(taskChangeConnection);
  userScheduler.addTask(taskNewConnection);
  userScheduler.addTask(animateOverlay);

  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP

  delay(1000);

  taskSendMessage.enable();
  taskReadMode.enable();
  taskReadBrightness.enable();
  taskReadCompass.enable();
  taskReadButton.enable();
  listenForMessage.enable();
  animateLights.enable();
  taskChangeConnection.enable();
  taskNewConnection.enable();
}

void loop()
{
  mesh.update();
}

/////////////////////////////////////////////////////////////////////////////////////
// Painless Mesh Code
/////////////////////////////////////////////////////////////////////////////////////
unsigned long lastTalked = 0;
void sendMessage()
{
  if (talk_button_pressed)
  {
    unsigned long currTime = millis();

    if ((currTime - lastTalked) > 350)
    {
      String msg = "Hi love!";
      mesh.sendBroadcast(msg);
      lastTalked = currTime;
    }
  }

  // taskSendMessage.setInterval(random(TASK_SECOND * 1, TASK_SECOND * 5));
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());

  has_message = true;
}

void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
  has_new_connection = true;
}

void changedConnectionCallback()
{
  Serial.printf("Changed connections\n");
  has_changed_connection = true;
}

void nodeTimeAdjustedCallback(int32_t offset)
{
  // Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

/////////////////////////////////////////////////////////////////////////////////////
// Brightness Slider Code
/////////////////////////////////////////////////////////////////////////////////////
void onReadBrightnessPin()
{
  uint16_t val = analogRead(PIN_SLIDE_BRIGHTNESS);
  value_slide_brightness = val;

  strip.setBrightness(value_slide_brightness / 16);

  // Serial.print("BRIGHTNESS Slider value: ");
  // Serial.println(value_slide_brightness);
}

/////////////////////////////////////////////////////////////////////////////////////
// Mode Slider Code
/////////////////////////////////////////////////////////////////////////////////////
void onReadModePin()
{
  uint16_t val = analogRead(PIN_SLIDE_MODE);
  // Serial.print("MODE Slider value: ");
  value_slide_mode = val;
  // Serial.println(value_slide_mode);
}

///////////////////////////////////////////////////////////////////////////////////
// Compass Code
///////////////////////////////////////////////////////////////////////////////////
void onReadCompass()
{
  /* Get a new sensor event */
  sensors_event_t event;
  mag.getEvent(&event);

  // Calculate the angle of the vector y,x
  compass_heading = (atan2(event.magnetic.y, event.magnetic.x) * 180) / Pi;

  // Normalize to 0-360
  if (compass_heading < 0)
  {
    compass_heading = 360 + compass_heading;
  }
  // Serial.print("Compass Heading: ");
  // Serial.println(compass_heading);
}

/////////////////////////////////////////////////////////////////////////////////////
// Talk button Code
/////////////////////////////////////////////////////////////////////////////////////
void onReadButton()
{
  if (digitalRead(PIN_BUTTON) == LOW)
  {
    talk_button_pressed = true;
  }
  else
  {
    talk_button_pressed = false;
  }
}

/////////////////////////////////////////////////////////////////////////////////////
// Colors
/////////////////////////////////////////////////////////////////////////////////////
uint32_t magenta = strip.Color(255, 0, 255);
uint32_t red = strip.Color(255, 0, 0);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t cyan = strip.Color(0, 255, 255);
uint32_t black = strip.Color(0, 0, 0);
uint32_t red2 = strip.Color(255, 2, 20);

/////////////////////////////////////////////////////////////////////////////////////
// LED State
/////////////////////////////////////////////////////////////////////////////////////
bool isRunningOverlay = false;

/////////////////////////////////////////////////////////////////////////////////////
// LED Show Runner
/////////////////////////////////////////////////////////////////////////////////////
void requestAnimationFrame()
{
  if (isRunningOverlay)
  {
    return;
  }

  // if (has_new_connection)
  // {
  //   flash(red, green);
  //   // return;
  // }

  if (value_slide_mode < 800)
  {
    stripTest();
  }
  else if (value_slide_mode >= 2600)
  {
    compassShow();
  }
}

void requestOverlayFrame()
{
  if (has_message)
  {
    flash(cyan, magenta, has_message);
  }

  if (has_changed_connection)
  {
    flash(red2, yellow, has_changed_connection);
  }
}

/////////////////////////////////////////////////////////////////////////////////////
// LED Shows
/////////////////////////////////////////////////////////////////////////////////////
void compassShow()
{
  uint32_t color = black;

  if (compass_heading >= 0 && compass_heading < 90)
  {
    color = cyan;
  }
  else if (compass_heading >= 90 && compass_heading < 180)
  {
    color = magenta;
  }
  else if (compass_heading >= 180 && compass_heading < 270)
  {
    color = red2;
  }
  else if (compass_heading >= 270 && compass_heading <= 360)
  {
    color = yellow;
  }

  for (int i = LED_COUNT - 1; i >= 0; i--)
  {
    strip.setPixelColor(i, color);
    strip.show();
    // delay(10);
    animateLights.delay(50);
  }
}

void flash(uint32_t color1, uint32_t color2, bool &overlayFlag)
{
  strip.setBrightness(255); // Make it full brightness

  uint32_t swap = color2;

  for (int i = 0; i < 5; i++)
  {
    // Fill odd pixels
    for (int i = 0; i < LED_COUNT; i += 2)
    {
      strip.setPixelColor(i, color1);
    }

    // Fill even pixels
    for (int i = 1; i <= LED_COUNT; i += 2)
    {
      strip.setPixelColor(i, color2);
    }

    strip.show();
    delay(20);
    // animateOverlay.delay(20);

    strip.fill(black, 0, LED_COUNT);
    delay(20);
    // animateOverlay.delay(20);

    if (i == 4)
    {
      animateOverlay.disable();
      animateLights.enable();
      overlayFlag = false;
    }

    swap = color1;
    color1 = color2;
    color2 = swap;
  }

  strip.setBrightness(value_slide_brightness);
}

// Runs 10 LEDs at a time along strip, cycling through red, green and blue.
// This requires about 200 mA for all the 'on' pixels + 1 mA per 'off' pixel.
int tail = 0, head = -10;  // Index of first 'on' and 'off' pixels
uint32_t color = 0xFF0000; // 'On' color (starts red)

void stripTest()
{
  strip.setPixelColor(head, color); // 'On' pixel at head
  strip.setPixelColor(tail, 0);     // 'Off' pixel at tail
  strip.show();                     // Refresh strip
  // delay(20);                        // Pause 20 milliseconds (~50 FPS)
  animateLights.delay(80);

  if (++head >= LED_COUNT)
  {                         // Increment head index.  Off end of strip?
    head = 0;               //  Yes, reset head index to start
    if ((color >>= 8) == 0) //  Next color (R->G->B) ... past blue now?
      color = 0xFF0000;     //   Yes, reset to red
  }
  if (++tail >= LED_COUNT)
    tail = 0; // Increment, reset tail index
}

/////////////////////////////////////////////////////////////////////////////////////
// Wireless Functions
/////////////////////////////////////////////////////////////////////////////////////
void onMessage()
{
  if (has_message)
  {
    animateLights.disable();
    animateOverlay.enable();
  }
}

void onChangeConnection()
{
  if (has_changed_connection)
  {
    animateLights.disable();
    animateOverlay.enable();
  }
}
void onNewConnection()
{
  if (has_new_connection)
  {
    // flash(red, green);
    has_new_connection = false;
  }
}