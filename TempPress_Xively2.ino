//****************************************************************
//  Sensor Example to Read BMP Temp/Pressure Sensor and DHT22
//  Humidity sensor and report to Xively.com the results.
//  Also grabs the Sea Level Pressure from KIAD Metar
//  To correct the Altitude reported from the BMP Library
//
//  Requires: Arduino with Yun Shield or Arduino Yun board,
//    BMP185 Sensor
//    DHT22 Sensor
//    Xively Account with API Key and Feed URI
//
//                       8/2015 MJ Wyrick
//****************************************************************

// Include for F() macro for our Strings
#include <avr/pgmspace.h>

// Pins
#define DHPIN 4
#define LEDPIN 6

// Yun
#include <Process.h>
#include <FileIO.h>

// Sensors
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <DHT.h>

// OLED
// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
SSD1306AsciiAvrI2c oled;

//NeoPixels
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip = Adafruit_NeoPixel(150, LEDPIN, NEO_GRB + NEO_KHZ800);

// Temperature Sensor
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// Hold SeaLevelPressure from Metar
float SLPress = 0;
int SLPFoundFlag = 0;

// Humidity Sensor
DHT dht(DHPIN, DHT22);

#define SCRIPTNAME "/tmp/onData.sh"
#define SLPSCRIPTNAME "/tmp/getSLP.sh"

#define DATAUPDATETIME 60
#define SLPUPDATETIME  600
#define WRAPTIME       600

#define FONTBIG   X11fixed7x14
#define FONTSMALL Adafruit5x7
//#define FONTSMALL Wendy3x5

//****************************************************************
// Create a script to download the Current SeaLevelPressure for KIAD
// Delays added to help the bridge not mess up
//
// NOTE: Hard Coded APIKey and xively feed URI
//****************************************************************
void uploadScript() {
  Console.println(F("Creating Scripts."));
  
  File script = FileSystem.open(SLPSCRIPTNAME, FILE_WRITE);
  script.print(F("#!/bin/sh\n"));
  script.print(F("curl -slient \"http://www.aviationweather.gov/adds/tafs/?station_ids=KIAD&submit_both=Get+TAFs+and+METARs\" | sed -n 's/.*\\(SLP\\)\\([0-9][0-9]\\)\\([0-9]\\).*/10\\2.\\3/p'\n"));
  script.close();  // close the file
  delay(500); 
  Process chmod;
  chmod.begin(F("chmod"));      // chmod: change mode
  chmod.addParameter("+x");  // x stays for executable
  chmod.addParameter(SLPSCRIPTNAME);  // path to the file to make it executable
  chmod.run();
  delay(500); 

  File script2 = FileSystem.open(SCRIPTNAME, FILE_WRITE);
  script2.print(F("#!/bin/sh\n"));
  script2.print(F("echo $1','$2','$3','$4','$5','$6','$7 > /tmp/SensorData.txt\n"));
  script2.print(F("curl -k --request PUT -d $\'Temperature, \'$1$\'\\nPressure, \'$2$\'\\nAltitude, \'$3$\'\\nSeaLevelPressure, \'$4$\'\\nSLPFound, \'$5$\'\\nDHTTemp, \'$6$\'\\nHumidity, \'$7 "));
  script2.print(F("-H \'X-ApiKey: aU9GRi4zRRY9ERCcopzuh9cImS5OXEs7MkrIv9DgFTLM3EOd\' https://api.xively.com/v2/feeds/608708105.csv\n"));
  script2.close();  // close the file
  delay(500); 
  chmod.begin(F("chmod"));         // chmod: change mode
  chmod.addParameter("+x");        // x stays for executable
  chmod.addParameter(SCRIPTNAME);  // path to the file to make it executable
  chmod.run();
  delay(500); 

  Console.println(F("Done Creating Scripts."));
}

/**************************************************************************/
/*
    Arduino setup function (automatically called at startup)
*/
/**************************************************************************/
void setup(void) 
{
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(FONTBIG);
  oled.clear();
  oled.print("MJW Xively Demo");

  // Setup the Yun Bridge, Console, and FileIO
  Bridge.begin();
  Console.begin();
  FileSystem.begin();

  //while(!Console);
  Console.println(F("Welcome to BMP185, DHT22 and Xively Demo from MJW")); 

  if(!bmp.begin())
  {
    /* There was a problem detecting the BMP085 ... check your connections */
    Console.print(F("no BMP185 detected ... Check your wiring or I2C ADDR!"));
    while(1);
  } 

  // DHT Sensor
  dht.begin();

  uploadScript();
  GetAndSetSLP();
  if (SLPFoundFlag) {
    Green();
    delay(1000);
  } else {
    Red();
    delay(1000);
  }
}

