#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // tested using v2.0.7 versions 3.0.5 to 3.0.8 unstable
#include <FastLED.h>                         // tested using v3.5.0

#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <HTTPClient.h>

#include "Wappsto.h"
#include "wappsto_config.h"

#include "display_defines.h"

const char ssid[] = "seluxit_guest";
const char password[] = "sunflower4everyone";

MatrixPanel_I2S_DMA *matrix = nullptr;
WiFiMulti WiFiMulti;
WiFiClientSecure client;
Wappsto wappsto(&client);

HTTPClient https;

Network *myNetwork;
Device *myDevice;
Value *colorBitmap;
Value *monoBitmap;
Value *brightness;
Value *text;

DeviceDescription_t myDeviceDescription = {
    .name = "Display",
    .product = "ESP32 64x64 RGB LED Matrix",
    .manufacturer = "",
    .description = "",
    .version = "1.0",
    .serial = "00001",
    .protocol = "Json-RPC",
    .communication = "WiFi",
};

unsigned int e = 0;

const uint8_t wifiIcon8x8[] = {
    B11111111 /*ff*/, B00000000 /*0x00*/, B01111110 /*0x7e*/, B00000000 /*0x00*/, B00111100 /*0x3c*/, B00000000 /*0x00*/,
    B00011000 /*0x18*/, B00011000 /*0x18*/};

static uint8_t maskBuffer[(MATRIX_HEIGHT * MATRIX_WIDTH) / 8];      // the maximum possible size needed for a monochrome (w*h)/8 bitmap
static uint8_t colorBitmapBuffer[MATRIX_HEIGHT * MATRIX_WIDTH * 2]; // the maximum possible size needed for a RGB565 w*h*2 bitmap

ValueNumber_t brightnessValue = {.name = "Brightness",
                                 .type = "int",
                                 .permission = READ_WRITE,
                                 .min = 0,
                                 .max = 100,
                                 .step = 1,
                                 .unit = "%",
                                 .si_conversion = ""};

ValueBlob_t monoBitmapValue = {.name = "Mono Bitmap",
                               .type = "value type",
                               .permission = READ_WRITE,
                               .max = 2048,
                               .encoding = ""};

ValueBlob_t colorBitmapValue = {.name = "RGB565 Bitmap",
                                .type = "value type",
                                .permission = READ_WRITE,
                                .max = 4092,
                                .encoding = ""};

ValueBlob_t textValue = {.name = "Text input",
                         .type = "value type",
                         .permission = READ_WRITE,
                         .max = 512,
                         .encoding = ""};

void drawCanvas16(int16_t x, int16_t y, int16_t w, int16_t h)
{
  uint16_t *bitmap16 = (uint16_t *)colorBitmapBuffer; /*casting the array type from 8bit array to 16bit,
                                                        since drawRGBBitmap takes 16bit bitmap*/
  GFXcanvas16 canvas(w, h);
  canvas.fillScreen(0x0000); // clearing canvas area

  memset(maskBuffer, 0xff, (MATRIX_HEIGHT * MATRIX_WIDTH) / 8); // filling out maskBuffer with white color

  canvas.drawRGBBitmap(0, 0, bitmap16, maskBuffer, w, h);                           // filling the canvas out with rgbBitmap and maskBuffer
  matrix->drawRGBBitmap(x, y, canvas.getBuffer(), canvas.width(), canvas.height()); // outputting canvas to a screen
}

void drawCanvas1(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t *monoBitmap, uint16_t bitColorHex)
{
  GFXcanvas1 canvas(w, h);
  canvas.fillScreen(0x0000);

  canvas.drawXBitmap(0, 0, monoBitmap, w, h, bitColorHex);                                    // drawing "monoBitmap" to a canvas
  matrix->drawBitmap(x, y, canvas.getBuffer(), canvas.width(), canvas.height(), bitColorHex); // drawing canvas on a screen
}

void drawCanvasText(int16_t x, int16_t y, int16_t w, int16_t h, String text, int16_t textSize, uint16_t textHex, uint16_t bgHex, bool drawBG)
{
  GFXcanvas1 canvas(w, h);
  canvas.setCursor(0, 0);
  canvas.setTextSize(textSize);
  canvas.println(text);

  if(drawBG)
  {
    matrix->drawBitmap(x, y, canvas.getBuffer(), canvas.width(), canvas.height(), textHex, bgHex);
  }
  else
  {    
    matrix->drawBitmap(x, y, canvas.getBuffer(), canvas.width(), canvas.height(), textHex);
  }
}

bool keyValidate(Value *value, DynamicJsonDocument *root, const char *keys[], uint8_t keyAmount)
{
  for (int i = 0; i < keyAmount; i++)
  {
    char *key = (char *)keys[i];
    if (!root->containsKey(key))
    {
      value->report("JSON missing " + (String)key);
      return false;
    }
  }
  return true;
}

void errorMessageReport(Value *value, String error)
{
  Serial.println(error);
  value->report(error);
}

