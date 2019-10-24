#include <SparkFunDS3234RTC.h>//I use a different library because I have a different chip
#include <Adafruit_GPS.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include "TouchScreen.h"
#include <EEPROM.h>
#include <Adafruit_LSM303_U.h>
#include <Adafruit_Sensor.h>
#include "graphics.h"
#include <TinyGPS.h>


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
TinyGPS gps;
float lat, lon;
int gpsYear;
byte gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond, gpsHundredth;
unsigned long fix_age;
bool erased = false;

//Set up the LSM303
Adafruit_LSM303_Mag_Unified mag = Adafruit_LSM303_Mag_Unified(12345);
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(22345); //the number here is random, but needs to be unique as it is an ID number

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
  float longitude;
  float latitude;
  struct DateTime gpsDateTime;
  struct DateTime dateTime;
};
struct Message
{
  char recipient[15];
  char sender[15];
  char content[200];
  struct GPS coords; 
};
struct Settings
{
  char predefinedMessages[4][200];
  short breadInterval;
  int currentInterval;
  bool militaryTime;
  char recipient[15];
  float utcOffset;
  short currentOffset;
  bool gpsTime;
  bool narrator;
};
struct Eeprom
{
  struct Message inbox[4];
  struct Message outbox[4];
  struct Settings settings; 
}eeprom;

bool sleeping = false;
bool sosing = false;
bool isNumBoard = false;
bool isLower = true;
char str[200];
char strNum[15];
int preNum = 0;
int incomingByte;
int lastTime = 0;
int lastDate = 0;
int breadCount = 1000;
int intervalMins[] = {0,1,2,5,10,15,30,60,120,180,240,300,360,720};
float timezones[] = {-12,-11,-10,-9.5,-9,-8,-7,-6,-5,-4,-3.5,-3,-2,-1,0,1,2,3,3.5,4,4.5,5,5.5,5.75,6,6.5,7,8,8.75,9,9.5,10,10.5,11,12,12.75,13,14};

byte factorySettings;

