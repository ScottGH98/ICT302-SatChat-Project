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
  double longitutde;
  double latitude;
  struct DateTime dateTime;
};
struct Message
{
  char recipient[13];
  char sender[13];
  char content[200];
  struct GPS coords; 
};
struct Settings
{
  char predefinedMessages[4][200];
  bool breadcrumbs;
  short breadInterval;
  bool militaryTime;
  char recipient[13];
};
struct Eeprom
{
  struct Message inbox;
  struct Message outbox;
  struct Settings settings; 
}eeprom;

bool sleeping = false;
bool sosing = false;
bool isNumBoard = false;
bool isLower = true;

byte factorySettings;

void setup(void) { 
  pinMode(2,INPUT_PULLUP);
  pinMode(3,INPUT_PULLUP);
  
  Serial.begin(9600,SERIAL_8N1);
  Serial.println("Device initiallised");
  Serial1.begin(9600,SERIAL_8N1);
  Serial2.begin(9600,SERIAL_8N1);
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
  //rtc.setTime(0, 40, 2, 5, 26, 9, 19);
  

  

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
 
  if(mode == 0) {
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
    
  }else if(mode == 1) {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] > 160 && touchPoint[1] > 145){
        SendMessage(4);
      } else if(touchPoint[0] < 160 && touchPoint[1] > 145){
        SendMessage(3);
        Serial1.println("SMessage Sent");
        delay(1000);
      }else if(touchPoint[0] > 160 && touchPoint[1] > 50){
        SendMessage(2);
        Serial1.println("SMessage Sent");
        delay(1000);
      }else if(touchPoint[0] < 160 && touchPoint[1] > 50){
        SendMessage(1);
        Serial1.println("SMessage Sent");
        delay(1000);
      }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
        EEPROM.put(0,eeprom);
        mainMenu();
      }
    }
    
  }else if(mode == 2) {
    EEPROM.get(0,eeprom);
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[1] > 145){
          tft.fillRect(0, 50, 320, 240, ILI9341_CYAN);
          tft.setCursor(5, 55);
          tft.setTextColor(0x0000);
          tft.setTextSize(1);
          tft.print("Inbox ");
          tft.print(eeprom.inbox.sender);
          tft.print("  ");
          tft.print(eeprom.inbox.coords.dateTime.hour);
          tft.print(":");
          tft.print(eeprom.inbox.coords.dateTime.minute);
          tft.print(":");
          tft.print(eeprom.inbox.coords.dateTime.second);
          tft.print("  ");
          tft.print(eeprom.inbox.coords.dateTime.day);
          tft.print("/");
          tft.print(eeprom.inbox.coords.dateTime.month);
          tft.print("/");
          tft.print(eeprom.inbox.coords.dateTime.year);
          //TODO GPS COORDS
          tft.print("\n");
          tft.setTextSize(2);
          textWrap(eeprom.inbox.content,26,10);
          mode = 6;
        }else if(touchPoint[1] > 50){
          tft.fillRect(0, 50, 320, 240, ILI9341_GREEN);
          tft.setCursor(5, 55);
          tft.setTextColor(0x0000);
          tft.setTextSize(1);
          tft.print("Outbox ");
          tft.print(eeprom.outbox.recipient);
          tft.print("  ");
          tft.print(eeprom.outbox.coords.dateTime.hour);
          tft.print(":");
          tft.print(eeprom.outbox.coords.dateTime.minute);
          tft.print(":");
          tft.print(eeprom.outbox.coords.dateTime.second);
          tft.print("  ");
          tft.print(eeprom.outbox.coords.dateTime.day);
          tft.print("/");
          tft.print(eeprom.outbox.coords.dateTime.month);
          tft.print("/");
          tft.print(eeprom.outbox.coords.dateTime.year);
          //TODO GPS COORDS
          tft.print("\n");
          tft.setTextSize(2);
          textWrap(eeprom.outbox.content,26,10);
          mode = 6;
        }else if(touchPoint[0] < 50 && touchPoint[1] < 50){
          EEPROM.put(0,eeprom);
          mainMenu();
        } else if(touchPoint[0] > 50 && touchPoint[1] < 50 && touchPoint[0] < 100)
        {
          TypeMsg();
        }
        
    }
        
  }else if(mode == 3) {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){        
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        mainMenu();
      }else if(touchPoint[1] > 50){
        coordinates();
      }
    }
        
  }else if(mode == 4) { //this is settings
    EEPROM.get(0,eeprom);
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
        
  } else if(mode == 5) {
      SendMessage(5);    
  } else if(mode == 6) {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        customMessages();
      }
    }
    
  } else if(mode == 7) {
    
  } else if(mode == 8) {
    if(p.z > MINPRESSURE && p.z < MAXPRESSURE){
      if(touchPoint[0] < 50 && touchPoint[1] < 50){
        customMessages();
      } else if(touchPoint[0] <= 33 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //Q
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("q");
          } else
          {
            Serial.print("Q");
          }
        } else
        {
          Serial.print("1");
        }
      } else if (touchPoint[0] <= 64 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //W
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("w");
          } else
          {
            Serial.print("W");
          }
        } else
        {
          Serial.print("2");
        }
      } else if (touchPoint[0] <= 96 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //E
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("e");
          } else
          {
            Serial.print("E");
          }
        } else
        {
          Serial.print("3");
        }
      } else if (touchPoint[0] <= 128 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //R
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("r");
          } else
          {
            Serial.print("R");
          }
        } else
        {
          Serial.print("4");
        }
      } else if (touchPoint[0] <= 160 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //T
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("t");
          } else
          {
            Serial.print("T");
          }
        } else
        {
          Serial.print("5");
        }
      } else if (touchPoint[0] <= 192 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //Y
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("y");
          } else
          {
            Serial.print("Y");
          }
        } else
        {
          Serial.print("6");
        }
      } else if (touchPoint[0] <= 224 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //U
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("u");
          } else
          {
            Serial.print("U");
          }
        } else
        {
          Serial.print("7");
        }
      } else if (touchPoint[0] <= 256 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //I
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("i");
          } else
          {
            Serial.print("I");
          }
        } else
        {
          Serial.print("8");
        }
      } else if (touchPoint[0] <= 288 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //O
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("o");
          } else
          {
            Serial.print("O");
          }
        } else
        {
          Serial.print("9");
        }
      } else if (touchPoint[0] <= 320 && touchPoint[1] <= 122 && touchPoint[1] >= 84)
      {
        //P
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("p");
          } else
          {
            Serial.print("P");
          }
        } else
        {
          Serial.print("0");
        }
      } 
      
      else if (touchPoint[0] <= 47 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //A
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("a");
          } else
          {
            Serial.print("A");
          }
        } else
        {
          Serial.print("!");
        }
      } else if (touchPoint[0] <= 79 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //S
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("s");
          } else
          {
            Serial.print("S");
          }
        } else
        {
          Serial.print("@");
        }
      } else if (touchPoint[0] <= 111 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //D
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("d");
          } else
          {
            Serial.print("D");
          }
        } else
        {
          Serial.print("#");
        }
      } else if (touchPoint[0] <= 143 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //F
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("f");
          } else
          {
            Serial.print("F");
          }
        } else
        {
          Serial.print("%");
        }
      } else if (touchPoint[0] <= 175 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //G
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("g");
          } else
          {
            Serial.print("G");
          }
        } else
        {
          Serial.print("$");
        }
      } else if (touchPoint[0] <= 207 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //H
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("h");
          } else
          {
            Serial.print("H");
          }
        } else
        {
          Serial.print("^");
        }
      } else if (touchPoint[0] <= 239 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //J
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("j");
          } else
          {
            Serial.print("J");
          }
        } else
        {
          Serial.print("&");
        }
      } else if (touchPoint[0] <= 271 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //K
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("k");
          } else
          {
            Serial.print("K");
          }
        } else
        {
          Serial.print("*");
        }
      } else if (touchPoint[0] <= 320 && touchPoint[1] <= 161 && touchPoint[1] >= 123)
      {
        //L
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("l");
          } else
          {
            Serial.print("L");
          }
        } else
        {
          Serial.print("/");
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
            Serial.print("z");
          } else
          {
            Serial.print("Z");
          }
        } else
        {
          Serial.print(":");
        }
      } else if (touchPoint[0] <= 96 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
      {
        //X
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("x");
          } else
          {
            Serial.print("X");
          }
        } else
        {
          Serial.print(";");
        }
      } else if (touchPoint[0] <= 128 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
      {
        //C
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("c");
          } else
          {
            Serial.print("C");
          }
        } else
        {
          Serial.print("'");
        }
      } else if (touchPoint[0] <= 160 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
      {
        //V
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("v");
          } else
          {
            Serial.print("V");
          }
        } else
        {
          Serial.print("\"");
        }
      } else if (touchPoint[0] <= 192 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
      {
        //B
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("b");
          } else
          {
            Serial.print("B");
          }
        } else
        {
          Serial.print("-");
        }
      } else if (touchPoint[0] <= 224 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
      {
        //N
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("n");
          } else
          {
            Serial.print("N");
          }
        } else
        {
          Serial.print("(");
        }
      } else if (touchPoint[0] <= 256 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
      {
        //M
        if(!isNumBoard)
        {
          if(isLower)
          {
            Serial.print("m");
          } else
          {
            Serial.print("M");
          }
        } else
        {
          Serial.print(")");
        }
      } else if (touchPoint[0] <= 320 && touchPoint[1] <= 200 && touchPoint[1] >= 162)
      {
        //<--
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
          Serial.print("?");
        } else
        {
          Serial.print("_");
        }
      } else if (touchPoint[0] <= 96 && touchPoint[1] >= 201)
      {
        //,
        Serial.print(",");
      } else if (touchPoint[0] <= 224 && touchPoint[1] >= 201)
      {
        //Space
        Serial.print(" ");
      } else if (touchPoint[0] <= 256 && touchPoint[1] >= 201)
      {
        //.
        Serial.print(".");
      } else if (touchPoint[0] <= 320 && touchPoint[1] >= 201)
      {
        //Send
      }
      delay(300);
    }
    
  }//we probably need another screen for the custom texts
  
  
  if(mode != 7)
  {
  
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

void presetMessages(){
  mode = 1;
  Serial1.print("\nX\n");
  Serial1.println("SPre-set messages.");
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

void customMessages(){
  mode = 2;
  EEPROM.get(0,eeprom);
  Serial1.print("\nX\n");
  Serial1.println("SCustom messages.");
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
  tft.setTextSize(1);
  tft.print("Outbox ");
  tft.print(eeprom.outbox.recipient);
  tft.print("  ");
  tft.print(eeprom.outbox.coords.dateTime.hour);
  tft.print(":");
  tft.print(eeprom.outbox.coords.dateTime.minute);
  tft.print(":");
  tft.print(eeprom.outbox.coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.outbox.coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.outbox.coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.outbox.coords.dateTime.year);
  //TODO GPS COORDS
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.outbox.content,26,5);

  
  tft.fillRect(0, 145, 320, 240, ILI9341_CYAN);
  tft.setCursor(5, 150);
  tft.setTextColor(0x0000);
  tft.setTextSize(1);
  tft.print("Inbox ");
  tft.print(eeprom.inbox.sender);
  tft.print("  ");
  tft.print(eeprom.inbox.coords.dateTime.hour);
  tft.print(":");
  tft.print(eeprom.inbox.coords.dateTime.minute);
  tft.print(":");
  tft.print(eeprom.inbox.coords.dateTime.second);
  tft.print("  ");
  tft.print(eeprom.inbox.coords.dateTime.day);
  tft.print("/");
  tft.print(eeprom.inbox.coords.dateTime.month);
  tft.print("/");
  tft.print(eeprom.inbox.coords.dateTime.year);
  //TODO GPS COORDS
  tft.print("\n");
  tft.setTextSize(2);
  textWrap(eeprom.inbox.content,26,5);
}

void coordinates(){
  mode = 3;
  Serial1.print("\nX\n");
  Serial1.println("SViewing coordinates.");
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  drawTime();

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(130, 55);
  tft.setTextSize(2);
  tft.println("Bearing:");
  tft.setCursor(130, 75);
  drawBearing();

  tft.setCursor(5, 100);
  tft.println("coordinates here");
}

void settings(){
  mode = 4;
  Serial1.print("\nX\n");
  Serial1.println("SSettings menu.");
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
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
  EEPROM.put(0,eeprom);
}

void SOS(){
  mode = 5;
  Serial1.print("\nX\n");
  Serial1.println("SEmergency Protocol Activated!");
  tft.fillScreen(ILI9341_RED);
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawTime();

  tft.setCursor(10, 85);
  tft.setTextColor(0x0000);
  tft.setTextSize(17);
  tft.print("SOS");
}

void Sleep()
{
  mode = 7;
  tft.fillScreen(ILI9341_BLACK);
}

void TypeMsg()
{
  isLower = true;
  isNumBoard = false;
  mode = 8;
  tft.fillScreen(ILI9341_BLACK);
  Serial1.print("\nX\n");
  Serial1.println("SNew Message.");
  tft.fillRect(0, 0, 320, 50, ILI9341_WHITE);
  drawBack();
  drawTime();

  tft.drawBitmap(0,0,keyboardlower,320,240,ILI9341_WHITE);
}

void mainMenu(){
  mode = 0;
  Serial1.print("X\n");
  Serial1.println("SMain menu.");
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


void SetupEeprom()
{
  strcpy(eeprom.settings.predefinedMessages[0], "All is going well.");
  strcpy(eeprom.settings.predefinedMessages[1], "Setting up camp.");
  strcpy(eeprom.settings.predefinedMessages[2], "Going for a hike.");
  strcpy(eeprom.settings.predefinedMessages[3], "Injured, need help.");
  eeprom.settings.breadcrumbs = true;
  eeprom.settings.breadInterval = 10;
  eeprom.settings.militaryTime = false;
  strcpy(eeprom.settings.recipient, "+61432123456");

  strcpy(eeprom.outbox.recipient,"+61400000000");
  eeprom.outbox.coords.dateTime.year = 10 ;
  eeprom.outbox.coords.dateTime.month = 1;
  eeprom.outbox.coords.dateTime.day = 1;
  eeprom.outbox.coords.dateTime.hour = 12;
  eeprom.outbox.coords.dateTime.minute = 0;
  eeprom.outbox.coords.dateTime.second = 0;
  //TODO GPS COORDS
  strcpy(eeprom.outbox.content,"No Messages.");
  
  strcpy(eeprom.inbox.sender,"+61400000000");
  eeprom.inbox.coords.dateTime.year = 10;
  eeprom.inbox.coords.dateTime.month = 1;
  eeprom.inbox.coords.dateTime.day = 1;
  eeprom.inbox.coords.dateTime.hour = 12;
  eeprom.inbox.coords.dateTime.minute = 0;
  eeprom.inbox.coords.dateTime.second = 0;
  //TODO GPS COORDS  
  strcpy(eeprom.inbox.content,"No Messages.");
  
  EEPROM.put(0,eeprom);
}



void drawBearing(){ //all this code is shamelessly stolen
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
  tft.println(heading);
}

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
  Serial.print(rtc.hour());
  Serial.print(":");
  Serial.print(rtc.minute());
  Serial.print(":");
  Serial.print(rtc.second());
  Serial.print("|");
  //TODO GPS COORDS
  if(opt == 1)
  {
    Serial.print(eeprom.settings.predefinedMessages[0]);
    strcpy(eeprom.outbox.recipient,eeprom.settings.recipient);
    eeprom.outbox.coords.dateTime.year = rtc.year();
    eeprom.outbox.coords.dateTime.month = rtc.month();
    eeprom.outbox.coords.dateTime.day = rtc.date();
    eeprom.outbox.coords.dateTime.hour = rtc.hour();
    eeprom.outbox.coords.dateTime.minute = rtc.minute();
    eeprom.outbox.coords.dateTime.second = rtc.second();
    //TODO GPS COORDS
    strcpy(eeprom.outbox.content,eeprom.settings.predefinedMessages[0]);
  } else if(opt == 2)
  {
    Serial.print(eeprom.settings.predefinedMessages[1]);
    strcpy(eeprom.outbox.recipient,eeprom.settings.recipient);
    eeprom.outbox.coords.dateTime.year = rtc.year();
    eeprom.outbox.coords.dateTime.month = rtc.month();
    eeprom.outbox.coords.dateTime.day = rtc.date();
    eeprom.outbox.coords.dateTime.hour = rtc.hour();
    eeprom.outbox.coords.dateTime.minute = rtc.minute();
    eeprom.outbox.coords.dateTime.second = rtc.second();
    //TODO GPS COORDS
    strcpy(eeprom.outbox.content,eeprom.settings.predefinedMessages[1]);
  } else if(opt == 3)
  {
    Serial.print(eeprom.settings.predefinedMessages[2]);
    strcpy(eeprom.outbox.recipient,eeprom.settings.recipient);
    eeprom.outbox.coords.dateTime.year = rtc.year();
    eeprom.outbox.coords.dateTime.month = rtc.month();
    eeprom.outbox.coords.dateTime.day = rtc.date();
    eeprom.outbox.coords.dateTime.hour = rtc.hour();
    eeprom.outbox.coords.dateTime.minute = rtc.minute();
    eeprom.outbox.coords.dateTime.second = rtc.second();
    //TODO GPS COORDS
    strcpy(eeprom.outbox.content,eeprom.settings.predefinedMessages[2]);
  } else if(opt == 4)
  {
    Serial.print(eeprom.settings.predefinedMessages[3]);
    strcpy(eeprom.outbox.recipient,eeprom.settings.recipient);
    eeprom.outbox.coords.dateTime.year = rtc.year();
    eeprom.outbox.coords.dateTime.month = rtc.month();
    eeprom.outbox.coords.dateTime.day = rtc.date();
    eeprom.outbox.coords.dateTime.hour = rtc.hour();
    eeprom.outbox.coords.dateTime.minute = rtc.minute();
    eeprom.outbox.coords.dateTime.second = rtc.second();
    //TODO GPS COORDS
    strcpy(eeprom.outbox.content,eeprom.settings.predefinedMessages[3]);
  } else if(opt == 5)
  {
    Serial.print("SOS - SEND HELP!");
    strcpy(eeprom.outbox.recipient,eeprom.settings.recipient);
    eeprom.outbox.coords.dateTime.year = rtc.year();
    eeprom.outbox.coords.dateTime.month = rtc.month();
    eeprom.outbox.coords.dateTime.day = rtc.date();
    eeprom.outbox.coords.dateTime.hour = rtc.hour();
    eeprom.outbox.coords.dateTime.minute = rtc.minute();
    eeprom.outbox.coords.dateTime.second = rtc.second();
    //TODO GPS COORDS
    strcpy(eeprom.outbox.content,"SOS - SEND HELP!");
  }
  Serial.print("\n");
  tft.setCursor(52, 10);
  tft.setTextColor(0x0000);
  tft.setTextSize(4);
  tft.print("SENT");
  Serial1.println("SMessage Sent");
  delay(1000);
  tft.fillRect(52, 0, 92, 50, ILI9341_WHITE);
  EEPROM.put(0,eeprom);
}

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

//the program forks here

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