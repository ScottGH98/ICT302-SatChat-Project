#include <SparkFunDS3234RTC.h>
#include <Adafruit_GPS.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include "TouchScreen.h"
#include <EEPROM.h>

// These are the four touchscreen analog pins
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an 00analog pin, use "An" notation!
#define YM 47   // can be any digital pin
#define XP 48   // can be any digital pin

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 90
#define TS_MINY 75
#define TS_MAXX 920
#define TS_MAXY 900

#define MINPRESSURE 10
#define MAXPRESSURE 1000

#define DS13074_CS_PIN 10 // DeadOn RTC Chip-select pin
#define INTERRUPT_PIN 4 // DeadOn RTC SQW/interrupt pin (optional)

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// The display uses hardware SP
#define TFT_CS 53
#define TFT_DC 49
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
long int touchPoint[2];
int mode = 0;

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

void setup(void) { 
  Serial.begin(9600,SERIAL_8N1);
  Serial2.begin(9600,SERIAL_8N1);
  
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);

  #ifdef INTERRUPT_PIN // If using the SQW pin as an interrupt
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  #endif
  rtc.begin(DS13074_CS_PIN);
  //rtc.setTime(0, 40, 2, 5, 26, 9, 19);
  rtc.update();
  rtc.enableAlarmInterrupt();
  rtc.setAlarm1(0);
  //SetupEeprom();
  
  //Build the main menu
  mainMenu();
}


void loop()
{
  // Retrieve a point
  TSPoint p = ts.getPoint();
  // Scale from ~0->1000 to tft.width using the calibration #'s
  touchPoint[0] = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
  touchPoint[1] = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());

  if(mode == 0) {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
        settings();
      } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
        coordinates();
      }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
        customMessages();
      }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
        presetMessages();
      }
    }
    
  }else if(mode == 1) {
    
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
         //function
        } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
          //function
        }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
          mainMenu();
        }
    }
    
  }else if(mode == 2) {
    
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
          //function
        } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
          //function
        }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
          mainMenu();
        }
    }
        
  }else if(mode == 3) {
    
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
          //function
        } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
          //function
        }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
          mainMenu();
        }
    }
        
  }else if(mode == 4) {
    
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
          //function
        } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
          //function
        }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
          //function
        }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
          mainMenu();
        }
    }
        
  }
  
  rtc.update();
  #ifdef INTERRUPT_PIN
    // Interrupt pin is active-low, if it's low, an alarm is triggered
    if (!digitalRead(INTERRUPT_PIN))
    {
  #endif
      // Check rtc.alarm1() to see if alarm 1 triggered the interrupt
      if (rtc.alarm1())
      {
        drawTime();
        rtc.setAlarm1(0);
      }
      if (rtc.alarm2())
      {
        rtc.setAlarm2(rtc.minute() + 1, rtc.hour());
      }
  #ifdef INTERRUPT_PIN
    }
  #endif
}

void presetMessages(){
  mode = 1;
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  tft.fillRect(0, 50, 160, 135, ILI9341_RED);
  tft.fillRect(160, 50, 240, 135, ILI9341_YELLOW);
  tft.fillRect(0, 145, 160, 240, ILI9341_GREEN);
  tft.fillRect(160, 145, 320, 240, ILI9341_CYAN);
  drawTime();
}

void customMessages(){
  mode = 2;
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  drawTime();
}

void coordinates(){
  mode = 3;
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  drawTime();
}

void settings(){
  mode = 4;
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  drawTime();
}

void SOS(){
  tft.fillScreen(ILI9341_BLACK);
  mode = 5;
}

void mainMenu(){
  mode = 0;
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 50, 160, 135, ILI9341_RED);
  tft.fillRect(160, 50, 240, 135, ILI9341_YELLOW);
  tft.fillRect(0, 145, 160, 240, ILI9341_GREEN);
  tft.fillRect(160, 145, 320, 240, ILI9341_CYAN);
  drawTime();
}


void SetupEeprom()
{
  strcpy(eeprom.settings.predefinedMessages[0], "All is going well.");
  strcpy(eeprom.settings.predefinedMessages[1], "Setting up camp.");
  strcpy(eeprom.settings.predefinedMessages[2], "Going for a hike.");
  strcpy(eeprom.settings.predefinedMessages[3], "Injured, need help.");
  eeprom.settings.breadcrumbs = true;
  eeprom.settings.breadInterval = 10;
  eeprom.settings.militaryTime = false;
  EEPROM.put(0,eeprom);
}

void drawTime() {
  rtc.update();
  tft.fillRect(200, 0, 320, 50, ILI9341_WHITE);
  tft.setCursor(200, 10);
  tft.setTextColor(0x0000);
  tft.setTextSize(4);
  tft.print(rtc.hour());
  tft.print(":");
  if(rtc.minute() < 10) 
    tft.print("0");
  tft.print(rtc.minute());
}