void setup(void) { 
  pinMode(2,INPUT_PULLUP);
  pinMode(3,INPUT_PULLUP);
  
  Serial.begin(9600,SERIAL_8N1);
  Serial.println("Device initiallised"); //Serial Output
  Serial1.begin(9600,SERIAL_8N1); //TTS Audio
  Serial2.begin(9600,SERIAL_8N1); //GPS
  Serial3.begin(9600); //EMIC serial

  Serial3.print("\nX\nS"); //End command (just in case),Stop speaking, prepare to speak. Do not end command until message has been sent;
  Serial3.println("Powered up."); //"SPowered up\n" is the entire command and terminates by itself. "S\nPowered up" will not work. 

  //set up the emic
  //Serial3.println("N0"); //set voice; N0 to N8
  //Serial3.println("V18"); //set volume; V-48 to V18
  //Serial3.println("W75"); //set words per minute. W75 to W600

  //turn on screen
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);

  //turn on compass
  mag.begin();
  accel.begin();

  #ifdef INTERRUPT_PIN // If using the SQW pin as an interrupt
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  #endif
  rtc.begin(DS13074_CS_PIN);
  rtc.update();
  rtc.enableAlarmInterrupt();
  rtc.setAlarm1(0);
  rtc.enableAlarmInterrupt();
  rtc.setAlarm2(rtc.minute() + 1);
  

  

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
  //delay(3500);
  // Retrieve a point
  TSPoint p = ts.getPoint();
  // Scale from ~0->1000 to tft.width using the calibration #'s
  touchPoint[0] = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
  touchPoint[1] = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
    
  int blackState = digitalRead(2);
  int redState = digitalRead(3);

  //Calls SOS function if red button is pressed
  if(redState == LOW)
  {
    if(sosing == false)
    {
      sleeping = false;
      sosing = true;
      SOS();
    } else
    {
      sosing = false;
      mainMenu();
    }
  }

  //Calls sleep function if black button is pressed
  if(blackState == LOW)
  {
    if(sleeping == false)
    {
      sleeping = true;
      sosing = false;
      Sleep();
    } else
    {
      sleeping = false;
      mainMenu();
    }
  }

  while(Serial2.available()){ // check for gps data 
    if(gps.encode(Serial2.read()))// encode gps data 
    {  
      gps.crack_datetime(&gpsYear, &gpsMonth, &gpsDay, &gpsHour, 
        &gpsMinute, &gpsSecond, &gpsHundredth, &fix_age);
      gps.f_get_position(&lat,&lon);  
    }
  }

  //Gets Input from serial used to emulate receiving message
  while(Serial.available())
  {
    String in = Serial.readString();
    ReceiveMessage(in);
  }

  //Gets Time from GPS
  GetGpsTime();
 
  if(mode == 0) //Main Menu
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){ //cyan
        settings();
      } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
        coordinates();
      }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
        customMessages();
      }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
        presetMessages();
      }
    }
    
  }else if(mode == 1) //Predefined Messages
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
        SendMessage(4);
      } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
        SendMessage(3);
        if(eeprom.settings.narrator)
        {
          Serial1.println("SMessage Sent");
        }
        delay(1000);
      }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
        SendMessage(2);
        if(eeprom.settings.narrator)
        {
          Serial1.println("SMessage Sent");
        }
        delay(1000);
      }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
        SendMessage(1);
        if(eeprom.settings.narrator)
        {
          Serial1.println("SMessage Sent");
        }
        delay(1000);
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        EEPROM.put(0,eeprom);
        mainMenu();
      }
    }
    
  }else if(mode == 2) //Custom Messages
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 145){
          Inbox();
        }else if(touchPoint[1] > 50){
          Outbox();
        }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
          EEPROM.put(0,eeprom);
          mainMenu();
        } else if(touchPoint[0] > 50 && touchPoint[1] < 50 && touchPoint[0] < 140)
        {
          TypeMsg();
        }
        
    }
        
  }else if(mode == 3) //GPS Coordinates
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){        
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        mainMenu();
      }
    }
    else
    {
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(130, 55);
      tft.setTextSize(2);
      tft.println("Bearing:");
      tft.setCursor(130, 75);
      drawBearing();
    }
        
  }else if(mode == 4) //Settings Menu
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 60 && touchPoint[1] < 110){
        messageSettings();      
      }else if(touchPoint[1] > 110 && touchPoint[1] < 160){
        clockSettings();
      }else if(touchPoint[1] > 165 && touchPoint[1] < 185){
        generalSettings();
      }else if(touchPoint[1] > 190 && touchPoint[1] < 230){
        if(touchPoint[0] > 240 && eeprom.settings.currentInterval < 13){
          eeprom.settings.currentInterval++;
          eeprom.settings.breadInterval = intervalMins[eeprom.settings.currentInterval];
          EEPROM.put(0,eeprom);
          delay(200);
        }else if (touchPoint[0] < 70 && eeprom.settings.currentInterval > 0){
          eeprom.settings.currentInterval--;
          eeprom.settings.breadInterval = intervalMins[eeprom.settings.currentInterval];
          EEPROM.put(0,eeprom);
          delay(200);
        }
        tft.fillRect(220,190,59,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(220,195);
        tft.println(eeprom.settings.breadInterval);
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        //before going to menu, we write everything back into eeprom for next time
        EEPROM.put(0,eeprom);
        mainMenu();
      }
        
    } 
        
  } else if(mode == 5) //SOS
  {
      SendMessage(5);    
  } else if(mode == 6) // expand outbox
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        Outbox();
      }
    }
    
  } else if(mode == 7) //Sleep
  {
    
  } else if(mode == 8) //Type custom message
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        customMessages();
      } else 
      {
        
        tft.fillRect(0, 50, 320, 34, ILI9341_BLACK);
        tft.setCursor(0,50);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(3);
        int count = DrawKeyboard(str);
        if(count == -1)
        {          
          SendMessage(str);
          Zero(str,200);
        } else if(count <= 17)
        {
          tft.print(str);
        } else
        {
          for(int i = count-17; i <= count-1; i++)
          {
            tft.print(str[i]);
          }
        }
        
      }
    }
    
  } else if(mode == 9) //Chose message to predefine
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
        preNum = 3;
        typePresetMessages();
      } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
        preNum = 2;
        typePresetMessages();
      }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
        preNum = 1;
        typePresetMessages();
      }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
        preNum = 0;
        typePresetMessages();
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        messageSettings();
      }
    }
    
  }else if(mode == 10) //Set Predefined Messages
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        setPresetMessages();
      } else 
      {
        
        tft.fillRect(0, 50, 320, 34, ILI9341_BLACK);
        tft.setCursor(0,50);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(3);
        int count = DrawKeyboard(str);
        if(count == -1)
        {          
          if(preNum == 0)
          {
            strcpy(eeprom.settings.predefinedMessages[0], str);
            EEPROM.put(0,eeprom);
            setPresetMessages();
          } else if(preNum == 1)
          {
            strcpy(eeprom.settings.predefinedMessages[1], str);
            EEPROM.put(0,eeprom);
            setPresetMessages();
          } else if(preNum == 2)
          {
            strcpy(eeprom.settings.predefinedMessages[2], str);
            EEPROM.put(0,eeprom);
            setPresetMessages();
          } else if(preNum == 3)
          {
            strcpy(eeprom.settings.predefinedMessages[3], str);
            EEPROM.put(0,eeprom);
            setPresetMessages();
          }
          Zero(str,200);
        } else if(count <= 17)
        {
          tft.print(str);
        } else
        {
          for(int i = count-17; i <= count-1; i++)
          {
            tft.print(str[i]);
          }
        }
        
      }
    }
  } else if(mode == 11) //Set Recipient
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        messageSettings();
      } else
      {
        tft.fillRect(0, 50, 320, 34, ILI9341_BLACK);
        tft.setCursor(0,50);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(3);
        int count = DrawNumkey(strNum);
        if(count == -1)
        {     
          strcpy(eeprom.settings.recipient,strNum);
          EEPROM.put(0,eeprom);
        } else
        {
          tft.print(strNum);
        }

        if(count > 0)
        {
          tft.fillRect(280, 86, 35, 62, ILI9341_BLACK);
          erased = true;
        } else if(erased)
        {
          tft.drawBitmap(0,0,numkey,320,240,ILI9341_WHITE);
          erased = false;
        }
      }
    }
    
  } else if(mode == 12) //Clock Settings
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 60 && touchPoint[1] < 110){
        if(touchPoint[0] > 240 && eeprom.settings.currentOffset < 37){
          eeprom.settings.currentOffset++;
          eeprom.settings.utcOffset = timezones[eeprom.settings.currentOffset];
          EEPROM.put(0,eeprom);
          delay(200);
        }else if (touchPoint[0] < 70 && eeprom.settings.currentOffset > 0){
          eeprom.settings.currentOffset--;
          eeprom.settings.utcOffset = timezones[eeprom.settings.currentOffset];
          EEPROM.put(0,eeprom);
          delay(200);
        }
        tft.fillRect(170,55,109,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(170,60);
        if(eeprom.settings.utcOffset > 0)
        {
          tft.print("+");
        }
        if(ceilf(eeprom.settings.utcOffset) == eeprom.settings.utcOffset)
        {
          tft.print((int)eeprom.settings.utcOffset);
          tft.print(":");
          tft.print("00");
        } else if((eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == 0.5 || (eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == -0.5)
        {
          tft.print((int)eeprom.settings.utcOffset);
          tft.print(":");
          tft.print("30");
        } else if((eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == 0.75 || (eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == -0.75)
        {
          tft.print((int)eeprom.settings.utcOffset);
          tft.print(":");
          tft.print("45");
        }       
      }else if(touchPoint[1] > 110 && touchPoint[1] < 160){
        if(eeprom.settings.gpsTime == true){
          eeprom.settings.gpsTime = false;
          
          tft.fillRect(5,190,310,40, ILI9341_BLACK);
          tft.setCursor(10,195);
          tft.setTextColor(ILI9341_WHITE);  
          tft.setTextSize(3);
          tft.println("Set Date");
          
          EEPROM.put(0,eeprom);
          drawTime();
          tft.fillRect(285,110,20,20,ILI9341_CYAN);
          delay(300);
        } else if (eeprom.settings.gpsTime == false){
          eeprom.settings.gpsTime = true;

          tft.fillRect(5,190,310,40, 0x8410);
          tft.setCursor(10,195);
          tft.setTextColor(ILI9341_WHITE);  
          tft.setTextSize(3);
          tft.println("Set Date");
          
          EEPROM.put(0,eeprom);
          drawTime();
          tft.fillRect(285,110,20,20,ILI9341_BLACK);
          delay(300);
        }          
      }else if(touchPoint[1] > 165 && touchPoint[1] < 185){
        timeSettings();
        
      }else if(touchPoint[1] > 190 && touchPoint[1] < 230 && !eeprom.settings.gpsTime){
        dateSettings();
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        //before going to menu, we write everything back into eeprom for next time
        EEPROM.put(0,eeprom);
        settings();
      }
        
    }
  } else if(mode == 13) //Time Settings
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 60 && touchPoint[1] < 110 && !eeprom.settings.gpsTime){  
          if(touchPoint[0] > 240 && rtc.getHour() < 23){
          rtc.setHour(rtc.getHour() + 1);
          delay(200);
        }else if (touchPoint[0] < 70 && rtc.getHour() > 0){
          rtc.setHour(rtc.getHour() - 1);
          delay(200);
        }
        tft.fillRect(220,55,49,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(220,60);
        tft.print(rtc.getHour());
        drawTime();
      }else if(touchPoint[1] > 110 && touchPoint[1] < 160 && !eeprom.settings.gpsTime){
        if(touchPoint[0] > 240 && rtc.getMinute() < 59){
          rtc.setMinute(rtc.getMinute() + 1);
          delay(200);
        }else if (touchPoint[0] < 70 && rtc.getMinute() > 0){
          rtc.setMinute(rtc.getMinute() - 1);
          delay(200);
        }
        tft.fillRect(220,100,49,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(220,105);
        tft.println(rtc.getMinute());   
        drawTime();  
      }else if(touchPoint[1] > 165 && touchPoint[1] < 185 && !eeprom.settings.gpsTime){
        if(touchPoint[0] > 240 && rtc.getSecond() < 59){
          rtc.setSecond(rtc.getSecond() + 1);
          delay(200);
        }else if (touchPoint[0] < 70 && rtc.getSecond() > 0){
          rtc.setSecond(rtc.getSecond() - 1);
          delay(200);
        }
        tft.fillRect(220,145,49,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(220,150);
        tft.println(rtc.getSecond());
        drawTime();
      }else if(touchPoint[1] > 190 && touchPoint[1] < 230){
        if(eeprom.settings.militaryTime == true){
          eeprom.settings.militaryTime = false;
          EEPROM.put(0,eeprom);
          drawTime();
          tft.fillRect(285,200,20,20,ILI9341_CYAN);
          delay(300);
        } else if (eeprom.settings.militaryTime == false){
          eeprom.settings.militaryTime = true;
          EEPROM.put(0,eeprom);
          drawTime();
          tft.fillRect(285,200,20,20,ILI9341_BLACK);
          delay(300);
        }
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        clockSettings();
      }
        
    }
  } else if(mode == 14) //Date Settings
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 60 && touchPoint[1] < 110){  
          if(touchPoint[0] > 240 && rtc.getYear() < 23){
          rtc.setYear(rtc.getYear() + 1);
          delay(200);
        }else if (touchPoint[0] < 70 && rtc.getYear() > 0){
          rtc.setYear(rtc.getYear() - 1);
          delay(200);
        }
        tft.fillRect(220,55,49,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(220,60);
        tft.print(rtc.getYear());
      }else if(touchPoint[1] > 110 && touchPoint[1] < 160){
        if(touchPoint[0] > 240 && rtc.getMonth() < 59){
          rtc.setMonth(rtc.getMonth() + 1);
          delay(200);
        }else if (touchPoint[0] < 70 && rtc.getMonth() > 0){
          rtc.setMonth(rtc.getMonth() - 1);
          delay(200);
        }
        tft.fillRect(220,100,49,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(220,105);
        tft.println(rtc.getMonth());   
      }else if(touchPoint[1] > 165 && touchPoint[1] < 185){
        if(touchPoint[0] > 240 && rtc.getDate() < 59){
          rtc.setDate(rtc.getDate() + 1);
          delay(200);
        }else if (touchPoint[0] < 70 && rtc.getDate() > 0){
          rtc.setDate(rtc.getDate() - 1);
          delay(200);
        }
        tft.fillRect(220,145,49,40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(3);
        tft.setCursor(220,150);
        tft.println(rtc.getDate());
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        clockSettings();
      }
        
    }
  } else if(mode == 15) //Outbox
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 191){
        tft.fillRect(0, 50, 320, 240, ILI9341_YELLOW);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.outbox[3].recipient);
        tft.print("  ");
        tft.print(eeprom.outbox[3].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.outbox[3].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.outbox[3].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.outbox[3].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.outbox[3].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.outbox[3].coords.dateTime.year);
        tft.print(" - ");
        tft.print(eeprom.outbox[3].coords.latitude,6);
        tft.print(",");
        tft.print(eeprom.outbox[3].coords.longitude,6);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.outbox[3].content,26,10);
        mode = 6;
      }else if(touchPoint[1] > 144){
        tft.fillRect(0, 50, 320, 240, ILI9341_RED);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.outbox[2].recipient);
        tft.print("  ");
        tft.print(eeprom.outbox[2].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.outbox[2].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.outbox[2].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.outbox[2].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.outbox[2].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.outbox[2].coords.dateTime.year);
        tft.print(" - ");
        tft.print(eeprom.outbox[2].coords.latitude,6);
        tft.print(",");
        tft.print(eeprom.outbox[2].coords.longitude,6);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.outbox[2].content,26,10);
        mode = 6;
      }else if(touchPoint[1] > 97){
        tft.fillRect(0, 50, 320, 240, ILI9341_CYAN);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.outbox[1].recipient);
        tft.print("  ");
        tft.print(eeprom.outbox[1].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.outbox[1].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.outbox[1].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.outbox[1].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.outbox[1].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.outbox[1].coords.dateTime.year);
        tft.print(" - ");
        tft.print(eeprom.outbox[1].coords.latitude,6);
        tft.print(",");
        tft.print(eeprom.outbox[1].coords.longitude,6);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.outbox[1].content,26,10);
        mode = 6;
      }else if(touchPoint[1] > 50){
        tft.fillRect(0, 50, 320, 240, ILI9341_GREEN);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.outbox[0].recipient);
        tft.print("  ");
        tft.print(eeprom.outbox[0].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.outbox[0].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.outbox[0].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.outbox[0].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.outbox[0].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.outbox[0].coords.dateTime.year);
        tft.print(" - ");
        tft.print(eeprom.outbox[0].coords.latitude,6);
        tft.print(",");
        tft.print(eeprom.outbox[0].coords.longitude,6);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.outbox[0].content,26,10);
        mode = 6;
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        EEPROM.put(0,eeprom);
        customMessages();
      } else if(touchPoint[0] > 50 && touchPoint[1] < 50 && touchPoint[0] < 140)
      {
        TypeMsg();
      }
    }
  } else if(mode == 16) //Inbox
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 191){
        tft.fillRect(0, 50, 320, 240, ILI9341_YELLOW);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.inbox[3].sender);
        tft.print("  ");
        tft.print(eeprom.inbox[3].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.inbox[3].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.inbox[3].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.inbox[3].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.inbox[3].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.inbox[3].coords.dateTime.year);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.inbox[3].content,26,10);
        mode = 17;
      }else if(touchPoint[1] > 144){
        tft.fillRect(0, 50, 320, 240, ILI9341_RED);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.inbox[2].sender);
        tft.print("  ");
        tft.print(eeprom.inbox[2].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.inbox[2].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.inbox[2].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.inbox[2].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.inbox[2].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.inbox[2].coords.dateTime.year);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.inbox[2].content,26,10);
        mode = 17;
      }else if(touchPoint[1] > 97){
        tft.fillRect(0, 50, 320, 240, ILI9341_CYAN);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.inbox[1].sender);
        tft.print("  ");
        tft.print(eeprom.inbox[1].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.inbox[1].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.inbox[1].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.inbox[1].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.inbox[1].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.inbox[1].coords.dateTime.year);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.inbox[1].content,26,10);
        mode = 17;
      }else if(touchPoint[1] > 50){
        tft.fillRect(0, 50, 320, 240, ILI9341_GREEN);
        tft.setCursor(5, 55);
        tft.setTextColor(0x0000);
        tft.setTextSize(1);
        tft.print(eeprom.inbox[0].sender);
        tft.print("  ");
        tft.print(eeprom.inbox[0].coords.dateTime.hour);
        tft.print(":");
        tft.print(eeprom.inbox[0].coords.dateTime.minute);
        tft.print(":");
        tft.print(eeprom.inbox[0].coords.dateTime.second);
        tft.print("  ");
        tft.print(eeprom.inbox[0].coords.dateTime.day);
        tft.print("/");
        tft.print(eeprom.inbox[0].coords.dateTime.month);
        tft.print("/");
        tft.print(eeprom.inbox[0].coords.dateTime.year);
        tft.print("\n");
        tft.setTextSize(2);
        textWrap(eeprom.inbox[0].content,26,10);
        mode = 17;
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        EEPROM.put(0,eeprom);
        customMessages();
      } else if(touchPoint[0] > 50 && touchPoint[1] < 50 && touchPoint[0] < 140)
      {
        TypeMsg();
      }
    }
  } else if(mode == 17) // expand inbox
  {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        Inbox();
      }
    }  
  } else if(mode == 18) //Message Settings
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 60 && touchPoint[1] < 110){
        TypeNum();        
      }else if(touchPoint[1] > 110 && touchPoint[1] < 160){
        setPresetMessages();
      }else if(touchPoint[1] > 165 && touchPoint[1] < 185){
      }else if(touchPoint[1] > 190 && touchPoint[1] < 230){
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        //before going to menu, we write everything back into eeprom for next time
        EEPROM.put(0,eeprom);
        settings();
      }
        
    } 
        
  } else if(mode == 19) //General Settings
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 60 && touchPoint[1] < 110){
        if(eeprom.settings.narrator == true){
          eeprom.settings.narrator = false;
          EEPROM.put(0,eeprom);
          tft.fillRect(285,65,20,20,ILI9341_CYAN);
          delay(300);
        } else if (eeprom.settings.narrator == false){
          eeprom.settings.narrator = true;
          EEPROM.put(0,eeprom);
          tft.fillRect(285,65,20,20,ILI9341_BLACK);
          delay(300);
        }          
      }else if(touchPoint[1] > 110 && touchPoint[1] < 160){
        FactoryReset();
      }else if(touchPoint[1] > 165 && touchPoint[1] < 185){
      }else if(touchPoint[1] > 190 && touchPoint[1] < 230){
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        //before going to menu, we write everything back into eeprom for next time
        EEPROM.put(0,eeprom);
        settings();
      }
        
    }
  } else if(mode == 20) //Factory Reset
  {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 190){
        SetupEeprom();
        mainMenu();
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        //before going to menu, we write everything back into eeprom for next time
        EEPROM.put(0,eeprom);
        generalSettings();
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
        if(mode != 7)
        {
          drawTime();
        }
        rtc.setAlarm1(0);
      }
      if (rtc.alarm2())
      {
        if(eeprom.settings.breadInterval != 0)
        {
          breadCount++;
          if(breadCount >= eeprom.settings.breadInterval)
          {
            if(eeprom.settings.narrator)
            {
              Serial1.println("SBreadcrumb Sent.");
            }
            SendBread();
            breadCount = 0;
          }
        }
        rtc.setAlarm2(rtc.minute() + 1);
      }
  #ifdef INTERRUPT_PIN
    }
    #endif
  
}

