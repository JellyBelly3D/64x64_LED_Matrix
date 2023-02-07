#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // tested using v2.0.7
#include <FastLED.h>                         // tested using v3.4.0

#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include "Wappsto.h"
#include "wappsto_config.h"

#define R1_PIN  25
#define G1_PIN  26
#define B1_PIN  27
#define R2_PIN  14
#define G2_PIN  12
#define B2_PIN  13

#define A_PIN   23
#define B_PIN   22
#define C_PIN   5
#define D_PIN   17
#define E_PIN   21  // required for 1/32 scan panels, like 64x64. Any available pin would do, i.e. IO32

#define LAT_PIN 4
#define OE_PIN  15

#define CLK_PIN 16

#define MATRIX_HEIGHT 64 // Number of pixels wide of each INDIVIDUAL panel module.
#define MATRIX_WIDTH  64 // Number of pixels tall of each INDIVIDUAL panel module.

const char ssid[] = "seluxit_guest";
const char password[] = "sunflower4everyone"; // "sunflower4everyone"

MatrixPanel_I2S_DMA *matrix = nullptr;
WiFiMulti WiFiMulti;
WiFiClientSecure client;
Wappsto wappsto(&client);

Network *myNetwork;
Device *myDevice;
Value *textArea1Header;
Value *textArea2;
Value *textArea3;
Value *textArea4Footer;
Value *monochromeBitMap;
Value *brightnessValue;
Value *genericCanvas;

DeviceDescription_t myDeviceDescription = {
    .name = "Display",
    .product = "ESP32 64x64 RBG LED Matrix",
    .manufacturer = "",
    .description = "This is a ESP32 64x64 display, it can output a maximum of 80 signs",
    .version = "1.0",
    .serial = "00001",
    .protocol = "Json-RPC",
    .communication = "WiFi",
};

unsigned int e = 0;

String myString = "";

//Defining canvases for the screen area.
//canvas64x16 has room for a maximum of 20 characters in two rows up to 10 characters each. 
GFXcanvas1 canvas64x16(64,16); 

const uint8_t wifiIcon8x8[] = {
    B11111111 /*ff*/,B00000000 /*0x00*/, B01111110 /*0x7e*/, B00000000 /*0x00*/, B00111100 /*0x3c*/, B00000000 /*0x00*/, 
    B00011000 /*0x18*/,B00011000 /*0x18*/ };

void displayText(String text, int yPos) {
  int16_t  x1, y1;
  uint16_t w, h;
  //matrix.setFont(&FreeSans9pt7b);
  
  matrix->setTextSize(1);
  char charBuf[text.length() + 1];
  text.toCharArray(charBuf, text.length() + 1);
  matrix->getTextBounds(charBuf, 0, yPos, &x1, &y1, &w, &h);
  //int startingX = 33 - (w / 2);
  matrix->setCursor(0, yPos);
  matrix->print(text);
}

ValueString_t valueTextArea1Header = {  .name = "Text Area 1: Header",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};
                                    
ValueString_t valueTextArea2 = {  .name = "Text Area 2",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};

ValueString_t valueTextArea3 = {  .name = "Text Area 3",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};

ValueString_t valueTextArea4Footer = {  .name = "Text Area 4: Footer",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};
    
ValueNumber_t intValueBrightness =  {  .name = "Brightness",
                                    .type = "int",
                                    .permission = READ_WRITE,
                                    .min = 0,
                                    .max = 100,
                                    .step = 1,
                                    .unit = "%",
                                    .si_conversion = ""};

ValueBlob_t genericBitmapValue = { .name = "Bitmap",
                                    .type = "value type",
                                    .permission = WRITE,
                                    .max = 2048,
                                    .encoding = ""};

ValueBlob_t genericCanvasValue = {  .name = "Generic JSON input",
                                    .type = "value type",
                                    .permission = WRITE,
                                    .max = 512,
                                    .encoding = ""};

