//TriLight lamp control
//By Silas Alrøe

//Include used libraries
#include <EEPROM.h>             //Allows storage of lamp data in non-volatile memory
#include <DS3231.h>             //Allows interaction with RTC module without directly using i2c
#include <Wire.h>               //Used by DS3231 for i2c communication with RTC
#include <ADCTouch.h>           //Lets analog pins be used as touch sensors by using internal capacitors intelligently
#include <Adafruit_NeoPixel.h>  //Allows control of WS2812 RGB LEDs and other Neopixel products

//Neopixel values
#define NEOPIN 2      //Pin on which pixels are connected
#define NUMPIXELS 14  //Number of pixels on string
#define ANIMINC 0.01  //Amount to change intensity by when animating

//Touch config values
#define THEAD A6          //Pin for the head touch button
#define TBASE A7          //Pin for the base touch button
#define TINITSAMPLES 500  //Amount of samples for touch reference measurement
#define TSENSESAMPLES 100 //Amount of continious samples when checking for touch
#define TACTIVATE 70      //Sensitivity of touch buttons - higher value means lower sensitivity

//LED wavelengths as according to WS2812 spec sheet
#define rwl 625 //Red
#define gwl 523 //Green
#define bwl 470 //Blue

//Temperature to blue light conversion values
#define TEMPA 0.03527021888377 //Multiplier
#define TEMPB -109.285         //Addition

//Interval for large tasks
#define TMINT 25 //Amount of cycles

//Initialize neopixels and RTC libraries
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);
DS3231 Clock;

//Map pixels from center for nicer dithering
const int pixelMap[] = {5, 6, 9, 10, 4, 7, 1, 2, 0, 3, 12, 13, 8, 11}; //Starts at middle, moving outwards in semi-spiral

bool h12;
bool PM;

//Init global values
int refHead, refBase;               //Touch reference values
int powerState = 1;                 //Turned on or off 0/1
float intensity = 0.25;             //Target intensity of pixels - starts at 0.25
int targetTemperatureLow = 100;     //Target temperature at night - is replaced with values from EEPROM
int targetTemperatureHigh = 10000;  //Target temperature at day - is replaced with values from EEPROM
int currentTemperature = 1000;      //Temperature at current time - is overwritten continiously
bool touchIsPressed, headIsTouched, touchWasPressed, headWasTouched, scrollingUp; //Program state bools
bool redMode = false;               //Is red night-light enabled
bool demoMode = false;              //Is demoMode enabled
int demoModeCounter = 0;            //Counter for handling demoMode logic

float movIntensity = 0.0;           //Actual intensity of the pixels
float prevIntensity = intensity;    //Target intensity before lamp was turned off

int tmc = 0; //Program cycle counter
int hour, minute, second, sDHour, sDMinute, sRHour, sRMinute, intHour, intMinute; //Time used for calculating intensity

void setup() {
  //Init serial
  Serial.begin(9600);
  //Init pixels
  pixels.begin();
  //Init touch
  refHead = ADCTouch.read(THEAD, TINITSAMPLES);
  refBase = ADCTouch.read(TBASE, TINITSAMPLES);
  //Init RTC
  Wire.begin();
  //Load stored values for temperature transition
  loadVals();
  //Calculate the current temperature to avoid jagging colorchange
  calculateTemperature();
}

void loop() {
  //Read serial for possible update to data
  readSerial();
  //Check touch buttons
  handleTouch();
  //Update pixels
  setFromTemperature(currentTemperature, intensity);
  //Only do this every TMINT cycle
  if (tmc >= TMINT) {
    //Calculate the temperature of light
    calculateTemperature();
    tmc = 0;
  }
  tmc ++;
  //DEMOMODE - Should be removed in final product
  handleDemoMode();
}

//SERIAL
void readSerial() {
  //Format: HH MM SdhSdh SdmSdm SrhSrh SrmSrm IhIh ImIm [Target temp low] [Target temp high]
  //EX: 23x58x22x30x06x30x01x02x500x8000
  //If serial has data available
  while (Serial.available() > 0) {
    //Read current time
    char hour = char(Serial.parseInt());
    char minute = char(Serial.parseInt());

    //Read sundown time
    sDHour = Serial.parseInt();
    sDMinute = Serial.parseInt();

    //Read sunrise time
    sRHour = Serial.parseInt();
    sRMinute = Serial.parseInt();

    //Read transition time
    intHour = Serial.parseInt();
    intMinute = Serial.parseInt();

    //Read maximum and minimum temperature
    int temperatureL = Serial.parseInt();
    int temperatureH = Serial.parseInt();

    //If the datastream has finished with a newline
    if (Serial.read() == '\n') {
      //Set final temperature values
      targetTemperatureLow = temperatureL;
      targetTemperatureHigh = temperatureH;

      //Update the RTC with new time in 24h format
      Clock.setClockMode(false);
      Clock.setHour(hour);
      Clock.setMinute(minute);
      //Return new time over serial
      Serial.print(Clock.getHour(h12, PM));
      Serial.print(":");
      Serial.print(Clock.getMinute());
      //Save new values to EEPROM
      saveVals();
    }
  }
  //OLD
  /*while (Serial.available() > 0) {
    // Read ints from serial
    int temperature = Serial.parseInt();
    intensity = 0.1;

    // Set on newline
    if (Serial.read() == '\n') {
      targetTemperature = temperature;
    }
    }*/
}