/**************************************************************************/
// Convert from HSV to RGB for the Led Colors
/**************************************************************************/
void HSVtoRGB(int hue, int sat, int val,int *colors) {
  int r, g, b, base;                                                 

  if (sat == 0) {                            
    colors[0]=val;
    colors[1]=val;
    colors[2]=val;
  } else  {
    base = ((255 - sat) * val)>>8;
    switch(hue/60) {
      case 0:
        colors[0] = val;
        colors[1] = (((val-base)*hue)/60)+base;
        colors[2] = base;
        break;
      case 1:
        colors[0] = (((val-base)*(60-(hue%60)))/60)+base;
        colors[1] = val;
        colors[2] = base;
        break;
      case 2:
        colors[0] = base;
        colors[1] = val;
        colors[2] = (((val-base)*(hue%60))/60)+base;
        break;
      case 3:
        colors[0] = base;
        colors[1] = (((val-base)*(60-(hue%60)))/60)+base;
        colors[2] = val;
        break;
      case 4:
        colors[0] = (((val-base)*(hue%60))/60)+base;
        colors[1] = base;
        colors[2] = val;
        break;
      case 5:
        colors[0] = val;
        colors[1] = base;
        colors[2] = (((val-base)*(60-(hue%60)))/60)+base;
        break;
    }
    
  }
}

/**************************************************************************/
//  Fill in the Led colors for a nice display
/**************************************************************************/
void ColorIt() {
  for (int x=0; x < 150; x++) {
//   strip.setPixelColor(x, random(255),random(255),random(255));
    int color[3];
//    int ran = random(0,150) - 30;
//    if (ran < 0) ran += 360;
/*
    int r = random(0,3);
    switch (r) {
      case 0:
        strip.setPixelColor(x, 255, 0, 0);    
        break;
      case 1:
        strip.setPixelColor(x, 0, 255, 0);          
        break;
      case 2:
        strip.setPixelColor(x, 0, 0, 255);          
        break;
    }
*/
    int hue = random(0, 360);    
    int sat = random(64,256);
    int val = random(128,256);
    HSVtoRGB(hue, sat, val, color);
    strip.setPixelColor(x, color[0], color[1], color[2]);
  }
  strip.show();
}

/**************************************************************************/
// Fill in all Leds with a color
/**************************************************************************/
void FillAll(byte red, byte green, byte blue) {
  for (int x=0; x < 150; x++) {
    strip.setPixelColor(x, red, green, blue);
  }
  strip.show();  
}

/**************************************************************************/
// Fill in all Red
/**************************************************************************/
void Red() {
  FillAll(255, 0, 0);
}

/**************************************************************************/
// Fill in all Green
/**************************************************************************/
void Green() {
  FillAll(0, 255, 0);
}

/**************************************************************************/
// Fill in all Blue
/**************************************************************************/
void Blue() {
  FillAll(0, 0, 255);
}

/**************************************************************************/
// Fill in all Yellow
/**************************************************************************/
void Yellow() {
  FillAll(255, 255, 0);
}

/**************************************************************************/
// Fill in all Magenta
/**************************************************************************/
void Magenta() {
  FillAll(255, 0, 255);
}

/**************************************************************************/
// Fill in all Cyan
/**************************************************************************/
void Cyan() {
  FillAll(0, 255, 255);
}

//****************************************************************
// Strip all Spaces from the Strings that dtostrf creates,
//   needed to correct passing to xively
//   will replace all spaces with null, This will truncate the
//   string at the first space.
//   Returns the same string for easy calling.
//****************************************************************
char* StripSpaces(char* s, int size) {
  for(int x = size-1; x > 0; x--) if (s[x] == ' ') s[x] = 0x00;
  return s;
}

// Used for both the float2str and int2str functions
#define TMPWIDTH 9
static char tmpstr[TMPWIDTH+1];  

//****************************************************************
// Convert a float to a string without any spaces
//****************************************************************
char * float2str(float f, int pre) {
  return StripSpaces( dtostrf( f, -TMPWIDTH, pre, tmpstr), TMPWIDTH);
}

//****************************************************************
// Convert a int to a string without any spaces
//****************************************************************
char * int2str(int i) {
  sprintf(tmpstr, "%i", i);
  return tmpstr;
}

//****************************************************************
// Call the Script to Get the SLP and Set the Global var with the Value
//****************************************************************
void GetAndSetSLP() {
  // Get the SLP from Metar at KIAD
  Process SLP;
  SLP.begin(SLPSCRIPTNAME);
  SLP.run();
  
  String reply = "";
  while(SLP.available()>0) {
    char c = SLP.read();
    reply += c;
  }
  float tmp = reply.toFloat();
  if (tmp > 0) {
    SLPFoundFlag = 1;
    SLPress = tmp;  
    Console.print(F("Updated SeaLevelPressure: "));
    Console.print(SLPress);
    Console.println(" hPa");

    oled.setFont(FONTBIG);
    oled.clear();
    oled.setCursor(0,2);
    oled.print(F("KIAD SLP : "));
    oled.println( float2str(SLPress, 1) );

  } else {
    SLPFoundFlag = 0;
    Console.println(F(" SeaLevelPressure not found!"));
    oled.setFont(FONTBIG);
    oled.clear();
    oled.println(F("SLP Not Found!"));
  }
}