void controlTextAreasCallback(Value *value, String data, String timestamp)
{
    uint16_t x = 0;
    uint16_t y = 0;

    canvas64x16.fillScreen(0x0000); // clearing the canvas 
    Serial.println(data);
    value->report(data);
    canvas64x16.setTextSize(1);
    canvas64x16.setCursor(0,0);
    canvas64x16.println(data);
    
    if(textArea1Header == value)
    {
        y = 0;
    }

    if(textArea2 == value)
    {
        y = 16;
    }

    if(textArea3 == value)
    {
        y = 32;
    }

    if(textArea4Footer == value)
    {
        y = 48;
    }

    matrix->drawBitmap(x, y, canvas64x16.getBuffer(), 64, 16, 0xffff, 0x0000); //printing canvas out to the screen
}

void controlBitmapCallback(Value *value, String data, String timestamp)
{
    //StaticJsonDocument<2048> root;
    DynamicJsonDocument root(2048);
    DeserializationError err = deserializeJson(root, data);
    if(err)
    {
        Serial.print("Deserialization failed: ");
        Serial.println(err.f_str());
        return;
    }

    int16_t x = root["pos"]["x"];
    int16_t y = root["pos"]["y"];

    int16_t w = root["size"]["w"];
    int16_t h = root["size"]["h"];

    String bColor = root["clr"]["hex"];

    uint16_t bitColorHex = (uint16_t) strtoul(bColor.c_str() + 2, NULL, 16);

    GFXcanvas1 canvas(w, h);
    canvas.fillScreen(0x0000);
    
    String bitMapString = root["bitMap"];

    uint8_t monoBitmap[bitMapString.length()/2]; /* setting the size of "monoBitmap" to half of bitMapString 
                                                    since 2 chars = 1 hex value */

    for (int i = 0; i < bitMapString.length(); i+=2)
    {
        char hexPair[3]; // 3 character array, 2 char for hex value 1 for NULL termination.
        strncpy(hexPair, bitMapString.c_str() + i, 2); // copying two characters from bitMapString to hexPair.
        hexPair[2] = '\0'; // adding NULL termination to hexPair array.

        monoBitmap[i/2] = (uint8_t) strtoul(hexPair, NULL, 16);
        /*Serial.print(i);
        Serial.print(" ");
        Serial.println(monoBitmap[i/2], HEX); */
    }
    
    canvas.drawXBitmap(0,0, monoBitmap, w,h, bitColorHex); // drawing "monoBitmap" to a canvas
    matrix->drawBitmap(x,y, canvas.getBuffer(), w, h, bitColorHex); // drawing canvas on a screen
}

void controlBlobCallback(Value *value, String data, String timestamp)
{
    StaticJsonDocument<512> root;
    DeserializationError err = deserializeJson(root, data);
    if(err)
    {
        Serial.print("Deserialization failed: ");
        Serial.println(err.f_str());
        return;
    }

    int16_t x = root["pos"]["x"];
    int16_t y = root["pos"]["y"];

    int16_t w = root["size"]["w"];
    int16_t h = root["size"]["h"];

    GFXcanvas1 canvas(w, h);
    canvas.fillScreen(0x0000);

    String text = root["txt"]["str"];

    int16_t textSize = root["tSize"]["int"];

    String textColor = root["tClr"]["hex"];
    String bgColor = root["bClr"]["hex"];

    uint16_t textHex = (uint16_t) strtoul(textColor.c_str() + 2, NULL, 16);
    uint16_t bgHex = (uint16_t) strtoul(bgColor.c_str() + 2, NULL, 16);

    canvas.setCursor(0,0);
    canvas.setTextSize(textSize);
    canvas.println(text);
    //canvas.printf(text.c_str());

    Serial.println(x);
    Serial.println(y);
    Serial.println(w);
    Serial.println(h);
    Serial.println(text);
    Serial.println(textHex, HEX);
    Serial.println(bgHex, HEX);
    Serial.println(textSize);
    Serial.println(root.memoryUsage());

    matrix->drawBitmap(x,y, canvas.getBuffer(), canvas.width(), canvas.height(), textHex, bgHex);

}