//SAVE / LOAD
void saveVals() {
  //Save values sDH, sdM sRH, sRM Ih Im Tl Th to EEPROM
  //Start at EEPROM position 0
  int ePos = 0;
  //Set variable
  EEPROM.put(ePos, sDHour);
  //Add size in bytes of the variable to the position
  ePos += sizeof(int);
  //Cont...
  EEPROM.put(ePos, sDMinute);
  ePos += sizeof(int);
  EEPROM.put(ePos, sRHour);
  ePos += sizeof(int);
  EEPROM.put(ePos, sRMinute);
  ePos += sizeof(int);
  EEPROM.put(ePos, intHour);
  ePos += sizeof(int);
  EEPROM.put(ePos, intMinute);
  ePos += sizeof(int);
  EEPROM.put(ePos, targetTemperatureLow);
  ePos += sizeof(int);
  EEPROM.put(ePos, targetTemperatureHigh);
  ePos += sizeof(int);

}

void loadVals() {
  //Load values sDH, sdM sRH, sRM Ih Im Tl Th from EEPROM
  //Start at EEPROM position 0
  int ePos = 0;
  //Get variable
  EEPROM.get(ePos, sDHour);
  //Add size in bytes of the variable to the position
  ePos += sizeof(int);
  //Cont...
  EEPROM.get(ePos, sDMinute);
  ePos += sizeof(int);
  EEPROM.get(ePos, sRHour);
  ePos += sizeof(int);
  EEPROM.get(ePos, sRMinute);
  ePos += sizeof(int);
  EEPROM.get(ePos, intHour);
  ePos += sizeof(int);
  EEPROM.get(ePos, intMinute);
  ePos += sizeof(int);
  EEPROM.get(ePos, targetTemperatureLow);
  ePos += sizeof(int);
  EEPROM.get(ePos, targetTemperatureHigh);
  ePos += sizeof(int);
}

//TIME
void calculateTemperature() {
  //RED mode clause
  if (redMode) {
    //Just set temperature very low and return
    currentTemperature = 100;
    return;
  }

  //Get time from RTC and get total minutes since 00:00
  int hour = int(Clock.getHour(h12, PM));
  int minute = int(Clock.getMinute());
  int totalTime = hour * 60 + minute;

  //Calculate same for sundown, sunrise and interval
  int sDPoint = sDHour * 60 + sDMinute;
  int sRPoint = sRHour * 60 + sRMinute;
  int intSpan = intHour * 60 + intMinute;
  //Halve interval to place sunrise and sundown times in middle of transition
  int hIntSpan = intSpan / 2;

  //Assuming sDPoint is before 0
  //If time is after sunrise transition and before sundown transition, set temperature to maximum
  if (totalTime > (sRPoint + hIntSpan) && totalTime < (sDPoint - hIntSpan)) {
    currentTemperature = targetTemperatureHigh;
    //Else if time is after sundown transition and before sunrise transition, set temperature to minimum
  } else if (totalTime < (sRPoint - hIntSpan) || totalTime > (sDPoint + hIntSpan)) {
    currentTemperature = targetTemperatureLow;
    //Else if time is during sunrise transition, map temperature linearly from min to max through transition time
  } else if (totalTime > (sRPoint - hIntSpan) && totalTime < (sRPoint + hIntSpan)) {
    currentTemperature = map(totalTime, sRPoint - hIntSpan, sRPoint + hIntSpan, targetTemperatureLow, targetTemperatureHigh);
    //Else if time is during sundown transition, map temperature linearly from max to min through transition time
  } else if (totalTime > (sDPoint - hIntSpan) && totalTime < (sDPoint + hIntSpan)) {
    currentTemperature = map(totalTime, sDPoint - hIntSpan, sDPoint + hIntSpan, targetTemperatureHigh, targetTemperatureLow);
  }

}