void controlColorBitmapCallback(Value *value, String data, String timestamp)
{
  DynamicJsonDocument root(1024);
  DeserializationError err = deserializeJson(root, data);
  if (err)
  {
    errorMessageReport(value, "JSON deserialization failed: " + (String)err.f_str());
    return;
  }

  const char *keys[5] = {"x", "y", "w", "h", "bitmap"};

  if (!(keyValidate(value, &root, keys, 5)))
  {
    return;
  }

  int16_t x = root["x"];
  int16_t y = root["y"];

  int16_t w = root["w"];
  int16_t h = root["h"];

  String url = root["bitmap"];
  Serial.println(url);

  WiFiClientSecure *insecure = new WiFiClientSecure;
  if (!insecure)
  {
    https.end();
    errorMessageReport(value, "Could not create new WiFi client");
    return;
  }

  insecure->setInsecure();

  https.begin(*insecure, url);
  int httpCode = https.GET(); // HTTPClient can output negative error codes, check "HTTPClient.h"
  if (httpCode != HTTP_CODE_OK)
  {
    https.end();
    delete insecure;
    errorMessageReport(value, (String)httpCode);
    return;
  }

  int contentLength = https.getSize();                    // length of 'Content-Length' HTTP header
  if (contentLength > (MATRIX_HEIGHT * MATRIX_WIDTH * 2)) // return if contentLength is bigger than max matrix dimensions*2 in size
  {
    https.end();
    delete insecure;
    errorMessageReport(value,
                       "Image size of " + String(contentLength)
                       + " bytes exeeds max posible size for matrix dimensions " 
                       + String(MATRIX_WIDTH) + " * " 
                       + String(MATRIX_HEIGHT) + " * " 
                       + "2 bytes.");
    return;
  }

  if (contentLength != w * h * 2) // return if contentLength does not match width*heigth*2 bytes
  {
    https.end();
    delete insecure;
    errorMessageReport(value,
                       "Image does not meet "
                        + String(w) + " * "
                        + String(h) + " * " 
                        + "2 byte size specification.");

    return;
  }

  uint8_t buffer[128] = {0};                     // buffer for reading 'n' bytes at the time
  WiFiClient *dataStream = https.getStreamPtr(); // pointer to tcp stream
  int index = 0;                                 // start address for the colorBitmapBuffer array, with every loop it shifts address pointer

  while (https.connected() && (contentLength > 0 || contentLength == -1)) // continue for as long as there is a http connection and content
  {
    size_t size = dataStream->available();
    if (size)
    {
      int data = dataStream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
      memcpy(colorBitmapBuffer + index, buffer, data); // copying buffer in to bitmap array and adjusting for position in which new data should be written
      index += data;                                   // shifting the start position in array by the size of 'data'

      if (contentLength > 0)
      {
        contentLength -= data; // updating the content length with the amoutn of bits written
      }
    }
  }

  https.end();
  delete insecure;

  drawCanvas16(x, y, w, h);
  value->report("Success");
}

void controlMonoBitmapCallback(Value *value, String data, String timestamp)
{
  DynamicJsonDocument root(2048);
  DeserializationError err = deserializeJson(root, data);
  if (err)
  {
    errorMessageReport(value, "JSON deserialization failed: " + (String)err.f_str());
    return;
  }

  const char *keys[6] = {"x", "y", "w", "h", "color", "bitmap"};

  if (!(keyValidate(value, &root, keys, 6)))
  {
    return;
  }

  int16_t x = root["x"];
  int16_t y = root["y"];

  int16_t w = root["w"];
  int16_t h = root["h"];

  String color = root["color"];

  uint16_t bitColorHex = (uint16_t)strtoul(color.c_str() + 2, NULL, 16); // converting string input to hex

  String bitMapString = root["bitmap"];

  if (bitMapString.length() > (MATRIX_HEIGHT * MATRIX_WIDTH) / 4)
  {
    errorMessageReport(value, "Bitmap size of " + String(bitMapString.length()) 
                       + " bytes exeeds max posible size for matrix dimensions " 
                       + String(MATRIX_WIDTH) + " * "
                       + String(MATRIX_HEIGHT) + " / " 
                       + "4 bytes.");
    return;
  }

  if (bitMapString.length() != (w * h) / 4)
  {
    errorMessageReport(value, "Bitmap size of " + String(bitMapString.length()) 
                       + " bytes does not meet " 
                       + String(w) + " * " 
                       + String(h) + " / " 
                       + "4 byte size specification.");
    return;
  }

  uint8_t monoBitmap[bitMapString.length() / 2]; /* setting the size of "monoBitmap" to half of bitMapString
                                                    since 2 chars = 1 hex value */

  for (int i = 0; i < bitMapString.length(); i += 2)
  {
    char hexPair[3];                               // 3 character array, 2 char for hex value 1 for NULL termination.
    strncpy(hexPair, bitMapString.c_str() + i, 2); // copying two characters from bitMapString to hexPair.
    hexPair[2] = '\0';                             // adding NULL termination to hexPair array.

    monoBitmap[i / 2] = (uint8_t)strtoul(hexPair, NULL, 16);
  }
  drawCanvas1(x, y, w, h, monoBitmap, bitColorHex);
  value->report("Success");
}

