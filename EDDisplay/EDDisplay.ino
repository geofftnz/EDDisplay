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

// 7seg displays
#define PIN_LED_SCLK 2
#define PIN_LED_RCLK 3
#define PIN_LED_DIO 4

// Misc
#define PIN_STANDBY 12

// LCD - Navigation
LiquidCrystal_I2C navLCD(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Display state.
#define DISPLAY_STANDBY 0
#define DISPLAY_INIT 1
#define DISPLAY_TEST 2
#define DISPLAY_RUN 3
int displayState = DISPLAY_STANDBY;

// 7 segment state
int cargo_value = 158;
int fuel_value = 8865;
unsigned char LED7_cargo[] = { 10,10,10,10 };
unsigned char LED7_fuel[] = { 10,10,10,10 };

// Serial
#define BUFLEN 64
#define BUFMASK 63
unsigned char serialBuffer[BUFLEN];
int serialBufferPos = 0;
unsigned char serial_command = 0;
unsigned char have_command = 0;
unsigned char serial_command_overflow = 0;

// Serial state machine
#define SS_WAITING 0      // startup
#define SS_WAITING_ESC 1  // first escape

#define SS_COMMAND_DATA 10 // reading command data
#define SS_COMMAND_DATA_ESC 11 // reading command data - got an escape

#define SS_COMMAND_DATA_OVERFLOW 12  // we've read too much data, so consume the rest and discard.

#define SS_COMMAND_END 20  // end the command.

unsigned char serialState = SS_WAITING;

// Serial Commands
#define CMD_ESC 0xff
#define CMD_END 0xfe

#define CMD_SHIFTREG 0x01

#define CMD_LCDCMASK  0xF0
#define CMD_LCDDMASK  0x0F
#define CMD_LCDLINE1 0x10
#define CMD_LCDLINE2 0x11
#define CMD_LCDLINE3 0x12
#define CMD_LCDLINE4 0x13
#define CMD_LCDLINE5 0x14
#define CMD_LCDLINE6 0x15
#define CMD_LCDLINE7 0x16
#define CMD_LCDLINE8 0x17

#define CMD_7SEG1 0x21
#define CMD_7SEG2 0x22

//                               0    1    2    3    4    5    6    7    8    9   bl
unsigned char LED_Lookup[] = { 0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90,0xff, 0x8C,0xBF,0xC6,0xA1,0x86,0xFF,0xbf };

// state parameters
int testSequence = 0;

void setup()
{
  // standby
  pinMode(PIN_STANDBY, INPUT_PULLUP);

  // shift register setup
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  // 7seg setup
  pinMode(PIN_LED_SCLK, OUTPUT);
  pinMode(PIN_LED_RCLK, OUTPUT);
  pinMode(PIN_LED_DIO, OUTPUT);


  // LCD setup
  navLCD.begin(20, 4);
  navLCD.backlight();

  // set up for serial
  Serial.begin(9600);
}

void loop()
{
  int nextState = displayState;

  switch (displayState)
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


void serialEvent() {
  while (Serial.available())
  {
    int nextState = serialState;
    unsigned char c = (unsigned char)Serial.read();

    switch (serialState)
    {
    case SS_WAITING:  // we've just started up, so we wait for an escape + command
      if (c == CMD_ESC)
        nextState = SS_WAITING_ESC;
      break;
    case SS_WAITING_ESC: // we're waiting for our first command. We've read an escape character, so are expecting a command.

      switch (c)
      {
      case CMD_END: // this was the end of another command
      case CMD_ESC: // this was an escaped escape
        nextState = SS_WAITING;
        break;
      default: // otherwise treat this as a command.
        serial_command = c;
        have_command = 0;
        serialBufferPos = 0;
        serial_command_overflow = 0;
        nextState = SS_COMMAND_DATA;
        break;
      }
      break;
    case SS_COMMAND_DATA:  // we're reading command data
      if (c == CMD_ESC)
      {
        nextState = SS_COMMAND_DATA_ESC;
      }
      else
      {
        if (serialBufferPos < BUFLEN) {
          serialBuffer[serialBufferPos++] = c;
        }
        else {
          serial_command_overflow = 1;
        }
      }
      break;
    case SS_COMMAND_DATA_ESC:  // we've read an escape while reading command data.

      switch (c) {
      case CMD_ESC:  // ESC ESC to write an escape to the data stream.
        if (serialBufferPos < BUFLEN) {
          serialBuffer[serialBufferPos++] = c;
        }
        else {
          serial_command_overflow = 1;
        }
        break;
      case CMD_END:  // got our command terminator, so return the command and go back to waiting
        have_command = 1;
        nextState = SS_WAITING;
      default:  // ignore unterminated commands
        nextState = SS_WAITING;
        break;
      }

      break;
    }
    serialState = nextState;

    // if we've received a command, break out of the loop so it can be processed.
    if (have_command == 1) {
      break;
    }
  }
}




int displayStandby()
{
  // turn off LCD backlights
  clearLCD(&navLCD);
  setLCDBacklight(&navLCD, LOW);
  setLCDLine(&navLCD, 0, "EDDisplay Standby");

  // turn off LEDs
  unsigned char data[] = { 0,0 };
  shiftRegisterWrite(data, 2);

  // turn off 7seg
  LED_Blank(LED7_cargo);
  LED_Blank(LED7_fuel);
  refresh7Segments();

  delay(500);
  return isStandbyOn() ? DISPLAY_INIT : DISPLAY_STANDBY;
}


void displayInit()
{
  setLCDBacklight(&navLCD, HIGH);
}

bool isStandbyOn()
{
  return digitalRead(PIN_STANDBY) == HIGH;
}

int displayTest()
{
  static int lights = 1;
  char output[21];
  unsigned char data[] = { 0,0 };
  int nextState = displayState;

  if (testSequence == 0)
  {
    clearLCD(&navLCD);
    setLCDLine(&navLCD, 0, "EDDisplay v0.01");
    setLCDLine(&navLCD, 1, "Geoff Thornburrow");
    setLCDLine(&navLCD, 2, "Starting up...");
  }

  sprintf(output, "TESTING: %02d", testSequence);
  setLCDLine(&navLCD, 3, output);

  if (testSequence < 16)
  {
    lights <<= 1;
    if (!lights) lights = 1;

    data[0] = lights >> 8;
    data[1] = (unsigned char)lights;
    shiftRegisterWrite(data, 2);
    delay(50);
  }
  if (testSequence == 16)
  {
    LED_SetFromInt(LED7_cargo, 1234);
    LED_Blank(LED7_fuel);
    for (int i = 0; i < 100; i++) {
      refresh7Segments();
      delay(2);
    }
  }
  if (testSequence == 17)
  {
    LED_Blank(LED7_cargo);
    LED_SetFromInt(LED7_fuel, 5678);
    for (int i = 0; i < 100; i++) {
      refresh7Segments();
      delay(2);
    }
  }

  // exit clause
  testSequence++;
  if (testSequence > 17)
  {
    // turn off 7seg
    LED_Blank(LED7_cargo);
    LED_Blank(LED7_fuel);
    refresh7Segments();

    clearLCD(&navLCD);
    data[0] = data[1] = 0;
    shiftRegisterWrite(data, 2);
    nextState = DISPLAY_RUN;
  }
  return nextState;
}

void refresh7Segments()
{
  unsigned char i;
  for (i = 0; i < 4; i++)
  {
    LED_Out(LED_Lookup[LED7_fuel[i] & 0x0f]);
    LED_Out(1 << i);
    LED_Out(LED_Lookup[LED7_cargo[i] & 0x0f]);
    LED_Out(1 << i);
    LowPulse(PIN_LED_RCLK);
  }
}

void displayMain()
{
  unsigned char data[2];

  // process pending command
  if (have_command)
  {
    // group comands
    if ((serial_command & CMD_LCDCMASK) == CMD_LCDLINE1) {
      // buffer overflow protection
      if (serialBufferPos < BUFLEN) {
        serialBuffer[serialBufferPos] = 0;
      }
      else {
        serialBuffer[BUFLEN - 1] = 0;
      }

      unsigned char line = serial_command & CMD_LCDDMASK;
      if (line >= 0 && line < 4) {
        setLCDLine(&navLCD, line, serialBuffer);
      }
    }
    else {
      switch (serial_command) {
      case CMD_7SEG1:
        if (serialBufferPos >= 4) {
          for (int i = 0; i < 4; i++) {
            LED7_cargo[i] = serialBuffer[i];
          }
        }
        break;
      case CMD_7SEG2:
        if (serialBufferPos >= 4) {
          for (int i = 0; i < 4; i++) {
            LED7_fuel[i] = serialBuffer[i];
          }
        }
        break;
      case CMD_SHIFTREG:
        shiftRegisterWrite(serialBuffer, serialBufferPos);
      }
    }
    have_command = 0;
  }

  refresh7Segments();
}

void setLCDBacklight(LiquidCrystal_I2C* lcd, uint8_t value)
{
  lcd->setBacklight(value);
}
void clearLCD(LiquidCrystal_I2C* lcd)
{
  lcd->clear();
  lcd->home();
}
void setLCDLine(LiquidCrystal_I2C* lcd, unsigned char line, char* text)
{
  lcd->setCursor(0, line);
  lcd->print(text);
}

void shiftRegisterWrite(unsigned char* data, int count)
{
  digitalWrite(latchPin, LOW);
  for (int i = 0; i < count; i++)
  {
    shiftOut(dataPin, clockPin, MSBFIRST, data[i]);
  }
  digitalWrite(latchPin, HIGH);
}



void LowPulse(int pin)
{
  digitalWrite(pin, LOW);
  digitalWrite(pin, HIGH);
}

void LED_Blank(unsigned char* data)
{
  unsigned char blank = 10;
  data[0] = blank;
  data[1] = blank;
  data[2] = blank;
  data[3] = blank;
}

void LED_SetFromInt(unsigned char* data, int value)
{
  unsigned char digit;
  for (digit = 0; digit < 4; digit++)
  {
    data[digit] = value % 10;
    value /= 10;
  }
}


void LED_Out(unsigned char x)
{
  unsigned char i;
  for (i = 0; i < 8; i++)
  {
    digitalWrite(PIN_LED_DIO, (x & 0x80) ? HIGH : LOW);
    x <<= 1;
    LowPulse(PIN_LED_SCLK);
  }
}

void LED_Display4(unsigned char* LEDs, int digit_ofs)
{
  unsigned char digit;
  for (digit = 0; digit < 4; digit++)
  {
    if (digit_ofs == 0) {
      LED_Out(0);
      LED_Out(0);
    }
    LED_Out(LED_Lookup[LEDs[digit] & 0x0f]);
    LED_Out(1 << digit);
    if (digit_ofs > 0) {
      LED_Out(0);
      LED_Out(0);
    }
    LowPulse(PIN_LED_RCLK);
  }
}

void LED_DisplayInt(int x, int digit_ofs)
{
  unsigned char digit;
  for (digit = 0; digit < 4; digit++)
  {
    LED_Out(LED_Lookup[x % 10]);
    LED_Out(1 << (digit + digit_ofs));
    LowPulse(PIN_LED_RCLK);

    x /= 10;
  }
}
