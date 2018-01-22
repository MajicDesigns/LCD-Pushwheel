// Use the LCD module display to create a mechanical pushwheel type display
// When numbers change they are scrolled up or down as if on a cylinder.
//
// Increment or decrement numbers using the UP/DOWN keys on an LCD shield
//
// Marco Colli - January 2018
// Available at https://github.com/MajicDesigns/LCD-Pushwheel
//

#include <LiquidCrystal.h>

#define DEBUG 0

#if DEBUG
#define	PRINT(s, v)	{ Serial.print(F(s)); Serial.print(v); }
#define	PRINTX(s, v)	{ Serial.print(F(s)); Serial.print(v, HEX); }
#define PRINTS(s)   Serial.print(F(s));
#else
#define	PRINT(s, v)
#define PRINTS(s)
#endif

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

// LCD definitions
// Pins
const uint8_t LCD_RS = 8;
const uint8_t LCD_ENA = 9;
const uint8_t LCD_D4 = 4;
const uint8_t LCD_D5 = LCD_D4 + 1;
const uint8_t LCD_D6 = LCD_D4 + 2;
const uint8_t LCD_D7 = LCD_D4 + 3;

LiquidCrystal lcd(LCD_RS, LCD_ENA, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// LCD Geometry
const uint8_t LCD_ROWS = 2;
const uint8_t LCD_COLS = 16;

// Display and animation parameters
const uint8_t ANIMATION_FRAME_TIME = 50;  // in milliseconds
const uint8_t DISP_R = 1;
const uint8_t DISP_C = 0;

// Structure to hold the data for each character to be displayed and animated.
// There can only be at most MAX_DIGITS displayed as the limit comes from
// the number of custom characters that can be defined for the number.
const uint8_t CHAR_ROWS = 8;
const uint8_t CHAR_COLS = 5;
const uint8_t MAX_DIGITS = 8;

typedef struct
{
  uint32_t timeLastFrame;     // time the last frame started animating
  uint8_t prev, curr;         // numeric value for the digit
  uint8_t index;              // animation progression index
  uint8_t charMap[CHAR_ROWS]; // generated custom char bitmap
} digitData_t;

digitData_t digits[MAX_DIGITS];

// User defined characters for digits. These should match the standard
// LCD ROM bitmaps for these digits.
uint8_t digitsMap[][CHAR_ROWS] =
{
  { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e, 0x00 }, // '0'
  { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e, 0x00 }, // '1'
  { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f, 0x00 }, // '2'
  { 0x1f, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0e, 0x00 }, // '3'
  { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02, 0x00 }, // '4'
  { 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e, 0x00 }, // '5'
  { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e, 0x00 }, // '6'
  { 0x1f, 0x11, 0x01, 0x02, 0x04, 0x04, 0x04, 0x00 }, // '7'
  { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e, 0x00 }, // '8'
  { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c, 0x00 }, // '9'
};

// LCD Shield key definitions
const uint8_t KEY_ADC_PORT = A0;
const uint8_t KEY_NONE = 0;
const uint8_t KEY_RIGHT = 1;
const uint8_t KEY_UP = 2;
const uint8_t KEY_DOWN = 3;
const uint8_t KEY_LEFT = 4;
const uint8_t KEY_SELECT = 5;

typedef struct
{
  int   adcThreshold;
  uint8_t id;
} keyDef_t;

const keyDef_t keyDefTable[] =
{
  { 760, KEY_SELECT },
  { 535, KEY_LEFT },
  { 360, KEY_DOWN },
  { 160, KEY_UP },
  { 60, KEY_RIGHT }
};

uint8_t getKey(unsigned int input)
// The LCD shield has a voltage divider tree for a set of buttons.
// Convert ADC value passed in to key identifier using the table of value defined
// Return KEY_NONE if no key.
{
  static uint32_t lastCheck = 0;
  uint8_t  key = KEY_NONE;

  if (millis() - lastCheck > 200)
  {
    for (int k = 0; k < ARRAY_SIZE(keyDefTable); k++)
    {
      if (input < keyDefTable[k].adcThreshold)
        key = keyDefTable[k].id;  // Assume it will match this one
      else
        break;
    }
  }

  if (key != KEY_NONE)
    lastCheck = millis();

  return (key);
}

void updateDisplay(uint8_t r, uint8_t c, bool init = false)
// do the necessary to display current number scrolling anchored
// on the LHS at LCD (r, c) coordinates.
{
  PRINTS("\nuD ")
  // for each digit position, create the lcd custom character, and
  // if told to, display the custom characters left to right.
  for (uint8_t i = 0; i < MAX_DIGITS; i++)
  {
    PRINT(".", digits[MAX_DIGITS - i - 1].curr);
    lcd.createChar(i, digits[MAX_DIGITS - i - 1].charMap);
    if (init)
    {
      lcd.setCursor(c + i, r);
      lcd.write((uint8_t)i);
    }
  }
}

boolean displayValue(uint32_t value)
// Display the required animated value on the LCD matrix and return true if 
// an animation is current. Finite state machine will ignore new values while 
// e=xisting animations are underway.
// Needs to be called repeatedly to ensure animations are completed smoothly.
{
  static enum { ST_INIT, ST_WAIT, ST_ANIM } state = ST_INIT;
  static uint32_t valueLast = 0;  // remember old value
  bool bUpdate = false;

  // finite state machine to control what we do
  switch (state)
  {
    case ST_INIT:	// Initialise the display - done once only on first call
      PRINTS("\nST_INIT");
      for (uint8_t i = 0; i < MAX_DIGITS; i++)
      {
        // separate digits
        digits[i].prev = digits[i].curr = value % 10;
        value = value / 10;
      }

      // Display the starting number
      for (uint8_t i = 0; i < MAX_DIGITS; i++)
        memcpy(digits[i].charMap, digitsMap[digits[i].curr], ARRAY_SIZE(digits[i].charMap));

      updateDisplay(DISP_R, DISP_C, true);

      // Now we just wait for a change
      state = ST_WAIT;
      PRINTS("\nTo ST_WAIT");
      break;

    case ST_WAIT: // not animating - save new value digits and check if we need to animate
      //PRINTS("\nST_WAIT");
      if (valueLast != value)
      {
        state = ST_ANIM;  // a change has been found - we will be animating something
        
        for (int8_t i = 0; i < MAX_DIGITS; i++)
        {
          // separate digits
          digits[i].curr = value % 10;
          value = value / 10;
          
          // initialise animation parameters for this digit
          digits[i].index = 0;
          digits[i].timeLastFrame = 0;
        }
      }

      if (state == ST_WAIT) // no changes - keep waiting
        break;
    // else fall through as we need to animate from now

    case ST_ANIM: // currently animating a change
      // work out the new intermediate bitmap for each character
      for (uint8_t i = 0; i < MAX_DIGITS; i++)
      {
        if ((digits[i].prev != digits[i].curr) && // values are different ...
            (millis() - digits[i].timeLastFrame >= ANIMATION_FRAME_TIME)) // ... and timer has expired
        {
          PRINT("\nST_ANIM ", i);
          PRINT(" '", digits[i].prev);
          PRINT("'-'", digits[i].curr);
          PRINT("' idx ", digits[i].index);

          if (value > valueLast)
          {
            // scroll up
            // copy the bottom of the old digit from the index position and then the
            // top of new digit for the rest of the character
            PRINTS(" UP ");
            for (int8_t p = 0; p < CHAR_ROWS; p++)
            {
              if (p < CHAR_ROWS - digits[i].index)
                digits[i].charMap[p] = digitsMap[digits[i].prev][p + digits[i].index];
              else
                digits[i].charMap[p] = digitsMap[digits[i].curr][p - CHAR_ROWS + digits[i].index];
            }

            bUpdate = true;
          }
          else
          {
            // scroll down
            // copy the bottom of new digit up to the index position and then from
            // the start of the old digit for the rest of the character
            PRINTS(" DWN ");
            for (uint8_t p = 0; p < CHAR_ROWS; p++)
            {
              if (p < digits[i].index)
                digits[i].charMap[p] = digitsMap[digits[i].curr][p + CHAR_ROWS - digits[i].index];
              else
                digits[i].charMap[p] = digitsMap[digits[i].prev][p - digits[i].index];
            }

            bUpdate = true;
          }

          // set new parameters for next animation and check if we are done
          digits[i].index++;
          digits[i].timeLastFrame = millis();
          if (digits[i].index > CHAR_ROWS)
            digits[i].prev = digits[i].curr;  // done animating
        }
      }

      if (bUpdate) updateDisplay(DISP_R, DISP_C);

      // are we done animating?
      {
        boolean allDone = true;

        for (uint8_t i = 0; allDone && (i < MAX_DIGITS); i++)
        {
          allDone = allDone && (digits[i].prev == digits[i].curr);
        }

        if (allDone)
        {
          valueLast = value;
          state = ST_WAIT;
        }
      }
      break;

    default:
      state = ST_INIT;
  }

  return (state == ST_WAIT);   // animation has ended
}

void setup()
{
#if DEBUG
  Serial.begin(57600);
#endif // DEBUG
  PRINTS("\n[LCD PushWheel]")

  // initialise LCD display
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.noCursor();
  lcd.print("Pushwheel Demo");

  pinMode(KEY_ADC_PORT, INPUT);
};

void loop()
{
  static uint32_t value = 12345678;
  uint8_t k = getKey(analogRead(KEY_ADC_PORT));

  if (k == KEY_UP) value++;
  else if (k == KEY_DOWN) value--;

  displayValue(value);
}