//TOUCH
void handleTouch() {
  //Sets powerState on unique touch
  if (isTouched()) {
    //Usual cycle: Off -> (red, on) -> On -> off
    //If the lamp is off, turn it on
    if (powerState == 0) {
      //Reset to ligt intensity before lamp was turned off
      intensity = prevIntensity;
      powerState = 1;
      //If it's dark outside, start in redmode
      if (isDark()) {
        redMode = true;
        //Recalculate temperature to get redMode temperature
        calculateTemperature();
      }
    } else if (redMode) {
      //Else if lamp is on and in redMode, disable it and update
      redMode = false;
      calculateTemperature();
    } else {
      //Else if lamp is just on, turn it off, and store the previous light intensity
      powerState = 0;
      prevIntensity = intensity;
      intensity = 0.0;
    }
  }
  //Serial.print(powerState);
  //Check if light intensity button is touched and act accordingly
  checkIntensityScroll();
}

bool isDark() {
  //Check if it's dark by checking if time is after sundown
  int hour = int(Clock.getHour(h12, PM));
  int minute = int(Clock.getMinute());
  int totalTime = hour * 60 + minute;

  int sDPoint = sDHour * 60 + sDMinute;
  int sRPoint = sRHour * 60 + sRMinute;
  int intSpan = intHour * 60 + intMinute;
  int hIntSpan = intSpan / 2;

  //Assuming sDPoint is before 0
  if (totalTime < (sRPoint - hIntSpan) || totalTime > (sDPoint + hIntSpan)) {
    return true;
  }
  return false;
}

bool isTouched() {
  //Check if touch is unique by seeing if was touched in previous cycle
  bool ret = false;
  touchIsPressed = touchPressed();
  if (touchIsPressed && !touchWasPressed) {
    ret = true;
    touchWasPressed = true;
  } else if (!touchIsPressed) {
    touchWasPressed = false;
  }
  return ret;
}

bool touchPressed() {
  //Return true if head touch button is pressed
  if ((ADCTouch.read(THEAD, TSENSESAMPLES) - refHead) > TACTIVATE) {
    return true;
  }
  return false;
}

void checkIntensityScroll() {
  if (powerState == 1) {
    //Check base button to scroll intensity up or down
    headIsTouched = ((ADCTouch.read(TBASE, TSENSESAMPLES) - refBase) > TACTIVATE);
    if (headIsTouched) {
      //Check if this is a new touch - if so, reverse the scroll direction
      if (!headWasTouched) {
        scrollingUp = !scrollingUp;
      }
      //Add or subtract ANIMINC amount to pixel intensity if scrolling up or down
      if (scrollingUp) {
        intensity += ANIMINC; //Use own constant?
      } else {
        intensity -= ANIMINC;//Use own constant?
      }
      //Constrain intensity to be within logical limits
      if (intensity < 0.0) {
        intensity = 0.0;
      } else if (intensity > 1.0) {
        intensity = 1.0;
      }
      //Stop continious touch from counting as a new
      headWasTouched = true;
    } else {
      //Reset wasTouched to allow levels to be reversed again.
      headWasTouched = false;
    }
  }
}

//NEOPIXEL
void setFromTemperature(int temperature, float intensity) {
  //Move the actual intensity of the pixels (movIntensity) towards the target intensity (intensity)
  //This creates a smooth transition from one intensity to another
  //If the difference between intensity is larger than the animation change, change the actual intensity in that direction.
  if (intensity > movIntensity && (intensity - movIntensity) > ANIMINC) {
    movIntensity += ANIMINC;
  } else if (intensity < movIntensity && (movIntensity - intensity) > ANIMINC) {
    movIntensity -= ANIMINC;
    //Else if the change is smaller than the animation change, just set it directly to the intensity value.
    //This stops continious jittering between two values on each side of intensity
  } else if (intensity > movIntensity && (intensity - movIntensity) <= ANIMINC) {
    movIntensity = intensity;
  } else if (intensity < movIntensity && (movIntensity - intensity) <= ANIMINC) {
    movIntensity = intensity;
  }

  //Calculate the red green and blue base values
  float r = 255.0;
  //Blue is calculated with formula - see function
  float b = calcBlue(temperature);
  //Green is interpolated linearly from red to blue, using their primary wavelength as positions
  float g = map(gwl, rwl, bwl, r, b);
  //bool doUpdate = false; <-- Old stuff, ignore
  //if (doUpdate) {

  //Dither the pixels with base values times the actual intensity
  ditherPixels(r * movIntensity, g * movIntensity, b * movIntensity);
  //}
}