void controlBrightnessCallback(Value *value, String data, String timestamp)
{
    int brightness = (data.toInt() / 100.0f) * 255;
    matrix->setBrightness8(brightness);
}

void initializeWifi(void)
{
    Serial.println("Initializing WiFi");
    WiFiMulti.addAP(ssid, password);

    matrix->setCursor(0,0);
    matrix->print("Connecting");
    matrix->setCursor(0,8);
    matrix->print(ssid);

    uint16_t notConnected = 0;

    while(WiFiMulti.run() != WL_CONNECTED) 
    {
        Serial.println("WiFi not connected");

        matrix->drawXBitmap(28,28, wifiIcon8x8, 8, 8, 0xe280);
        delay(500);
        matrix->drawXBitmap(28,28, wifiIcon8x8, 8, 8, 0x0000);

        notConnected++;
        if(notConnected > 150)
        {
            Serial.println("Resetting the controller: WiFI not connecting");
            ESP.restart();
        }
    }

    matrix->drawXBitmap(28,28, wifiIcon8x8, 8,8, 0x0460);

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

    HUB75_I2S_CFG::i2s_pins _pins={ R1_PIN, 
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
        MATRIX_WIDTH,           // module width
        MATRIX_HEIGHT,          // module height
        1,                      // Chain length
        _pins,                  // pin mapping
        HUB75_I2S_CFG::FM6126A  // driver chip
    );

    mxconfig.clkphase = false;
    matrix = new MatrixPanel_I2S_DMA(mxconfig);
    matrix->begin();
    matrix->setBrightness8(255);
    initializeWifi();
    initializeNtp();

    wappsto.config(network_uuid, ca, client_crt, client_key, 5, NO_LOGS);
    if(wappsto.connect()) 
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
    
    // Create first string value
    textArea1Header = myDevice->createStringValue(&valueTextArea1Header);
    textArea1Header->onControl(&controlTextAreasCallback);

    // Create second string value
    textArea2 = myDevice->createStringValue(&valueTextArea2);
    textArea2->onControl(&controlTextAreasCallback);

    // Create third string value
    textArea3 = myDevice->createStringValue(&valueTextArea3);
    textArea3->onControl(&controlTextAreasCallback);

    // Create fourth string value
    textArea4Footer = myDevice->createStringValue(&valueTextArea4Footer);
    textArea4Footer->onControl(&controlTextAreasCallback);

    // Create shape value
    monochromeBitMap = myDevice->createBlobValue(&genericBitmapValue);
    monochromeBitMap->onControl(&controlBitmapCallback);

    // Create a Value for brightness control
    brightnessValue = myDevice->createValueNumber(&intValueBrightness);
    brightnessValue->onControl(&controlBrightnessCallback);

    // Create a Value for genericCanvas
    genericCanvas = myDevice->createBlobValue(&genericCanvasValue);
    genericCanvas->onControl(&controlBlobCallback);
    
    // Get last wappsto session values
    myString = textArea1Header->getControlData();
    displayText(myString, 0);
    textArea1Header->report(myString);
    Serial.println(myString);

    myString = textArea2->getControlData();
    displayText(myString, 16); 
    textArea2->report(myString);
    Serial.println(myString);

    myString = textArea3->getControlData();
    displayText(myString, 32);
    textArea3->report(myString);
    Serial.println(myString);

    myString = textArea4Footer->getControlData();
    displayText(myString, 48);
    textArea4Footer->report(myString);
    Serial.println(myString);
}

void loop()
{
    wappsto.dataAvailable();
    //debug led
    if (e == 0) 
    {
        matrix->drawPixel(63, 63, matrix->color565(0, 255, 0));
        e++;
    } 
    else 
    {
        matrix->drawPixel(63, 63, matrix->color565(255, 0, 0));
        e=0;
        //matrix->drawPixel(63,63, matrix->color565(0,0,255));
    }   
}