void textWrap(char * msg, int siz,int lin)
{
  int count = 1;
  int size = strlen(msg);
  char str[size];
  for(int i = 0; i < size; i++)
  {
    str[i] = msg[i];
  }
   
  if(strlen(str) > siz)
  {
    int len = 0;
    char delim[] = " ";
    
    char *tkn = strtok(str, delim);

    while(tkn != NULL)
    {
      int add = strlen(tkn) + 1;
      len = len + add;
      if(len > siz)
      {
        tft.print("\n");
        len = 0;
        count++;
      }
      if(count <= lin)
      {
        tft.print(tkn);
        tft.print(" ");
      }
      
      
      tkn = strtok(NULL,delim);
    }
    
  } else
  {
    tft.print(msg);
  }
}

void textWrap2(char * msg)
{
  int count = 1;
  int size = strlen(msg);
  char str[size];
  for(int i = 0; i < size; i++)
  {
    str[i] = msg[i];
  }
  
  if(strlen(str) > 12)
  {
    int x = 70;
    int len = 0;
    char delim[] = " ";
    
    char *tkn = strtok(str, delim);

    while(tkn != NULL)
    {
      int add = strlen(tkn) + 1;
      len = len + add;
      if(len > 12)
      {
        tft.print("\n");
        tft.setCursor(165, x);
        x = x + 15;
        len = 0;
        count++;
      }
      if(count <= 5)
      {
        tft.print(tkn);
        tft.print(" ");
      }
      
      tkn = strtok(NULL,delim);
    }
    
  } else
  {
    tft.print(msg);
  }
}

void textWrap3(char * msg)
{
  int count = 1;
  int size = strlen(msg);
  char str[size];
  for(int i = 0; i < size; i++)
  {
    str[i] = msg[i];
  }
  
  if(strlen(str) > 12)
  {
    int len = 0;
    int x = 165;
    char delim[] = " ";
    
    char *tkn = strtok(str, delim);

    while(tkn != NULL)
    {
      int add = len + strlen(tkn) + 1;
      len = len + add;
      if(len > 12)
      {
        tft.print("\n");
        tft.setCursor(165, x);
        x = x + 15;
        len = 0;
        count++;
      }
      if(count <= 5)
      {
        tft.print(tkn);
        tft.print(" ");
      }
      
      tkn = strtok(NULL,delim);
    }
    
  } else
  {
    tft.print(msg);
  }
}

//Draws preset messages screen
void presetMessages(){
  mode = 1;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SPre-set messages.");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0, 50, 160, 135, ILI9341_RED);
  tft.fillRect(160, 50, 240, 135, ILI9341_YELLOW);
  tft.fillRect(0, 145, 160, 240, ILI9341_GREEN);
  tft.fillRect(160, 145, 320, 240, ILI9341_CYAN);
  drawTime();

  tft.setCursor(5, 55);
  tft.setTextColor(0x0000);
  tft.setTextSize(2);
  textWrap(eeprom.settings.predefinedMessages[0],12,5);
  tft.setCursor(165, 55);
  textWrap2(eeprom.settings.predefinedMessages[1]);
  tft.setCursor(5,150);
  textWrap(eeprom.settings.predefinedMessages[2],12,5);
  tft.setCursor(165,150);
  textWrap3(eeprom.settings.predefinedMessages[3]);
}

//draws custom messages screen
void customMessages(){
  mode = 2;
  EEPROM.get(0,eeprom);
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SCustom messages.");
  }
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  drawTime();
  
  tft.fillRect(50, 0, 90, 50, ILI9341_YELLOW);
  tft.setCursor(80,5);
  tft.setTextColor(0x0000);
  tft.setTextSize(2);
  tft.print("New");
  tft.setCursor(55,20);
  tft.print("Message");
  

  tft.fillRect(0, 50, 320, 135, ILI9341_GREEN);
  tft.setCursor(5, 55);
  tft.setTextColor(0x0000);
  tft.setTextSize(5);
  tft.print("Outbox");
  
  tft.fillRect(0, 145, 320, 240, ILI9341_CYAN);
  tft.setCursor(5, 150);
  tft.setTextColor(0x0000);
  tft.setTextSize(5);
  tft.print("Inbox");
}

//draws inbox screen
void Inbox()
{
  mode = 16;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SIn-box.");
  }
  //msg 1
  tft.fillRect(0, 50, 320, 47, ILI9341_GREEN);
  tft.setCursor(5, 55);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.inbox[0].sender);
  tft.print("  ");
  if(eeprom.inbox[0].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[0].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.inbox[0].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[0].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.inbox[0].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[0].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.inbox[0].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.inbox[0].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.inbox[0].coords.dateTime.year);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.inbox[0].content,26,2);

  //msg 2
  tft.fillRect(0, 97, 320, 47, ILI9341_CYAN);
  tft.setCursor(5, 102);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.inbox[1].sender);
  tft.print("  ");
  if(eeprom.inbox[1].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[1].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.inbox[1].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[1].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.inbox[1].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[1].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.inbox[1].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.inbox[1].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.inbox[1].coords.dateTime.year);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.inbox[1].content,26,2);

  //msg 3
  tft.fillRect(0, 144, 320, 47, ILI9341_RED);
  tft.setCursor(5, 149);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.inbox[2].sender);
  tft.print("  ");
  if(eeprom.inbox[2].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[2].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.inbox[2].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[2].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.inbox[2].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[2].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.inbox[2].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.inbox[2].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.inbox[2].coords.dateTime.year);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.inbox[2].content,26,2);

  //msg 4
  tft.fillRect(0, 191, 320, 49, ILI9341_YELLOW);
  tft.setCursor(5, 196);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.inbox[3].sender);
  tft.print("  ");
  if(eeprom.inbox[3].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[3].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.inbox[3].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[3].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.inbox[3].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.inbox[3].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.inbox[3].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.inbox[3].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.inbox[3].coords.dateTime.year);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.inbox[3].content,26,2);
}

