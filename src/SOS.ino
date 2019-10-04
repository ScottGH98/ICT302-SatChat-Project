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

byte factorySettings;

void setup(void) { 
  
  Serial.begin(9600,SERIAL_8N1);
  Serial.println("Device initiallised");
  Serial2.begin(9600,SERIAL_8N1);
  Serial3.begin(9600); //EMIC serial

  Serial3.print("\nX\nS"); //End command (just in case),Stop speaking, prepare to speak. Do not end command until message has been sent;
  Serial3.println("Powered up."); //"SPowered up\n" is the entire command and terminates by itself. "S\nPowered up" will not work. 

  //set up the emic
  //Serial3.println("N0"); //set voice; N0 to N8
  //Serial3.println("V18"); //set volume; V-48 to V18
  //Serial3.println("W75"); //set words per minute. W75 to W600
  
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

  

  EEPROM.get(0,eeprom);
  factorySettings = EEPROM.read(0); //the MEGA default for EEPROM is 255; if we have set up the device before it will not be 255
  if(factorySettings == 255){ //this is a new device; conduct first time set up
    SetupEeprom();
  }
  
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
        
  }else if(mode == 4) { //this is settings
    
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 60 && touchPoint[1] < 110){
          if(eeprom.settings.breadcrumbs == true){
            eeprom.settings.breadcrumbs = false;
          } else if (eeprom.settings.breadcrumbs == false){
            eeprom.settings.breadcrumbs = true;
          }
          settings();
        }else if(touchPoint[1] > 110 && touchPoint[1] < 160){
          if(eeprom.settings.militaryTime == true){
            eeprom.settings.militaryTime = false;
          } else if (eeprom.settings.militaryTime == false){
            eeprom.settings.militaryTime = true;
          }
          settings();
        }else if(touchPoint[1] > 165 && touchPoint[1] < 185){
          //set text function
          settings(); //refresh the screen
        }else if(touchPoint[1] > 190 && touchPoint[1] < 230){
          if(touchPoint[0] > 240 && eeprom.settings.breadInterval < 15){
            eeprom.settings.breadInterval = eeprom.settings.breadInterval + 1;
          }else if (touchPoint[0] < 70 && eeprom.settings.breadInterval > 0){
            eeprom.settings.breadInterval = eeprom.settings.breadInterval - 1;
          }
          settings(); //refresh the screen
        }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
          //before going to menu, we write everything back into eeprom for next time
          EEPROM.put(0,eeprom);
          mainMenu();
        }
        
    }
        
  }//we probably need another screen for the custom texts
  
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
  Serial3.print("\nX\nS");
  Serial3.println("View preset messages.");
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
  Serial3.print("\nX\nS");
  Serial3.println("Send custom messages.");
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  drawTime();
}

void coordinates(){
  mode = 3;
  Serial3.print("\nX\nS");
  Serial3.println("Viewing coordinated.");
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  drawTime();
}

void settings(){
  mode = 4;
  Serial3.print("\nX\nS");
  Serial3.println("Settings menu.");
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK); //(x,y,xwidth,yheight)
  tft.fillRect(0,50,320,240, ILI9341_CYAN);
  tft.fillRect(5,55,310,40, ILI9341_BLACK);
  
  //checkbox for breadcrumbs
  tft.setCursor(10,60);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(3);
  tft.println("Breadcrumbs");
  tft.fillRect(280,60,30,30,ILI9341_CYAN);
  if(eeprom.settings.breadcrumbs == true){
    tft.fillRect(285,65,20,20,ILI9341_BLACK);
  }

  //checkbox for 24 hour time
  tft.fillRect(5,100,310,40, ILI9341_BLACK);
  tft.setCursor(10,105);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(3);
  tft.println("24 Hour Clock");
  tft.fillRect(280,105,30,30,ILI9341_CYAN);
  if(eeprom.settings.militaryTime == true){
    tft.fillRect(285,110,20,20,ILI9341_BLACK);
  }
  //text editor
  tft.fillRect(5,145,310,40, ILI9341_BLACK);
  tft.setCursor(10,150);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(3);
  tft.println("Set texts");

  //time editor
  tft.fillRect(5,190,310,40, ILI9341_BLACK);
  tft.setCursor(50,195);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(3);
  tft.println("Interval: ");
  tft.setCursor(230,195);
  tft.println(eeprom.settings.breadInterval);
  tft.fillRect(10,195,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,210,30,200,30,220, ILI9341_BLACK);
  tft.fillRect(280,195,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,210,290,200,290,220, ILI9341_BLACK);

  //update time
  drawTime();
}

void SOS(){
  Serial3.print("X\nS");
  Serial3.println("Emergency protocal activated.");
  tft.fillScreen(ILI9341_BLACK);
  mode = 5;
}

void mainMenu(){
  mode = 0;
  Serial3.print("X\nS");
  Serial3.println("Main menu.");
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
