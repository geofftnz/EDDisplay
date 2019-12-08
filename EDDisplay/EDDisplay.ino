/*
 * Elite Dangerous Control Panel Driver
 * 
 * (c)2019 Geoff Thornburrow
 * 
 */
// required for LCD displays
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Shift register config
#define dataPin 5 //Pin connected to DS of 74HC595
#define latchPin 6 //Pin connected to ST_CP of 74HC595
#define clockPin 7 //Pin connected to SH_CP of 74HC595

// LCD - Navigation
LiquidCrystal_I2C navLCD(0x27, 2, 1, 0, 4, 5, 6, 7,3, POSITIVE);

// Display state.
int displayState = 0;
#define DISPLAY_INIT 0
#define DISPLAY_TEST 1
#define DISPLAY_RUN 2

// state parameters
int testSequence = 0;

void setup() 
{

  // shift register setup
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  // LCD setup
  navLCD.begin(20,4);
  navLCD.backlight();
  
  // set up for serial
  Serial.begin(9600);
}

void loop() 
{
  int nextState = displayState;
  
  switch(displayState)
  {
    case DISPLAY_INIT:
      testSequence = 0;
      nextState = DISPLAY_TEST;  
      break;

    case DISPLAY_TEST:
      nextState = displayTest();
      break;

    case DISPLAY_RUN:
      displayMain();
      break;
  }
  displayState = nextState;
}

int displayTest()
{
  static int lights=1;
  char output[21];
  byte data[2];
  int nextState = displayState;
  
  if (testSequence == 0)
  {
    clearLCD(&navLCD); 
    setLCDLine(&navLCD,0,"EDDisplay v0.01"); 
    setLCDLine(&navLCD,1,"Geoff Thornburrow"); 
    setLCDLine(&navLCD,2,"Starting up..."); 
  }

  sprintf(output,"TESTING: %02d",testSequence);
  setLCDLine(&navLCD,3,output); 

  lights<<=1;
  if (!lights) lights = 1;

  data[0] = lights >> 8;
  data[1] = (byte)lights;
  shiftRegisterWrite(data,2);
  delay(250);
  
  testSequence++;
  if (testSequence > 15)
  {
    clearLCD(&navLCD); 
    data[0] = data[1] = 0;
    shiftRegisterWrite(data,2);
    nextState = DISPLAY_RUN;  
  }
  return nextState;
}

void displayMain()
{

}

void clearLCD(LiquidCrystal_I2C *lcd)
{
  lcd->clear();
  lcd->home();
}
void setLCDLine(LiquidCrystal_I2C *lcd,byte line, char *text)
{
  lcd->setCursor ( 0,line );        
  lcd->print (text);  
}

void shiftRegisterWrite(byte *data, int count)
{
  digitalWrite(latchPin, LOW);
  for (int i=0;i<count;i++)
  {
    shiftOut(dataPin, clockPin, MSBFIRST, data[i]);    
  }
  digitalWrite(latchPin, HIGH);  
}