float calcBlue(int temperature) {
  //Using externally calculated constants, calculate the fitting amount of blue for a specific temperature
  return temperature * TEMPA + TEMPB;
}

//OLD + Unused
/*void setPixels(int r, int g, int b) {
  // constrain the values to 0 - 255
  r = constrain(r, 0, 255) * powerState;
  g = constrain(g, 0, 255) * powerState;
  b = constrain(b, 0, 255) * powerState;

  for (int pixnum = 0; pixnum < NUMPIXELS; pixnum++) {
    pixels.setPixelColor(pixnum, pixels.Color(r, g, b));
    pixels.show();
    //printPixels(r,g,b);
  }
  }*/

void ditherPixels(float r, float g, float b) {
  int sr, sg, sb;

  //Calculate floor and ceiling values of rgb
  int rHi = ceil(r);
  int rLo = floor(r);
  //Isolate decimal part of float and multiply for pixel count
  //This lets extra detail of float be represented in a fitting amount of pixels being slightly brighter
  int rPC = int((r * 10.0 - float(rLo * 10)) * 1.4);

  int gHi = ceil(g);
  int gLo = floor(g);
  int gPC = int((g * 10.0 - float(gLo * 10)) * 1.4);

  int bHi = ceil(b);
  int bLo = floor(b);
  int bPC = int((b * 10.0 - float(bLo * 10)) * 1.4);

  //Iterate through all pixels
  for (int pixnum = 0; pixnum < NUMPIXELS; pixnum++) {
    //if current pixel is within count of those brighter, set it to the ceil value, else floor
    if (pixnum < rPC) {
      sr = constrain(rHi, 0, 255);
    } else {
      sr = constrain(rLo, 0, 255);
    }

    if (pixnum < gPC) {
      sg = constrain(gHi, 0, 255);
    } else {
      sg = constrain(gLo, 0, 255);
    }

    if (pixnum < bPC) {
      sb = constrain(bHi, 0, 255);
    } else {
      sb = constrain(bLo, 0, 255);
    }
    //Set each pixel at their mapped position to the calculated color/brightness
    pixels.setPixelColor(pixelMap[pixnum], pixels.Color(sr, sg, sb));
    //printPixels(sr,sg,sb);
  }
  //Push new ligt settings to pixels
  pixels.show();
}

//EXTRA
//Prints pixel state - not currently in use
void printPixels(int r, int g, int b) {
  Serial.print(r);
  Serial.print(" ");
  Serial.print(g);
  Serial.print(" ");
  Serial.println(b);
}
void handleDemoMode() {
  //Enables demoing of the product
  if (touchIsPressed && headIsTouched) {
    demoMode = true;
    demoModeCounter = 0;
  }
  if (demoMode) {
    demoModeCounter += 1;
    //Copy-Pasted from time temperature keeper
    //RED mode clause
    if (redMode) {
      //Just set temperature very low and return
      currentTemperature = 100;
      return;
    }
    int totalTime = demoModeCounter; //Time is now counter

    //Calculate same for sundown, sunrise and interval
    int sDPoint = sDHour * 60 + sDMinute;
    int sRPoint = sRHour * 60 + sRMinute;
    int intSpan = intHour * 60 + intMinute;
    //Halve interval to place sunrise and sundown times in middle of transition
    int hIntSpan = intSpan / 2;

    //Assuming sDPoint is before 0
    //If time is after sunrise transition and before sundown transition, set temperature to maximum
    if (totalTime > (sRPoint + hIntSpan) && totalTime < (sDPoint - hIntSpan)) {
      currentTemperature = targetTemperatureHigh;
      //Else if time is after sundown transition and before sunrise transition, set temperature to minimum
    } else if (totalTime < (sRPoint - hIntSpan) || totalTime > (sDPoint + hIntSpan)) {
      currentTemperature = targetTemperatureLow;
      //Else if time is during sunrise transition, map temperature linearly from min to max through transition time
    } else if (totalTime > (sRPoint - hIntSpan) && totalTime < (sRPoint + hIntSpan)) {
      currentTemperature = map(totalTime, sRPoint - hIntSpan, sRPoint + hIntSpan, targetTemperatureLow, targetTemperatureHigh);
      //Else if time is during sundown transition, map temperature linearly from max to min through transition time
    } else if (totalTime > (sDPoint - hIntSpan) && totalTime < (sDPoint + hIntSpan)) {
      currentTemperature = map(totalTime, sDPoint - hIntSpan, sDPoint + hIntSpan, targetTemperatureHigh, targetTemperatureLow);
    }
  }

  if (demoModeCounter >= 1440) { //Full day
    demoMode = false;
  }
}