//draws outbox screen
void Outbox()
{
  mode = 15;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SOut-box.");
  }
  //msg 1
  tft.fillRect(0, 50, 320, 47, ILI9341_GREEN);
  tft.setCursor(5, 55);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.outbox[0].recipient);
  tft.print("  ");
  if(eeprom.outbox[0].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[0].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.outbox[0].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[0].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.outbox[0].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[0].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.outbox[0].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.outbox[0].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.outbox[0].coords.dateTime.year);
  tft.print(" ");
  tft.print(eeprom.outbox[0].coords.latitude);
  tft.print(",");
  tft.print(eeprom.outbox[0].coords.longitude);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.outbox[0].content,26,2);

  //msg 2
  tft.fillRect(0, 97, 320, 47, ILI9341_CYAN);
  tft.setCursor(5, 102);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.outbox[1].recipient);
  tft.print("  ");
  if(eeprom.outbox[1].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[1].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.outbox[1].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[1].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.outbox[1].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[1].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.outbox[1].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.outbox[1].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.outbox[1].coords.dateTime.year);
  tft.print(" ");
  tft.print(eeprom.outbox[1].coords.latitude);
  tft.print(",");
  tft.print(eeprom.outbox[1].coords.longitude);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.outbox[1].content,26,2);

  //msg 3
  tft.fillRect(0, 144, 320, 47, ILI9341_RED);
  tft.setCursor(5, 149);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.outbox[2].recipient);
  tft.print("  ");
  if(eeprom.outbox[2].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[2].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.outbox[2].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[2].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.outbox[2].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[2].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.outbox[2].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.outbox[2].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.outbox[2].coords.dateTime.year);
  tft.print(" ");
  tft.print(eeprom.outbox[2].coords.latitude);
  tft.print(",");
  tft.print(eeprom.outbox[2].coords.longitude);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.outbox[2].content,26,2);

  //msg 4
  tft.fillRect(0, 191, 320, 49, ILI9341_YELLOW);
  tft.setCursor(5, 196);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print(eeprom.outbox[3].recipient);
  tft.print("  ");
  if(eeprom.outbox[3].coords.dateTime.hour < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[3].coords.dateTime.hour);
  tft.print(":");
  if(eeprom.outbox[3].coords.dateTime.minute < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[3].coords.dateTime.minute);
  tft.print(":");
  if(eeprom.outbox[3].coords.dateTime.second < 10)
  {
    tft.print("0");
  }
  tft.print(eeprom.outbox[3].coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.outbox[3].coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.outbox[3].coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.outbox[3].coords.dateTime.year);
  tft.print(" ");
  tft.print(eeprom.outbox[3].coords.latitude);
  tft.print(",");
  tft.print(eeprom.outbox[3].coords.longitude);
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.outbox[3].content,26,2);
}

//draws coordinates screen
void coordinates(){
  mode = 3;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SViewing coordinates.");
  }
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  drawTime();  
  tft.fillRect(100,100,200,120,ILI9341_BLACK);
  tft.setCursor(110, 100);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("No GPS Signal");
}

//draws settings screen
void settings(){
  mode = 4;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SSettings menu.");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0,50,320,240, ILI9341_CYAN);
  tft.fillRect(5,55,310,40, ILI9341_BLACK);
  
  //Message Settigns
  tft.setCursor(10,60);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Message Settings");

  //clock settings
  tft.fillRect(5,100,310,40, ILI9341_BLACK);
  tft.setCursor(10,105);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Clock Settings");
  
  
  //General Settings
  tft.fillRect(5,145,310,40, ILI9341_BLACK);
  tft.setCursor(10,150);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("General Settings");

  //interval editor
  tft.fillRect(5,190,310,40, ILI9341_BLACK);
  tft.setCursor(50,195);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(2);
  tft.print("Breadcrumbs");
  tft.setCursor(50,212);
  tft.print("Interval(min): "); 
  tft.setCursor(220,195);
  tft.setTextSize(3);
  tft.println(eeprom.settings.breadInterval);
  tft.fillRect(10,195,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,210,30,200,30,220, ILI9341_BLACK);
  tft.fillRect(280,195,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,210,290,200,290,220, ILI9341_BLACK);

  //update time
  drawTime();
  EEPROM.put(0,eeprom);
}

//draws general settings screen
void generalSettings()
{
  mode = 19;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SGeneral Settings");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0,50,320,240, ILI9341_CYAN);
  tft.fillRect(5,55,310,40, ILI9341_BLACK);
  
  //Narrator
  tft.fillRect(5,55,310,40, ILI9341_BLACK);
  tft.setCursor(10,60);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Narrator");
  tft.fillRect(280,60,30,30,ILI9341_CYAN);
  if(eeprom.settings.narrator == true){
    tft.fillRect(285,65,20,20,ILI9341_BLACK);
  }

  //Factory Reset
  tft.fillRect(5,100,310,40, ILI9341_BLACK);
  tft.setCursor(10,105);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Factory Reset");

  //update time
  drawTime();
  EEPROM.put(0,eeprom);
}

//draws factory reset confirmation screen
void FactoryReset()
{
  mode = 20;

  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SWarning! Factory reset will delete all messages and set all settings to their defaults! Are you sure you wish to continue?");
  }

  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0,50,350,240, ILI9341_WHITE);

  tft.setCursor(120,60);
  tft.setTextColor(ILI9341_RED);  
  tft.setTextSize(2);
  tft.print("Warning!");
  tft.setCursor(10,80);
  tft.print("Factory reset will delete");
  tft.setCursor(10,100);
  tft.print("all messages and set all");
  tft.setCursor(10,120);
  tft.print("settings to their");
  tft.setCursor(10,140);
  tft.print("defaults! Are you sure");
  tft.setCursor(10,160);
  tft.print("you wish to continue?"); 

  //Factory Reset
  tft.fillRect(5,190,310,40, ILI9341_BLACK);
  tft.setCursor(10,195);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Continue");

  drawTime();  
}

//draws message settings screen
void messageSettings(){
  mode = 18;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SMessage Settings");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0,50,320,240, ILI9341_CYAN);
  tft.fillRect(5,55,310,40, ILI9341_BLACK);
  
  //Define Recipient
  tft.setCursor(10,60);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Recipient");

  //Define Preset Messages
  tft.fillRect(5,100,310,40, ILI9341_BLACK);
  tft.setCursor(10,105);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Set Preset Texts");

  //update time
  drawTime();
  EEPROM.put(0,eeprom);
}