void controlTextCallback(Value *value, String data, String timestamp)
{
  DynamicJsonDocument root(512);
  DeserializationError err = deserializeJson(root, data);
  if (err)
  {
    errorMessageReport(value, "JSON deserialization failed: " + (String)err.f_str());
    return;
  }

  const char *keys[8] = {"x", "y", "w", "h", "text", "tColor", "bColor", "drawBG"}; 

  if (!(keyValidate(value, &root, keys, 8)))
  {
    return;
  }

  int16_t x = root["x"];
  int16_t y = root["y"];

  int16_t w = root["w"];
  int16_t h = root["h"];

  String text = root["text"];

  int16_t textSize = root["tSize"];

  String textColor = root["tColor"];
  String bgColor = root["bColor"];

  bool drawBG = root["drawBG"].as<bool>();

  uint16_t textHex = (uint16_t)strtoul(textColor.c_str() + 2, NULL, 16);
  uint16_t bgHex = (uint16_t)strtoul(bgColor.c_str() + 2, NULL, 16);

  drawCanvasText(x, y, w, h, text, textSize, textHex, bgHex, drawBG);
  value->report("Success");
}

void controlBrightnessCallback(Value *value, String data, String timestamp)
{
  int strength = (data.toInt() / 100.0f) * 255;
  matrix->setBrightness8(strength);
  int receivedValue = data.toInt();
  brightness->report(receivedValue);
}

void initializeWifi(void)
{
  Serial.println("Initializing WiFi");
  WiFiMulti.addAP(ssid, password);

  matrix->setCursor(0, 0);
  matrix->print("Connecting");
  matrix->setCursor(0, 8);
  matrix->print(ssid);

  uint16_t notConnected = 0;

  while (WiFiMulti.run() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected");

    matrix->drawXBitmap(28, 28, wifiIcon8x8, 8, 8, 0xe280);
    delay(500);
    matrix->drawXBitmap(28, 28, wifiIcon8x8, 8, 8, 0x0000);

    /*notConnected++;
    if (notConnected > 150)
    {
      Serial.println("Resetting the controller: WiFI not connecting");
      ESP.restart();
    }*/
  }

  matrix->drawXBitmap(28, 28, wifiIcon8x8, 8, 8, 0x0460);

  matrix->setCursor(0, 37);
  matrix->print("Connected");

  matrix->setCursor(0, 44);
  matrix->println(WiFi.localIP());

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);

  HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN,
                                   G1_PIN,
                                   B1_PIN,
                                   R2_PIN,
                                   G2_PIN,
                                   B2_PIN,
                                   A_PIN,
                                   B_PIN,
                                   C_PIN,
                                   D_PIN,
                                   E_PIN,
                                   LAT_PIN,
                                   OE_PIN,
                                   CLK_PIN};
  HUB75_I2S_CFG mxconfig(
      MATRIX_WIDTH,          // module width
      MATRIX_HEIGHT,         // module height
      CHAIN_LENGTH,          // Chain length
      _pins,                 // pin mapping
      HUB75_I2S_CFG::FM6126A // driver chip
  );

  mxconfig.clkphase = false;
  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  initializeWifi();
  initializeNtp();

  wappsto.config(network_uuid, ca, client_crt, client_key, 5, NO_LOGS);
  if (wappsto.connect())
  {
    Serial.println("Connected to Wappsto");
  }
  else
  {
    Serial.println("Could not connect");
  }

  // Create network
  myNetwork = wappsto.createNetwork("Display 64x64");

  // Create device
  myDevice = myNetwork->createDevice(&myDeviceDescription);

  // Create Bitmap value
  monoBitmap = myDevice->createBlobValue(&monoBitmapValue);
  monoBitmap->onControl(&controlMonoBitmapCallback);

  // Create RGB565 Bitmap value
  colorBitmap = myDevice->createBlobValue(&colorBitmapValue);
  colorBitmap->onControl(&controlColorBitmapCallback);

  // Create a Value for brightness control
  brightness = myDevice->createValueNumber(&brightnessValue);
  brightness->onControl(&controlBrightnessCallback);

  // Create a Value for genericCanvas
  text = myDevice->createBlobValue(&textValue);
  text->onControl(&controlTextCallback);

  // Set latest brightness value from WappsTo
  String lastBrightness = brightness->getControlData();
  matrix->setBrightness8((uint8_t)lastBrightness.toInt());

  Serial.println("Setup complete");
}

void loop()
{
  wappsto.dataAvailable();
  // debug led
  if (e == 0)
  {
    matrix->drawPixel(63, 63, matrix->color565(0, 255, 0));
    e++;
  }
  else
  {
    matrix->drawPixel(63, 63, matrix->color565(255, 0, 0));
    e = 0;
  }
}