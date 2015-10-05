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
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(FONTBIG);
  oled.clear();
  oled.print("MJW Xively Demo");

  // Setup the Yun Bridge, Console, and FileIO
  Bridge.begin();
  Console.begin();
  FileSystem.begin();

  oled.clear();
  oled.print("MJW - Yun Done");

  //while(!Console);
  Console.println(F("Welcome to BMP185, DHT22 and Xively Demo from MJW")); 

  if(!bmp.begin())
  {
    oled.clear();
    oled.print("No BMP");
    Console.print(F("no BMP185 detected ... Check your wiring or I2C ADDR!"));
  } 

  // DHT Sensor
  dht.begin();

  uploadScript();
  GetAndSetSLP();
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

  // Must be called at least ever second
  long curtime = millis();
  if (curtime >= (lasttime + 1000)) {
    lasttime = curtime;
    seconds++;
    
    // Check for SLP update Time
    if ((seconds % SLPUPDATETIME) == 0) {
      GetAndSetSLP();          
    }

    // Check for Data Update Time
    if ((seconds % DATAUPDATETIME) == 0) {
      GetSensorDataAndPost();   
    }

    // Check for seconds Wrap
    if(seconds == WRAPTIME) seconds = 0;
  } // Once Second Passed

  delay(400);  // Just so we don't spin
} // loop