void clockSettings()
{
  mode = 12;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SClock Settings");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0,50,320,240, ILI9341_CYAN);

  //utc offset
  tft.fillRect(5,55,310,40, ILI9341_BLACK);
  tft.setCursor(50,60);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("UTC: ");
  tft.setCursor(170,60);
  if(eeprom.settings.utcOffset > 0)
  {
    tft.print("+");
  }
  if(ceilf(eeprom.settings.utcOffset) == eeprom.settings.utcOffset)
  {
    tft.print((int)eeprom.settings.utcOffset);
    tft.print(":");
    tft.print("00");
  } else if((eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == 0.5 || (eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == -0.5)
  {
    tft.print((int)eeprom.settings.utcOffset);
    tft.print(":");
    tft.print("30");
  } else if((eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == 0.75 || (eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == -0.75)
  {
    tft.print((int)eeprom.settings.utcOffset);
    tft.print(":");
    tft.print("45");
  }
  tft.fillRect(10,60,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,75,30,65,30,85, ILI9341_BLACK);
  tft.fillRect(280,60,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,75,290,65,290,85, ILI9341_BLACK);

  //checkbox for GPS Time
  tft.fillRect(5,100,310,40, ILI9341_BLACK);
  tft.setCursor(10,105);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Use GPS Time");
  tft.fillRect(280,105,30,30,ILI9341_CYAN);
  if(eeprom.settings.gpsTime == true){
    tft.fillRect(285,110,20,20,ILI9341_BLACK);
  }

  //Time
  tft.fillRect(5,145,310,40, ILI9341_BLACK);
  tft.setCursor(10,150);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Set Time");

  //Date
  if(!eeprom.settings.gpsTime)
  {
    tft.fillRect(5,190,310,40, ILI9341_BLACK);
  } else
  {
    tft.fillRect(5,190,310,40, 0x8410);
  }
  tft.setCursor(10,195);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Set Date");
  
  drawTime();
  EEPROM.put(0,eeprom);
}

//draws time settings screen
void timeSettings()
{
  mode = 13;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SSet Time");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0,50,320,240, ILI9341_CYAN);

  //Hour
  if(!eeprom.settings.gpsTime)
  {
    tft.fillRect(5,55,310,40, ILI9341_BLACK);
  } else
  {
    tft.fillRect(5,55,310,40, 0x8410);
  }
  tft.setCursor(50,60);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Hour: ");
  tft.setCursor(230,60);
  tft.println(rtc.getHour());
  tft.fillRect(10,60,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,75,30,65,30,85, ILI9341_BLACK);
  tft.fillRect(280,60,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,75,290,65,290,85, ILI9341_BLACK);

  //Minute
  if(!eeprom.settings.gpsTime)
  {
    tft.fillRect(5,100,310,40, ILI9341_BLACK);
  } else
  {
    tft.fillRect(5,100,310,40, 0x8410);
  }
  tft.setCursor(50,105);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Minute: ");
  tft.setCursor(230,105);
  tft.println(rtc.getMinute());
  tft.fillRect(10,105,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,120,30,110,30,130, ILI9341_BLACK);
  tft.fillRect(280,105,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,120,290,110,290,130, ILI9341_BLACK);

  //Second
  if(!eeprom.settings.gpsTime)
  {
    tft.fillRect(5,145,310,40, ILI9341_BLACK);
  } else
  {
    tft.fillRect(5,145,310,40, 0x8410);
  }
  tft.setCursor(50,150);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Second: ");
  tft.setCursor(230,150);
  tft.println(rtc.getSecond());
  tft.fillRect(10,150,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,165,30,155,30,175, ILI9341_BLACK);
  tft.fillRect(280,150,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,165,290,155,290,175, ILI9341_BLACK);  

  //checkbox for 24 hour time
  tft.fillRect(5,190,310,40, ILI9341_BLACK);
  tft.setCursor(10,195);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("24 Hour Clock");
  tft.fillRect(280,195,30,30,ILI9341_CYAN);
  if(eeprom.settings.militaryTime == true){
    tft.fillRect(285,200,20,20,ILI9341_BLACK);
  }

  drawTime();
}

//draws date settings screen
void dateSettings()
{
  mode = 14;
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SSet Date");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  tft.fillRect(0,50,320,240, ILI9341_CYAN);

  //Year
  tft.fillRect(5,55,310,40, ILI9341_BLACK);
  tft.setCursor(50,60);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Year: ");
  tft.setCursor(230,60);
  tft.println(rtc.getYear());
  tft.fillRect(10,60,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,75,30,65,30,85, ILI9341_BLACK);
  tft.fillRect(280,60,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,75,290,65,290,85, ILI9341_BLACK);

  //Month
  tft.fillRect(5,100,310,40, ILI9341_BLACK);
  tft.setCursor(50,105);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Month: ");
  tft.setCursor(230,105);
  tft.println(rtc.getMonth());
  tft.fillRect(10,105,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,120,30,110,30,130, ILI9341_BLACK);
  tft.fillRect(280,105,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,120,290,110,290,130, ILI9341_BLACK);

  //Day
  tft.fillRect(5,145,310,40, ILI9341_BLACK);
  tft.setCursor(50,150);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Day: ");
  tft.setCursor(230,150);
  tft.println(rtc.getDate());
  tft.fillRect(10,150,30,30,ILI9341_CYAN);
  tft.fillTriangle(15,165,30,155,30,175, ILI9341_BLACK);
  tft.fillRect(280,150,30,30,ILI9341_CYAN);
  tft.fillTriangle(305,165,290,155,290,175, ILI9341_BLACK);  

  drawTime();
}

//draws SOS screen
void SOS(){
  mode = 5;
  Serial1.println("\nX\n");
  Serial1.println("SEmergency Protocol Activated!");
  tft.fillScreen(ILI9341_RED);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawTime();

  tft.setCursor(10, 85);
  tft.setTextColor(0x0000);
  tft.setTextSize(17);
  tft.print("SOS");
}

//Sets screen to black
void Sleep()
{
  mode = 7;
  tft.fillScreen(ILI9341_BLACK);
}

//draws screen to type messages on
void TypeMsg()
{
  isLower = true;
  isNumBoard = false;
  mode = 8;
  tft.fillScreen(ILI9341_BLACK);
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SNew Message.");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  drawTime();
  Zero(str,200);

  tft.drawBitmap(0,0,keyboardlower,320,240,ILI9341_WHITE);
}

//draws screen to enter mobile number
void TypeNum()
{
  mode = 11;
  tft.fillScreen(ILI9341_BLACK);
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SSet Recipient");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  drawTime();
  Zero(strNum,15);
  
  EEPROM.get(0,eeprom);
  tft.fillRect(0, 50, 320, 34, 0x8410);
  tft.setCursor(0,50);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);

  tft.print(eeprom.settings.recipient);

  tft.drawBitmap(0,0,numkey,320,240,ILI9341_WHITE);
}

//draws screen to select which preset message to define
void setPresetMessages()
{
  presetMessages();
  mode = 9;
}

//draws screen to type preset message
void typePresetMessages()
{
  mode = 10;
  isLower = true;
  isNumBoard = false;
  
  tft.fillScreen(ILI9341_BLACK);
  if(eeprom.settings.narrator)
  {
    Serial1.println("\nX\n");
    Serial1.println("SDefine Pre-Set Messages");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  drawTime();
  Zero(str,200);

  tft.drawBitmap(0,0,keyboardlower,320,240,ILI9341_WHITE);
}

//Draws main menu screen
void mainMenu(){
  mode = 0;
  if(eeprom.settings.narrator)
  {
    Serial1.println("X\n");
    Serial1.println("SMain menu.");
  }
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  tft.fillRect(0, 50, 160, 135, ILI9341_RED);
  tft.fillRect(160, 50, 240, 135, ILI9341_YELLOW);
  tft.fillRect(0, 145, 160, 240, ILI9341_GREEN);
  tft.fillRect(160, 145, 320, 240, ILI9341_CYAN);
  
  tft.setCursor(5, 70);
  tft.setTextColor(0x0000);
  tft.setTextSize(3);
  tft.println("Preset");
  tft.setCursor(5, 100);
  tft.print("Messages");

  tft.setCursor(165, 65);
  tft.print("Custom");
  tft.setCursor(165,100);
  tft.print("Messages");

  tft.setCursor(5,165);
  tft.print("GPS");
  tft.setCursor(5,195);
  tft.print("Coords");

  tft.setCursor(165,180);
  tft.print("Settings");
  
  drawTime();
}

//erases c string
void Zero(char * str, int len)
{
  for(int i = len-1; i >= 0; i--)
  {
    str[i] = '\0';
  }
}

//Gets GPS data and displays it if the coordinates menu is selected
//Updates rtc if gps time is enabled
void GetGpsTime()
{
  if((gpsHour*100*100)+(gpsMinute*100)+(gpsSecond) > lastTime || (gpsYear*100*100)+(gpsMonth*100)+(gpsDay) > lastDate)
      {
        if(mode == 3)
        {
          tft.fillRect(100,100,220,120,ILI9341_BLACK);
          tft.setCursor(110, 100);
          tft.print("Lat: ");
          tft.print(lat,6);
          tft.setCursor(110, 120);
          tft.print("Long: ");
          tft.println(lon,6);
          tft.setCursor(110, 140);
          tft.print(gpsDay);
          tft.print("/");
          tft.print(gpsMonth);
          tft.print("/");
          tft.print(gpsYear);
          tft.setCursor(110, 160);
          if(gpsHour < 10)
          {
            tft.print("0");
          }
          tft.print(gpsHour);
          tft.print(":");
          if(gpsMinute < 10)
          {
            tft.print("0");
          }
          tft.print(gpsMinute);
          tft.print(":");
          if(gpsSecond < 10)
          {
            tft.print("0");
          }
          tft.print(gpsSecond);
          tft.println(" UTC");
        }
        int h,m,y,mn,d;
        //calculate UTC OFFSET
        int offset = (int)eeprom.settings.utcOffset;
          h = gpsHour + offset;
        if((eeprom.settings.utcOffset - offset) == 0.5)
        {
          m = gpsMinute + 30;
          if(m > 59)
          {
            m = m - 60;
            h = h + 1;
          }
        } else if((eeprom.settings.utcOffset - offset) == -0.5)
        {
          m = gpsMinute - 30;
          if(m < 0)
          {
            m = m + 60;
            h = h - 1;
          }
        } else if((eeprom.settings.utcOffset - offset) == 0.75)
        {
          m = gpsMinute + 45;
          if(m > 59)
          {
            m = m - 60;
            h = h + 1;
          }
        } else if((eeprom.settings.utcOffset - offset) == -0.75)
        {
          m = gpsMinute - 45;
          if(m < 0)
          {
            m = m + 60;
            h = h - 1;
          }
        } else
        {
          m = gpsMinute;
        }
        if(h >= 0 && h < 24)
        {
          y = gpsYear;
          mn = gpsMonth;
          d = gpsDay;
        } else if(h < 0)  //Go back a day
        {
          h = h + 24;
          if(gpsDay == 1)
          {
            if(gpsMonth == 1)
            {
              mn = 12;
              d = 31;
              y = gpsYear - 1;
            } else if(gpsMonth == 5 || gpsMonth == 7 || gpsMonth == 10 || gpsMonth == 12)
            {
              mn = gpsMonth -1;
              d = 30;
            } else if(gpsMonth == 3)
            {
              mn = gpsMonth - 1;
              if(((gpsYear % 4) == 0))
              {
                d = 29;
              } else
              {
                d = 28;
              }
            } else
            {
              mn = gpsMonth - 1;
              d = 31;
            }
          } else
          {
            d = gpsDay - 1;
          }
          
        } else if(h >= 24)
        {
          h = h - 24;
          if(gpsMonth == 4 || gpsMonth == 6 || gpsMonth == 9 || gpsMonth == 11)
          {
            if(gpsDay == 30)
            {
              d = 1;
              mn = gpsMonth + 1;
            }
            else
            {
              d = gpsDay + 1;
            }
          } else if(gpsMonth == 2)
          {
            if(((gpsYear % 4) == 0))
            {
              if(gpsDay == 29)
              {
                mn = gpsMonth + 1;
                d = 1;
              }
              else
              {
                d = gpsDay + 1;
              }
            } else
            {
              if(gpsDay == 28)
              {
                mn = gpsMonth + 1;
                d = 1;
              }
              else
              {
                d = gpsDay + 1;
              }
            }
          } else
          {
            if(gpsDay == 31)
            {
              if(gpsMonth == 12)
              {
                y = gpsYear + 1;
                mn = 1;
                d = 1;
              } else
              {
                mn = gpsMonth + 1;
                d = 1;
              }
            } else
            {
              d = gpsDay + 1;
            }
          }       
        }
        if(mode == 3)
        {
          tft.setCursor(110, 180);
          tft.print(d);
          tft.print("/");
          tft.print(mn);
          tft.print("/");
          tft.print(y);
          tft.setCursor(110, 200);
          if(h < 10)
          {
            tft.print("0");
          }
          tft.print(h);
          tft.print(":");
          if(m < 10)
          {
            tft.print("0");
          }
          tft.print(m);
          tft.print(":");
          if(gpsSecond < 10)
          {
            tft.print("0");
          }
          tft.print(gpsSecond);
          tft.print(" UTC");
          if(offset > 0)
          {
            tft.print("+");
          }
          if(ceilf(eeprom.settings.utcOffset) == eeprom.settings.utcOffset)
          {
            tft.print((int)eeprom.settings.utcOffset);
            tft.print(":");
            tft.print("00");
          } else if((eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == 0.5 || (eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == -0.5)
          {
            tft.print((int)eeprom.settings.utcOffset);
            tft.print(":");
            tft.print("30");
          } else if((eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == 0.75 || (eeprom.settings.utcOffset - (int)eeprom.settings.utcOffset) == -0.75)
          {
            tft.print((int)eeprom.settings.utcOffset);
            tft.print(":");
            tft.print("45");
          }
        }
        
        if(eeprom.settings.gpsTime)
        {
          int oldH = rtc.getHour();
          int oldM = rtc.getMinute();          
          rtc.setHour(h);
          rtc.setMinute(m);
          rtc.setSecond(gpsSecond);
          rtc.setDate(d);
          rtc.setMonth(mn);
          rtc.setYear(y-2000);
          if(oldH != h || oldM != m)
          {
            drawTime();
          }
        }
        lastTime = (gpsHour*100*100)+(gpsMinute*100)+(gpsSecond);
        lastDate = (gpsYear*100*100)+(gpsMonth*100)+(gpsDay);
      }
}

//Writes defaults to eeprom
void SetupEeprom()
{
  strcpy(eeprom.settings.predefinedMessages[0], "All is going well.");
  strcpy(eeprom.settings.predefinedMessages[1], "Setting up camp.");
  strcpy(eeprom.settings.predefinedMessages[2], "Going for a hike.");
  strcpy(eeprom.settings.predefinedMessages[3], "Injured, need help.");
  eeprom.settings.breadInterval = 1;
  eeprom.settings.currentInterval = 1;
  eeprom.settings.militaryTime = false;
  strcpy(eeprom.settings.recipient, "+61432123456");
  eeprom.settings.utcOffset = 8;
  eeprom.settings.currentOffset = 27;
  eeprom.settings.gpsTime = false;
  eeprom.settings.narrator = true;

  for(int i = 0; i < 4; i++)
  {
    strcpy(eeprom.outbox[i].recipient,"+61400000000");
    eeprom.outbox[i].coords.dateTime.year = 10 ;
    eeprom.outbox[i].coords.dateTime.month = 1;
    eeprom.outbox[i].coords.dateTime.day = 1;
    eeprom.outbox[i].coords.dateTime.hour = 12;
    eeprom.outbox[i].coords.dateTime.minute = 0;
    eeprom.outbox[i].coords.dateTime.second = 0;
    eeprom.outbox[i].coords.longitude = 0;
    eeprom.outbox[i].coords.latitude = 0;
    strcpy(eeprom.outbox[i].content,"No Messages.");

    strcpy(eeprom.inbox[i].sender,"+61400000000");
    eeprom.inbox[i].coords.dateTime.year = 10;
    eeprom.inbox[i].coords.dateTime.month = 1;
    eeprom.inbox[i].coords.dateTime.day = 1;
    eeprom.inbox[i].coords.dateTime.hour = 12;
    eeprom.inbox[i].coords.dateTime.minute = 0;
    eeprom.inbox[i].coords.dateTime.second = 0;
    eeprom.outbox[i].coords.longitude = 0;
    eeprom.outbox[i].coords.latitude = 0;
    strcpy(eeprom.inbox[i].content,"No Messages.");
  }
  EEPROM.put(0,eeprom);
}


//draws compass bearing to screen
void drawBearing(){
  sensors_event_t event; 
  mag.getEvent(&event);
      
  float Pi = 3.14159;
      
  // Calculate the angle of the vector y,x
  float heading = (atan2(event.magnetic.y,event.magnetic.x) * 180) / Pi;
      
  // Normalize to 0-360
  if (heading < 0)
  {
    heading = 360 + heading;
  }
  tft.fillRect(100,75,180,25,ILI9341_BLACK);
  tft.print(heading);
}

//Handles recieved messages and saves them to eeprom
void ReceiveMessage(String txt)
{
  EEPROM.get(0,eeprom);
  Serial1.println("\nX\n");
  Serial1.println("SMessage Received");    
  pushInbox();
  
  char delim[] = "|";
  int count = 0;
  char *str = &(txt[0]);
  char *tkn = strtok(str, delim);
  
  while(tkn != NULL)
  {
    if(count == 0)
    {
      strcpy(eeprom.inbox[0].sender,tkn);
      count++;
    } else if(count == 1)
    {
      strcpy(eeprom.inbox[0].content,tkn);
    }
    tkn = strtok(NULL,delim);
  }

  eeprom.inbox[0].coords.dateTime.year = rtc.year();
  eeprom.inbox[0].coords.dateTime.month = rtc.month();
  eeprom.inbox[0].coords.dateTime.day = rtc.date();
  eeprom.inbox[0].coords.dateTime.hour = rtc.hour();
  eeprom.inbox[0].coords.dateTime.minute = rtc.minute();
  eeprom.inbox[0].coords.dateTime.second = rtc.second();
  
  EEPROM.put(0,eeprom);
}

//sends preset message based on integer code
//1-4 are the preset messages
//5 is the SOS message
void SendMessage(int opt)
{
  Serial.print(eeprom.settings.recipient);
  Serial.print("|");
  Serial.print(rtc.year());
  Serial.print("/");
  Serial.print(rtc.month());
  Serial.print("/");
  Serial.print(rtc.date());
  Serial.print("|");
  if(rtc.hour() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.hour());
  Serial.print(":");
  if(rtc.minute() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.minute());
  Serial.print(":");
  if(rtc.second() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.second());
  Serial.print("|");
  Serial.print(eeprom.settings.utcOffset);
  Serial.print("|");
  
  Serial.print(lat,6);
  Serial.print(",");
  Serial.print(lon,6);
  Serial.print("|");
  Serial.print(gpsYear);
  Serial.print("/");
  Serial.print(gpsMonth);
  Serial.print("/");
  Serial.print(gpsDay);
  Serial.print("|");
  if(gpsHour < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsHour);
  Serial.print(":");
  if(gpsMinute < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsMinute);
  Serial.print(":");
  if(gpsSecond < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsSecond);
  Serial.print("|");

  pushOutbox();
  
  if(opt == 1)
  {    
    Serial.print(eeprom.settings.predefinedMessages[0]);
    strcpy(eeprom.outbox[0].recipient,eeprom.settings.recipient);
    eeprom.outbox[0].coords.dateTime.year = rtc.year();
    eeprom.outbox[0].coords.dateTime.month = rtc.month();
    eeprom.outbox[0].coords.dateTime.day = rtc.date();
    eeprom.outbox[0].coords.dateTime.hour = rtc.hour();
    eeprom.outbox[0].coords.dateTime.minute = rtc.minute();
    eeprom.outbox[0].coords.dateTime.second = rtc.second();
    
    eeprom.outbox[0].coords.gpsDateTime.year = gpsYear;
    eeprom.outbox[0].coords.gpsDateTime.month = gpsMonth;
    eeprom.outbox[0].coords.gpsDateTime.day = gpsDay;
    eeprom.outbox[0].coords.gpsDateTime.hour = gpsHour;
    eeprom.outbox[0].coords.gpsDateTime.minute = gpsMinute;
    eeprom.outbox[0].coords.gpsDateTime.second = gpsSecond;
    
    eeprom.outbox[0].coords.longitude = lon;
    eeprom.outbox[0].coords.latitude = lat;
    strcpy(eeprom.outbox[0].content,eeprom.settings.predefinedMessages[0]);
  } else if(opt == 2)
  {
    Serial.print(eeprom.settings.predefinedMessages[1]);
    strcpy(eeprom.outbox[0].recipient,eeprom.settings.recipient);
    eeprom.outbox[0].coords.dateTime.year = rtc.year();
    eeprom.outbox[0].coords.dateTime.month = rtc.month();
    eeprom.outbox[0].coords.dateTime.day = rtc.date();
    eeprom.outbox[0].coords.dateTime.hour = rtc.hour();
    eeprom.outbox[0].coords.dateTime.minute = rtc.minute();
    eeprom.outbox[0].coords.dateTime.second = rtc.second();
    
    eeprom.outbox[0].coords.gpsDateTime.year = gpsYear;
    eeprom.outbox[0].coords.gpsDateTime.month = gpsMonth;
    eeprom.outbox[0].coords.gpsDateTime.day = gpsDay;
    eeprom.outbox[0].coords.gpsDateTime.hour = gpsHour;
    eeprom.outbox[0].coords.gpsDateTime.minute = gpsMinute;
    eeprom.outbox[0].coords.gpsDateTime.second = gpsSecond;
    
    eeprom.outbox[0].coords.longitude = lon;
    eeprom.outbox[0].coords.latitude = lat;
    strcpy(eeprom.outbox[0].content,eeprom.settings.predefinedMessages[1]);
  } else if(opt == 3)
  {
    Serial.print(eeprom.settings.predefinedMessages[2]);
    strcpy(eeprom.outbox[0].recipient,eeprom.settings.recipient);
    eeprom.outbox[0].coords.dateTime.year = rtc.year();
    eeprom.outbox[0].coords.dateTime.month = rtc.month();
    eeprom.outbox[0].coords.dateTime.day = rtc.date();
    eeprom.outbox[0].coords.dateTime.hour = rtc.hour();
    eeprom.outbox[0].coords.dateTime.minute = rtc.minute();
    eeprom.outbox[0].coords.dateTime.second = rtc.second();
    
    eeprom.outbox[0].coords.gpsDateTime.year = gpsYear;
    eeprom.outbox[0].coords.gpsDateTime.month = gpsMonth;
    eeprom.outbox[0].coords.gpsDateTime.day = gpsDay;
    eeprom.outbox[0].coords.gpsDateTime.hour = gpsHour;
    eeprom.outbox[0].coords.gpsDateTime.minute = gpsMinute;
    eeprom.outbox[0].coords.gpsDateTime.second = gpsSecond;
    
    eeprom.outbox[0].coords.longitude = lon;
    eeprom.outbox[0].coords.latitude = lat;
    strcpy(eeprom.outbox[0].content,eeprom.settings.predefinedMessages[2]);
  } else if(opt == 4)
  {
    Serial.print(eeprom.settings.predefinedMessages[3]);
    strcpy(eeprom.outbox[0].recipient,eeprom.settings.recipient);
    eeprom.outbox[0].coords.dateTime.year = rtc.year();
    eeprom.outbox[0].coords.dateTime.month = rtc.month();
    eeprom.outbox[0].coords.dateTime.day = rtc.date();
    eeprom.outbox[0].coords.dateTime.hour = rtc.hour();
    eeprom.outbox[0].coords.dateTime.minute = rtc.minute();
    eeprom.outbox[0].coords.dateTime.second = rtc.second();
    
    eeprom.outbox[0].coords.gpsDateTime.year = gpsYear;
    eeprom.outbox[0].coords.gpsDateTime.month = gpsMonth;
    eeprom.outbox[0].coords.gpsDateTime.day = gpsDay;
    eeprom.outbox[0].coords.gpsDateTime.hour = gpsHour;
    eeprom.outbox[0].coords.gpsDateTime.minute = gpsMinute;
    eeprom.outbox[0].coords.gpsDateTime.second = gpsSecond;
    
    eeprom.outbox[0].coords.longitude = lon;
    eeprom.outbox[0].coords.latitude = lat;
    strcpy(eeprom.outbox[0].content,eeprom.settings.predefinedMessages[3]);
  } else if(opt == 5)
  {
    Serial.print("SOS - SEND HELP!");
    strcpy(eeprom.outbox[0].recipient,eeprom.settings.recipient);
    eeprom.outbox[0].coords.dateTime.year = rtc.year();
    eeprom.outbox[0].coords.dateTime.month = rtc.month();
    eeprom.outbox[0].coords.dateTime.day = rtc.date();
    eeprom.outbox[0].coords.dateTime.hour = rtc.hour();
    eeprom.outbox[0].coords.dateTime.minute = rtc.minute();
    eeprom.outbox[0].coords.dateTime.second = rtc.second();
    
    eeprom.outbox[0].coords.gpsDateTime.year = gpsYear;
    eeprom.outbox[0].coords.gpsDateTime.month = gpsMonth;
    eeprom.outbox[0].coords.gpsDateTime.day = gpsDay;
    eeprom.outbox[0].coords.gpsDateTime.hour = gpsHour;
    eeprom.outbox[0].coords.gpsDateTime.minute = gpsMinute;
    eeprom.outbox[0].coords.gpsDateTime.second = gpsSecond;
    
    eeprom.outbox[0].coords.longitude = lon;
    eeprom.outbox[0].coords.latitude = lat;
    strcpy(eeprom.outbox[0].content,"SOS - SEND HELP!");
  }
  Serial.print("\n");
  tft.setCursor(52, 10);
  tft.setTextColor(0x0000);
  tft.setTextSize(4);
  tft.print("SENT");
  if(eeprom.settings.narrator)
  {
    Serial1.println("SMessage Sent");
  }
  delay(1000);
  tft.fillRect(52, 0, 92, 50, ILI9341_WHITE);
  EEPROM.put(0,eeprom);
}

//Sends custom message taking string of message as input
void SendMessage(char * str)
{
  Serial.print(eeprom.settings.recipient);
  Serial.print("|");
  Serial.print(rtc.year());
  Serial.print("/");
  Serial.print(rtc.month());
  Serial.print("/");
  Serial.print(rtc.date());
  Serial.print("|");
  if(rtc.hour() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.hour());
  Serial.print(":");
  if(rtc.minute() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.minute());
  Serial.print(":");
  if(rtc.second() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.second());
  Serial.print("|");
  Serial.print(eeprom.settings.utcOffset);
  Serial.print("|");
  
  Serial.print(lat,6);
  Serial.print(",");
  Serial.print(lon,6);
  Serial.print("|");
  Serial.print(gpsYear);
  Serial.print("/");
  Serial.print(gpsMonth);
  Serial.print("/");
  Serial.print(gpsDay);
  Serial.print("|");
  if(gpsHour < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsHour);
  Serial.print(":");
  if(gpsMinute < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsMinute);
  Serial.print(":");
  if(gpsSecond < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsSecond);
  Serial.print("|");
  
  Serial.print(str);
  
  pushOutbox();
  
  strcpy(eeprom.outbox[0].recipient,eeprom.settings.recipient);
  eeprom.outbox[0].coords.dateTime.year = rtc.year();
  eeprom.outbox[0].coords.dateTime.month = rtc.month();
  eeprom.outbox[0].coords.dateTime.day = rtc.date();
  eeprom.outbox[0].coords.dateTime.hour = rtc.hour();
  eeprom.outbox[0].coords.dateTime.minute = rtc.minute();
  eeprom.outbox[0].coords.dateTime.second = rtc.second();
    
  eeprom.outbox[0].coords.gpsDateTime.year = gpsYear;
  eeprom.outbox[0].coords.gpsDateTime.month = gpsMonth;
  eeprom.outbox[0].coords.gpsDateTime.day = gpsDay;
  eeprom.outbox[0].coords.gpsDateTime.hour = gpsHour;
  eeprom.outbox[0].coords.gpsDateTime.minute = gpsMinute;
  eeprom.outbox[0].coords.gpsDateTime.second = gpsSecond;
    
  eeprom.outbox[0].coords.longitude = lon;
  eeprom.outbox[0].coords.latitude = lat;
  strcpy(eeprom.outbox[0].content,str);
    
  Serial.print("\n");
  tft.setCursor(52, 10);
  tft.setTextColor(0x0000);
  tft.setTextSize(4);
  tft.print("SENT");
  if(eeprom.settings.narrator)
  {
    Serial1.println("SMessage Sent");
  }
  delay(1000);
  tft.fillRect(52, 0, 92, 50, ILI9341_WHITE);
  EEPROM.put(0,eeprom);
}

//Sends breadcrumb of current location
//This message is not saved to the eeprom
void SendBread()
{
  Serial.print(eeprom.settings.recipient);
  Serial.print("|");
  Serial.print(rtc.year());
  Serial.print("/");
  Serial.print(rtc.month());
  Serial.print("/");
  Serial.print(rtc.date());
  Serial.print("|");
  if(rtc.hour() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.hour());
  Serial.print(":");
  if(rtc.minute() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.minute());
  Serial.print(":");
  if(rtc.second() < 10)
  {
    Serial.print("0");
  }
  Serial.print(rtc.second());
  Serial.print("|");
  Serial.print(eeprom.settings.utcOffset);
  Serial.print("|");
  
  Serial.print(lat,6);
  Serial.print(",");
  Serial.print(lon,6);
  Serial.print("|");
  Serial.print(gpsYear);
  Serial.print("/");
  Serial.print(gpsMonth);
  Serial.print("/");
  Serial.print(gpsDay);
  Serial.print("|");
  if(gpsHour < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsHour);
  Serial.print(":");
  if(gpsMinute < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsMinute);
  Serial.print(":");
  if(gpsSecond < 10)
  {
    Serial.print("0");
  }
  Serial.print(gpsSecond);
  Serial.print("|");
  
  Serial.println("Current Location");
  
}

//Pushes messages in outbox down one deleting the oldest message
//to be called before writing new message to outbox
void pushOutbox()
{
  EEPROM.get(0,eeprom);
  for(int i = 3; i > 0; i--)
  {
    strcpy(eeprom.outbox[i].recipient,eeprom.outbox[i-1].recipient);
    eeprom.outbox[i].coords.dateTime.year = eeprom.outbox[i-1].coords.dateTime.year;
    eeprom.outbox[i].coords.dateTime.month = eeprom.outbox[i-1].coords.dateTime.month;
    eeprom.outbox[i].coords.dateTime.day = eeprom.outbox[i-1].coords.dateTime.day;
    eeprom.outbox[i].coords.dateTime.hour = eeprom.outbox[i-1].coords.dateTime.hour;
    eeprom.outbox[i].coords.dateTime.minute = eeprom.outbox[i-1].coords.dateTime.minute;
    eeprom.outbox[i].coords.dateTime.second = eeprom.outbox[i-1].coords.dateTime.second;
  
    eeprom.outbox[i].coords.gpsDateTime.year = eeprom.outbox[i-1].coords.gpsDateTime.year;
    eeprom.outbox[i].coords.gpsDateTime.month = eeprom.outbox[i-1].coords.gpsDateTime.month;
    eeprom.outbox[i].coords.gpsDateTime.day = eeprom.outbox[i-1].coords.gpsDateTime.day;
    eeprom.outbox[i].coords.gpsDateTime.hour = eeprom.outbox[i-1].coords.gpsDateTime.hour;
    eeprom.outbox[i].coords.gpsDateTime.minute = eeprom.outbox[i-1].coords.gpsDateTime.minute;
    eeprom.outbox[i].coords.gpsDateTime.second = eeprom.outbox[i-1].coords.gpsDateTime.second;
  
    eeprom.outbox[i].coords.longitude = eeprom.outbox[i-1].coords.longitude;
    eeprom.outbox[i].coords.latitude = eeprom.outbox[i-1].coords.latitude;
    strcpy(eeprom.outbox[i].content,eeprom.outbox[i-1].content);
  }
  EEPROM.put(0,eeprom);
}

//Pushes messages in inbox down one deleting the oldest message
//to be called before writing new message to inbox
void pushInbox()
{
  EEPROM.get(0,eeprom);
  for(int i = 3; i > 0; i--)
  {
    strcpy(eeprom.inbox[i].sender,eeprom.inbox[i-1].sender);
    eeprom.inbox[i].coords.dateTime.year = eeprom.inbox[i-1].coords.dateTime.year;
    eeprom.inbox[i].coords.dateTime.month = eeprom.inbox[i-1].coords.dateTime.month;
    eeprom.inbox[i].coords.dateTime.day = eeprom.inbox[i-1].coords.dateTime.day;
    eeprom.inbox[i].coords.dateTime.hour = eeprom.inbox[i-1].coords.dateTime.hour;
    eeprom.inbox[i].coords.dateTime.minute = eeprom.inbox[i-1].coords.dateTime.minute;
    eeprom.inbox[i].coords.dateTime.second = eeprom.inbox[i-1].coords.dateTime.second;
  
    eeprom.inbox[i].coords.gpsDateTime.year = eeprom.inbox[i-1].coords.gpsDateTime.year;
    eeprom.inbox[i].coords.gpsDateTime.month = eeprom.inbox[i-1].coords.gpsDateTime.month;
    eeprom.inbox[i].coords.gpsDateTime.day = eeprom.inbox[i-1].coords.gpsDateTime.day;
    eeprom.inbox[i].coords.gpsDateTime.hour = eeprom.inbox[i-1].coords.gpsDateTime.hour;
    eeprom.inbox[i].coords.gpsDateTime.minute = eeprom.inbox[i-1].coords.gpsDateTime.minute;
    eeprom.inbox[i].coords.gpsDateTime.second = eeprom.inbox[i-1].coords.gpsDateTime.second;
  
    eeprom.inbox[i].coords.longitude = eeprom.inbox[i-1].coords.longitude;
    eeprom.inbox[i].coords.latitude = eeprom.inbox[i-1].coords.latitude;
    strcpy(eeprom.inbox[i].content,eeprom.inbox[i-1].content);
  }
  EEPROM.put(0,eeprom);
}

//Draws a "back button" on the screen for navigation
void drawBack()
{
  tft.fillRect(0, 0, 50, 50, ILI9341_BLACK);
  tft.setCursor(3,7);
  tft.setTextSize(4);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("<");
  tft.setCursor(15,7);
  tft.setTextSize(4);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("-");
}

//draws a clock on the screen
void drawTime() {
  rtc.update();
  tft.fillRect(150, 0, 320, 50, ILI9341_WHITE);
  
  if(eeprom.settings.militaryTime == false)
  {
    if(rtc.hour() >= 12)
    {
      if((rtc.hour() - 12) > 9 || rtc.hour() == 12 )
      {
        tft.setCursor(150, 10);
        tft.setTextColor(0x0000);
        tft.setTextSize(4);
      } else
      {
        tft.setCursor(170, 10);
        tft.setTextColor(0x0000);
        tft.setTextSize(4);
      }
      if(rtc.hour() > 12)
      {
        tft.print(rtc.hour() - 12);
      } else
      {
        tft.print(rtc.hour());
      }
      
      tft.print(":");
      if(rtc.minute() < 10) 
        tft.print("0");
      tft.print(rtc.minute());

      tft.print("PM");
    } else
    {
      if(rtc.hour() > 9 || rtc.hour() == 0)
      {
        tft.setCursor(150, 10);
        tft.setTextColor(0x0000);
        tft.setTextSize(4);        
      } else
      {
        tft.setCursor(170, 10);
        tft.setTextColor(0x0000);
        tft.setTextSize(4);
      }
      if(rtc.hour() == 0)
      {
        tft.print(rtc.hour() + 12);
      } else
      {
        tft.print(rtc.hour());
      }
      
      tft.print(":");
      if(rtc.minute() < 10) 
        tft.print("0");
      tft.print(rtc.minute());

      tft.print("AM");
    }
  } else
  {
    tft.setCursor(200, 10);
    tft.setTextColor(0x0000);
    tft.setTextSize(4);
    tft.print(rtc.hour());
    tft.print(":");
    if(rtc.minute() < 10) 
      tft.print("0");
    tft.print(rtc.minute());
  }  
}

//used for input of number keyboard
int DrawNumkey(char * str)
{
  int count = strlen(str);
  if(count < 15)
  {
    if(touchPoint[0] <= 55 && touchPoint[1] <= 148 && touchPoint[1] >= 86)
    {
      //1
      str[count] = '1';
      count++;
    } else if(touchPoint[0] <= 110 && touchPoint[1] <= 148 && touchPoint[1] >= 86)
    {
      //2
      str[count] = '2';
      count++;
    } else if(touchPoint[0] <= 165 && touchPoint[1] <= 148 && touchPoint[1] >= 86)
    {
      //3
      str[count] = '3';
      count++;
    } else if(touchPoint[0] <= 220 && touchPoint[1] <= 148 && touchPoint[1] >= 86)
    {
      //4
      str[count] = '4';
      count++;
    } else if(touchPoint[0] <= 275 && touchPoint[1] <= 148 && touchPoint[1] >= 86)
    {
      //5
      str[count] = '5';
      count++;
    } else if(touchPoint[1] <= 148 && touchPoint[1] >= 86)
    {
      //+
      if(count == 0)
      {
        str[count] = '+';
        count++;
      }
    }
    
    else if(touchPoint[0] <= 55 && touchPoint[1] >= 162)
    {
      //6
      str[count] = '6';
      count++;
    } else if(touchPoint[0] <= 110 && touchPoint[1] >= 162)
    {
      //7
      str[count] = '7';
      count++;
    } else if(touchPoint[0] <= 165 && touchPoint[1] >= 162)
    {
      //8
      str[count] = '8';
      count++;
    } else if(touchPoint[0] <= 220 && touchPoint[1] >= 162)
    {
      //9
      str[count] = '9';
      count++;
    } else if(touchPoint[0] <= 275 && touchPoint[1] >= 162)
    {
      //0
      str[count] = '0';
      count++;
    } 
  }
  if(touchPoint[0] >= 280 && touchPoint[1] <= 190 && touchPoint[1] >= 162)
  {
    //<--
    count--;
    str[count] = '\0';
  } else if(touchPoint[0] >= 280 && touchPoint[1] >= 196)
  {
    //Send
    count = -1;
  } 
  delay(200);
  return count;
}

//used for input of keyboard
int DrawKeyboard(char * str)
{
  int count = strlen(str);
  if(touchPoint[0] <= 33 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //Q
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'q';
        count++;
      } else
      {
        str[count] = 'Q';
        count++;
      }
    } else
    {
      str[count] = '1';
      count++;
    }
  } else if (touchPoint[0] <= 64 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //W
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'w';
        count++;
      } else
      {
        str[count] = 'W';
        count++;
      }
    } else
    {
      str[count] = '2';
      count++;
    }
  } else if (touchPoint[0] <= 96 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //E
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'e';
        count++;
      } else
      {
        str[count] = 'E';
        count++;
      }
    } else
    {
      str[count] = '3';
      count++;
    }
  } else if (touchPoint[0] <= 128 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //R
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'r';
        count++;
      } else
      {
        str[count] = 'R';
        count++;
      }
    } else
    {
      str[count] = '4';
      count++;
    }
  } else if (touchPoint[0] <= 160 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //T
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 't';
        count++;
      } else
      {
        str[count] = 'T';
        count++;
      }
    } else
    {
      str[count] = '5';
      count++;
    }
  } else if (touchPoint[0] <= 192 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //Y
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'y';
        count++;
      } else
      {
        str[count] = 'Y';
        count++;
      }
    } else
    {
      str[count] = '6';
      count++;
    }
  } else if (touchPoint[0] <= 224 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //U
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'u';
        count++;
      } else
      {
        str[count] = 'U';
        count++;
      }
    } else
    {
      str[count] = '7';
      count++;
    }
  } else if (touchPoint[0] <= 256 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //I
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'i';
        count++;
      } else
      {
        str[count] = 'I';
        count++;
      }
    } else
    {
      str[count] = '8';
      count++;
    }
  } else if (touchPoint[0] <= 288 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //O
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'o';
        count++;
      } else
      {
        str[count] = 'O';
        count++;
      }
    } else
    {
      str[count] = '9';
      count++;
    }
  } else if (touchPoint[0] <= 320 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
  {
    //P
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'p';
        count++;
      } else
      {
        str[count] = 'P';
        count++;
      }
    } else
    {
      str[count] = '0';
      count++;
    }
  } 
  
  else if (touchPoint[0] <= 47 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //A
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'a';
        count++;
      } else
      {
        str[count] = 'A';
        count++;
      }
    } else
    {
      str[count] = '!';
      count++;
    }
  } else if (touchPoint[0] <= 79 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //S
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 's';
        count++;
      } else
      {
        str[count] = 'S';
        count++;
      }
    } else
    {
      str[count] = '@';
      count++;
    }
  } else if (touchPoint[0] <= 111 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //D
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'd';
        count++;
      } else
      {
        str[count] = 'D';
        count++;
      }
    } else
    {
      str[count] = '#';
      count++;
    }
  } else if (touchPoint[0] <= 143 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //F
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'f';
        count++;
      } else
      {
        str[count] = 'F';
        count++;
      }
    } else
    {
      str[count] = '%';
      count++;
    }
  } else if (touchPoint[0] <= 175 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //G
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'g';
        count++;
      } else
      {
        str[count] = 'G';
        count++;
      }
    } else
    {
      str[count] = '$';
      count++;
    }
  } else if (touchPoint[0] <= 207 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //H
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'h';
        count++;
      } else
      {
        str[count] = 'H';
        count++;
      }
    } else
    {
      str[count] = '^';
      count++;
    }
  } else if (touchPoint[0] <= 239 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //J
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'j';
        count++;
      } else
      {
        str[count] = 'J';
        count++;
      }
    } else
    {
      str[count] = '&';
      count++;
    }
  } else if (touchPoint[0] <= 271 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //K
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'k';
        count++;
      } else
      {
        str[count] = 'K';
        count++;
      }
    } else
    {
      str[count] = '*';
      count++;
    }
  } else if (touchPoint[0] <= 320 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
  {
    //L
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'l';
        count++;
      } else
      {
        str[count] = 'L';
        count++;
      }
    } else
    {
      str[count] = '/';
      count++;
    }
  } 
  
  else if (touchPoint[0] <= 32 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //^
    if(!isNumBoard)
    {
      if(isLower)
      {
        tft.fillRect(0,50,320,240,ILI9341_BLACK);
        tft.drawBitmap(0,0,keyboard,320,240,ILI9341_WHITE);
        isLower = false;
      } else
      {
        tft.fillRect(0,50,320,240,ILI9341_BLACK);
        tft.drawBitmap(0,0,keyboardlower,320,240,ILI9341_WHITE);
        isLower = true;
      }
    }
  } else if (touchPoint[0] <= 64 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //Z
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'z';
        count++;
      } else
      {
        str[count] = 'Z';
        count++;
      }
    } else
    {
      str[count] = ':';
      count++;
    }
  } else if (touchPoint[0] <= 96 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //X
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'x';
        count++;
      } else
      {
        str[count] = 'X';
        count++;
      }
    } else
    {
      str[count] = ';';
      count++;
    }
  } else if (touchPoint[0] <= 128 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //C
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'c';
        count++;
      } else
      {
        str[count] = 'C';
        count++;
      }
    } else
    {
      str[count] = '\'';
      count++;
    }
  } else if (touchPoint[0] <= 160 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //V
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'v';
        count++;
      } else
      {
        str[count] = 'V';
        count++;
      }
    } else
    {
      str[count] = '\"';
      count++;
    }
  } else if (touchPoint[0] <= 192 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //B
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'b';
        count++;
      } else
      {
        str[count] = 'B';
        count++;
      }
    } else
    {
      str[count] = '-';
      count++;
    }
  } else if (touchPoint[0] <= 224 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //N
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'n';
        count++;
      } else
      {
        str[count] = 'N';
        count++;
      }
    } else
    {
      str[count] = '(';
      count++;
    }
  } else if (touchPoint[0] <= 256 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //M
    if(!isNumBoard)
    {
      if(isLower)
      {
        str[count] = 'm';
        count++;
      } else
      {
        str[count] = 'M';
        count++;
      }
    } else
    {
      str[count] = ')';
      count++;
    }
  } else if (touchPoint[0] <= 320 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
  {
    //<--
    count--;
    str[count] = '\0';
  } 
  
  else if (touchPoint[0] <= 32 && touchPoint[1] >= 201)
  {
    //Num
    if(!isNumBoard)
    {
      tft.fillRect(0,50,320,240,ILI9341_BLACK);
      tft.drawBitmap(0,0,numpad,320,240,ILI9341_WHITE);
      isNumBoard = true;
    } else
    {
      tft.fillRect(0,50,320,240,ILI9341_BLACK);
      if(isLower)
      {
        tft.drawBitmap(0,0,keyboardlower,320,240,ILI9341_WHITE);
      } else 
      {
        tft.drawBitmap(0,0,keyboard,320,240,ILI9341_WHITE);
      }
      isNumBoard = false;
    }
  } else if (touchPoint[0] <= 64 && touchPoint[1] >= 201)
  {
    //?
    if(!isNumBoard)
    {
      str[count] = '?';
      count++;
    } else
    {
      str[count] = '_';
      count++;
    }
  } else if (touchPoint[0] <= 96 && touchPoint[1] >= 201)
  {
    //,
    str[count] = ',';
    count++;
  } else if (touchPoint[0] <= 224 && touchPoint[1] >= 201)
  {
    //Space
    str[count] = ' ';
    count++;
  } else if (touchPoint[0] <= 256 && touchPoint[1] >= 201)
  {
    //.
    str[count] = '.';
    count++;
  } else if (touchPoint[0] <= 320 && touchPoint[1] >= 201)
  {
    //Send
    count = 0;
    return -1;
  }
  delay(200);
  return count;
}