//****************************************************************
// Print out a Nice Display of the Sensor Value
//****************************************************************
void PrintSensor(const __FlashStringHelper *namest, float val, char *unit) {
  Console.print(namest);
  Console.print(val);
  Console.println(unit);    
}

//****************************************************************
// Grab the Sensor Data, Display it, and Post to anywere we need it.
//****************************************************************
void GetSensorDataAndPost() {
  static float temperature;
  static float Alt;
  static float hum;
  static float dhttemp;
  
  /* Get a new sensor event */ 
  sensors_event_t event;
  bmp.getEvent(&event);

  // Check to see if we got the Sensor Data
  if (event.pressure)  {   
    bmp.getTemperature(&temperature);
    Alt = bmp.pressureToAltitude(SLPress, event.pressure);
    hum = dht.readHumidity();
    dhttemp = dht.readTemperature();

    PrintSensor(F("SeaLevelPressure: "), SLPress, " hPa");  
    PrintSensor(F("Pressure:         "), event.pressure, " hPa");   
    PrintSensor(F("Temperature:      "), temperature, " C");
    PrintSensor(F("Altitude:         "), Alt, " m"); 
    PrintSensor(F("DHT Temp:         "), dhttemp, " C");
    PrintSensor(F("DHT Humidity:     "), hum, " %");
    Console.println("");

    //OLED
    oled.setFont(FONTSMALL);
    oled.clear();
    oled.setCursor(0,2);
    oled.print(F("SLP         : "));
    oled.println( float2str(SLPress, 1) );
    oled.print(F("Pressure    : "));
    oled.println(event.pressure);
    oled.print(F("Temperature : "));
    oled.println(temperature);
    oled.print(F("Altitude    : "));
    oled.println(Alt);   
    oled.print(F("DHT Temp    : "));
    oled.println(dhttemp);
    oled.print(F("DHT Humidity: "));
    oled.println( float2str(hum,1) );

    Process onData;
    onData.begin(SCRIPTNAME);
    onData.addParameter( float2str(temperature, 2) );
    onData.addParameter( float2str(event.pressure, 2) );
    onData.addParameter( float2str(Alt, 2) );
    onData.addParameter( float2str(SLPress, 2) );
    onData.addParameter( int2str(SLPFoundFlag) );
    onData.addParameter( float2str(dhttemp, 1) );
    onData.addParameter( float2str(hum, 1) );
    onData.run();
  } else  {
    Console.println(F("Sensor error"));
  }
}

bool ledflip = 0;
int lastledtime = 0;
int ledcolor = 0;

/**************************************************************************/
/*
    Arduino loop function, called once 'setup' is complete (your own code
    should go here)
*/
/**************************************************************************/
void loop(void) 
{
  static int seconds = -1;       // Start Right away
  static long lasttime = -1000;  // Start Right away
  Process onGoal;

  // Must be called at least ever second
  long curtime = millis();
  if (curtime >= (lasttime + 1000)) {
    lasttime = curtime;
    seconds++;
    // Check for SLP update Time
    if ((seconds % SLPUPDATETIME) == 0) {
      Blue();
      GetAndSetSLP();          
      if (SLPFoundFlag) {
        Green();
        delay(100);
      }
    }

    // Check for Data Update Time
    if ((seconds % DATAUPDATETIME) == 0) {
      Yellow();
      GetSensorDataAndPost();   
    }

    // Check for seconds Wrap
    if(seconds == WRAPTIME) seconds = 0;
/*
    ledcolor = 0;
    if ((seconds%5) == 0) {
      ledcolor = 1;
      lastledtime = seconds;
    } 
    if ((seconds%15) == 0) {
      ledcolor = 2;
      lastledtime = seconds;
    }   
*/    
  } // Once Second Passed

/*
  ledflip = !ledflip;
  if (ledflip) {
    switch (ledcolor) {
      case 0: 
        strip.clear(); 
        strip.show();
        break;
      case 1: Yellow(); 
        break;
      case 2: Blue(); 
        break;   
    }
  } else {
    strip.clear();
    strip.show();
  }
*/
  if (SLPFoundFlag) {
    ColorIt();  
  } else {
    Red();
  }
  delay(400);  // Just so we don't spin
} // loop


