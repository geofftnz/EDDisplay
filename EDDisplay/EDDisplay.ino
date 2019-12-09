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

// Misc
#define PIN_STANDBY 12

// LCD - Navigation
LiquidCrystal_I2C navLCD(0x27, 2, 1, 0, 4, 5, 6, 7,3, POSITIVE);

// Display state.
#define DISPLAY_STANDBY 0
#define DISPLAY_INIT 1
#define DISPLAY_TEST 2
#define DISPLAY_RUN 3
int displayState = DISPLAY_STANDBY;

// Serial
#define BUFLEN 64
#define BUFMASK 63
unsigned char serialBuffer[BUFLEN];
int bufPos = -1;
unsigned char lastByte=255;
int bufReadPos = -1;

// Serial Commands
#define SS_WAITING 0
#define SS_INCOMMAND 1
byte serialState = SS_WAITING;

//                               0    1    2    3    4    5    6    7    8    9   bl
unsigned char LED_Lookup[] = {0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90,0xff, 0x8C,0xBF,0xC6,0xA1,0x86,0xFF,0xbf};


// state parameters
int testSequence = 0;

void setup() 
{
  // standby
  pinMode(PIN_STANDBY,INPUT_PULLUP);

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
    case DISPLAY_STANDBY:
      nextState = displayStandby();    
      break;
      
    case DISPLAY_INIT:
      testSequence = 0;
      displayInit();
      nextState = DISPLAY_TEST;  
      break;

    case DISPLAY_TEST:
      nextState = displayTest();
      break;

    case DISPLAY_RUN:
      displayMain();
      break;
  }
  // override next state
  if (!isStandbyOn())
    nextState = DISPLAY_STANDBY;
    
  displayState = nextState;
}

int displayStandby()
{
  // turn off LCD backlights
  clearLCD(&navLCD);
  setLCDBacklight(&navLCD,LOW);
  setLCDLine(&navLCD,0,"EDDisplay Standby");

  // turn off LEDs
  byte data[]={0,0};
  shiftRegisterWrite(data,2);

  delay(500);
  return isStandbyOn()?DISPLAY_INIT:DISPLAY_STANDBY;
}

void displayInit()
{
  setLCDBacklight(&navLCD,HIGH);
}

bool isStandbyOn()
{
  return digitalRead(PIN_STANDBY) == HIGH;
}

int displayTest()
{
  static int lights=1;
  char output[21];
  byte data[]={0,0};
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
  delay(50);
  
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
  byte data[2];

  if (bufReadPos < bufPos && bufPos >= 0)
  {
    bufReadPos++;
    char output[21];
    sprintf(output,"RECV: %02x",serialBuffer[bufReadPos]);
    setLCDLine(&navLCD,3,output); 
    data[0] = serialBuffer[bufReadPos];
    data[1] = 0;

    shiftRegisterWrite(data,2);
    
    bufReadPos = -1;
    bufPos = 0;
  }
}

void setLCDBacklight(LiquidCrystal_I2C *lcd, uint8_t value)
{
  lcd->setBacklight(value);
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

void serialEvent() {
  while (Serial.available()) 
  {

    byte currentByte = (byte)Serial.read();
    if (bufPos < 0) bufPos = 0;
    serialBuffer[bufPos++] = currentByte;
    if (bufPos >= BUFLEN)
    {
      bufPos = 0;
    }
  }
}
