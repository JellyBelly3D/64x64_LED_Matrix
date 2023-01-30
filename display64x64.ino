#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // tested using v2.0.7
#include <FastLED.h>                         // tested using v3.4.0

#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include "Wappsto.h"
#include "wappsto_config.h"

//#include "ntp.ino"

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
const char password[] = "sunflower4everyone";

MatrixPanel_I2S_DMA *matrix = nullptr;
WiFiMulti WiFiMulti;
WiFiClientSecure client;
Wappsto wappsto(&client);

Network *myNetwork;
Device *myDevice;
//Value *myLedValue;
Value *myEchoStringValueTA1Header;
Value *myEchoStringValueTA2;
Value *myEchoStringValueTA3;
Value *myEchoStringValueTA4Footer;
Value *myEchoStringValueShape;
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

unsigned int    p = 0;
unsigned int    s = 0;
unsigned int    e = 0;
//unsigned int tmps = 0;
int16_t width;
int16_t height;

//int myLed = 0;
String myString = "";
//String myString2 = ""; // still not sure if i need more than 1 since the first one can be overwritten


//Defining canvases for the screen area.
//canvas64x16 has room for a maximum of 20 characters in two rows up to 10 characters each. 
GFXcanvas1 canvas64x16(64,16); 
GFXcanvas1 shape(32,16);

GFXcanvas1 blobShape(64, 64);


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

ValueString_t echoStringValueTA1Header = {  .name = "Text Area 1: Header",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};
                                    
ValueString_t echoStringValueTA2 = {  .name = "Text Area 2",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};

ValueString_t echoStringValueTA3 = {  .name = "Text Area 3",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};

ValueString_t echoStringValueTA4Footer = {  .name = "Text Area 4: Footer",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 20,
                                    .encoding = ""};

ValueString_t echoStringValueShape = {  .name = "Test Shape",
                                    .type = "string",
                                    .permission = READ_WRITE,
                                    .max = 10,
                                    .encoding = ""};
                                    
ValueNumber_t intValueBrightness =  {  .name = "Brightness",
                                    .type = "int",
                                    .permission = READ_WRITE,
                                    .min = 0,
                                    .max = 100,
                                    .step = 1,
                                    .unit = "%",
                                    .si_conversion = ""};

ValueBlob_t genericCanvasValue = {  .name = "Generic JSON input",
                                    .type = "value type",
                                    .permission = WRITE,
                                    .max = 512,
                                    .encoding = ""};

void controlEchoCallback(Value *value, String data, String timestamp)
{
    int x = NULL;
    int y = NULL;

    canvas64x16.fillScreen(0x0000); // clearing the canvas 
    myString = data;
    Serial.println(myString);
    value->report(myString);
    canvas64x16.setTextSize(1);
    canvas64x16.setCursor(0,0);
    canvas64x16.println(myString);
    
    if(myEchoStringValueTA1Header == value)
    {
        x = 0;
        y = 0;
        matrix->drawBitmap(x,y, canvas64x16.getBuffer(), 64, 16, 0xffff, 0x0000); //printing canvas out to the screen
    }

    if(myEchoStringValueTA2 == value)
    {
        x = 0;
        y = 16;
        matrix->drawBitmap(x,y, canvas64x16.getBuffer(), 64, 16, 0xffff, 0x0000); //printing canvas out to the screen
    }

    if(myEchoStringValueTA3 == value)
    {
        x = 0;
        y = 32;
        matrix->drawBitmap(x,y, canvas64x16.getBuffer(), 64, 16, 0xffff, 0x0000); //printing canvas out to the screen
    }

    if(myEchoStringValueTA4Footer == value)
    {
        x = 0;
        y = 48;
        matrix->drawBitmap(x,y, canvas64x16.getBuffer(), 64, 16, 0xffff, 0x0000); //printing canvas out to the screen
    }

    if(myEchoStringValueShape == value)
    {
        x = 32;
        y = 40;

        shape.fillScreen(0x0000); // clearing the canvas 
        myString = data;
        Serial.println(myString);
        value->report(myString);
        shape.setTextSize(1);
        shape.setCursor(0,0);
        shape.println(myString);
        matrix->drawBitmap(x,y, shape.getBuffer(), shape.width(), shape.height(), 0x001F, 0xFFFF);
    }

    
}

