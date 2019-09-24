#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <EEPROM.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include "TouchScreen.h"

// These are the four touchscreen analog pins
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM 47   // can be any digital pin
#define XP 48   // can be any digital pin

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 90
#define TS_MINY 75
#define TS_MAXX 920
#define TS_MAXY 900

#define MINPRESSURE 10
#define MAXPRESSURE 1000

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// The display uses hardware SP
#define TFT_CS 53
#define TFT_DC 49
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

struct DateTime
{
  short hour;
  short minute;
  short second;
  short day;
  short month;
  short year; 
};

struct GPS
{
  double longitutde;
  double latitude;
  struct DateTime;
};

struct Message
{
  char sender[20];
  char recipient[20];
  char content[200];
  struct GPS coords; 
};

struct Settings
{
  char predefinedMessages[4][200];
  bool breadcrumbs;
  short breadInterval;
  bool militaryTime;
};

struct Eeprom
{
  struct Message inbox[8];
  struct Message outbox[4];
  struct Settings settings; 
}eeprom;

void SetupEeprom(void)
{
  strcpy(eeprom.settings.predefinedMessages[0], "All if going well.");
  strcpy(eeprom.settings.predefinedMessages[1], "Setting up camp.");
  strcpy(eeprom.settings.predefinedMessages[2], "Going for a hike.");
  strcpy(eeprom.settings.predefinedMessages[3], "Injured, need help.");

  eeprom.settings.breadcrumbs = true;
  eeprom.settings.breadInterval = 10;
  eeprom.settings.militaryTime = false;
  EEPROM.put(0,eeprom);
}

void setup(void) { 
  Serial.begin(9600);
  
  tft.begin();
  tft.setRotation(3);

  
  //only run once ever to setup initial defaults on the eeprom
  //comment out after first run
  SetupEeprom(); 

  EEPROM.get(0,eeprom);
  
    
  MainMenu();
}


void loop()
{
  // Retrieve a point  
  TSPoint p = ts.getPoint();
  
  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) {
     return;
  }
  
  // Scale from ~0->1000 to tft.width using the calibration #'s
  long int xTouch = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
  long int yTouch = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());

  if(p.y < 120)
  {
    if(p.x < 160)
    {
      RedMenu();
    } else if(p.x < 240)
    {
      YellowMenu();
    }
  } else if (p.y < 240)
  {
    if(p.x < 160)
    {
      GreenMenu();
    } else if(p.x < 240)
    {
      BlueMenu();
    }
  }
  
  tft.fillCircle(xTouch, yTouch, 10, ILI9341_BLACK);
}

void MainMenu()
{
  tft.fillScreen(ILI9341_BLACK);
  
  // make the color selection boxes
  tft.fillRect(0, 0, 160, 120, ILI9341_RED);
  tft.fillRect(160, 0, 240, 120, ILI9341_YELLOW);
  tft.fillRect(0, 120, 160, 240, ILI9341_GREEN);
  tft.fillRect(160, 120, 240, 240, ILI9341_CYAN);
  loop();
}

void RedMenu()
{
  tft.fillScreen(ILI9341_RED);
  while(true)
  {
    // Retrieve a point  
    TSPoint p = ts.getPoint();
  
    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!
    if (p.z < MINPRESSURE || p.z > MAXPRESSURE) {
      return;
    }
  
    // Scale from ~0->1000 to tft.width using the calibration #'s
    long int xTouch = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
    long int yTouch = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
    if(p.y > 0 && p.y < 140 && p.x > 0 && p.x < 240)
    {
      MainMenu();
    }
  }
}

void YellowMenu()
{
  tft.fillScreen(ILI9341_YELLOW);
  while(true)
  {
    // Retrieve a point  
    TSPoint p = ts.getPoint();
  
    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!
    if (p.z < MINPRESSURE || p.z > MAXPRESSURE) {
      return;
    }
  
    // Scale from ~0->1000 to tft.width using the calibration #'s
    long int xTouch = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
    long int yTouch = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
    if(p.y > 0 && p.y < 140 && p.x > 0 && p.x < 240)
    {
      MainMenu();
    }
  }
}

void GreenMenu()
{
  tft.fillScreen(ILI9341_GREEN);
  while(true)
  {
    // Retrieve a point  
    TSPoint p = ts.getPoint();
  
    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!
    if (p.z < MINPRESSURE || p.z > MAXPRESSURE) {
      return;
    }
  
    // Scale from ~0->1000 to tft.width using the calibration #'s
    long int xTouch = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
    long int yTouch = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
    if(p.y > 0 && p.y < 140 && p.x > 0 && p.x < 240)
    {
      MainMenu();
    }
  }
}

void BlueMenu()
{
  tft.fillScreen(ILI9341_CYAN);
  while(true)
  {
    // Retrieve a point  
    TSPoint p = ts.getPoint();
  
    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!
    if (p.z < MINPRESSURE || p.z > MAXPRESSURE) {
      return;
    }
  
    // Scale from ~0->1000 to tft.width using the calibration #'s
    long int xTouch = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
    long int yTouch = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
    if(p.y > 0 && p.y < 140 && p.x > 0 && p.x < 240)
    {
      MainMenu();
    }
  }
}