void controlBlobCallback(Value *value, String data, String timestamp)
{
    //memset(doc->_readBuffer, 0x00, sizeof(doc->_readBuffer));
    StaticJsonDocument<512> root;
    DeserializationError err = deserializeJson(root, data);
    if(err)
    {
        Serial.print("Deserialization failed: ");
        Serial.println(err.f_str());
        return;
    }

    int16_t x = root["textCanvasPosXY"]["x"];
    int16_t y = root["textCanvasPosXY"]["y"];

    int16_t w = root["textCanvasSizeWH"]["width"];
    int16_t h = root["textCanvasSizeWH"]["height"];

    GFXcanvas1 canvas(w, h);

    String text = root["textContent"]["string"];

    uint16_t textColor = root["textColor16bit565"]["color"];
    uint16_t bgColor = root["backgroundColor16bit565"]["color"];


    canvas.setCursor(0,0);
    canvas.println(text);

    Serial.println(x);
    Serial.println(y);
    Serial.println(w);
    Serial.println(h);
    Serial.println(text);
    Serial.println(textColor);
    Serial.println(bgColor);
    Serial.println(root.memoryUsage());

    matrix->drawBitmap(x,y, canvas.getBuffer(), canvas.width(), canvas.height(), textColor, 0x0000);

    //int memoryUsage = root.memoryUsage();   
}

void controlBrightnessCallback(Value *value, String data, String timestamp)
{
    int userInput = data.toInt();
    Serial.println(userInput);
    int brightness = (userInput / 100.0f) * 255;
    Serial.println(brightness);
    matrix->setBrightness8(brightness);
}

void initializeWifi(void)
{
    Serial.println("Initializing WiFi");
    WiFiMulti.addAP(ssid, password);
    while(WiFiMulti.run() != WL_CONNECTED) 
    {
        Serial.print(".");
        delay(500);
    }
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup()
{
    Serial.begin(115200);
    randomSeed(analogRead(0));

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
        64,                     // module width
        64,                     // module height
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
    
    // Create first echo string value
    myEchoStringValueTA1Header = myDevice->createStringValue(&echoStringValueTA1Header);
    myEchoStringValueTA1Header->onControl(&controlEchoCallback);

    // Create second echo string value
    myEchoStringValueTA2 = myDevice->createStringValue(&echoStringValueTA2);
    myEchoStringValueTA2->onControl(&controlEchoCallback);

    // Create third echo string value
    myEchoStringValueTA3 = myDevice->createStringValue(&echoStringValueTA3);
    myEchoStringValueTA3->onControl(&controlEchoCallback);

    // Create fourth echo string value
    myEchoStringValueTA4Footer = myDevice->createStringValue(&echoStringValueTA4Footer);
    myEchoStringValueTA4Footer->onControl(&controlEchoCallback);

    myEchoStringValueShape = myDevice->createStringValue(&echoStringValueShape);
    myEchoStringValueShape->onControl(&controlEchoCallback);

    brightnessValue = myDevice->createValueNumber(&intValueBrightness);
    brightnessValue->onControl(&controlBrightnessCallback);

    genericCanvas = myDevice->createBlobValue(&genericCanvasValue);
    genericCanvas->onControl(&controlBlobCallback);
    
    // Get the last echo string values
    myString = myEchoStringValueTA1Header->getControlData();
    displayText(myString, 0);
    myEchoStringValueTA1Header->report(myString);
    Serial.println(myString);

    myString = myEchoStringValueTA2->getControlData();
    displayText(myString, 16); 
    myEchoStringValueTA2->report(myString);
    Serial.println(myString);

    myString = myEchoStringValueTA3->getControlData();
    displayText(myString, 32);
    myEchoStringValueTA3->report(myString);
    Serial.println(myString);

    myString = myEchoStringValueTA4Footer->getControlData();
    displayText(myString, 48);
    myEchoStringValueTA4Footer->report(myString);
    Serial.println(myString);
}

void loop()
{
    delay(200);
    
    wappsto.dataAvailable();
    
    //debug led
    if (e == 0) 
    {
        matrix->drawPixel(63, 63, matrix->color565(0, 255, 0));
        e++;
    } 
    else 
    {
        matrix->drawPixel(63, 63, matrix->color565(0, 0, 0));
        e=0;
    }
     
}
