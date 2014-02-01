#include <TFT.h>  // Arduino LCD library
#include <SPI.h>
#include <Average.h> // math
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
//#include <avr/sleep.h>


//Init adc
Adafruit_ADS1115 adc;  /* Use this for the 16-bit version */

//pin definition TFT
#define SD_CS  4
#define LCD_CS 10
#define DC     9
#define RST    8 

//Init screen
TFT TFTscreen = TFT(LCD_CS, DC, RST);

//pin definiiton Button
#define BUTTON 7


// Initialize variables
float O2 = 0;                    //O2 sensor value
float lastO2 = 1;                //Previous O2 reading for screen update
boolean calibrated = false;      //Is sensor calibrated
int screenState = 0;             //State of the screen
//1: Main view
//2: Calibratin
//3: 
float cal = 1.0;                 //Calibration factor
float oldCal = 0;
unsigned long calTime = 0;      //Time for last calibration
unsigned long calTimeElapsed = 0; //Time since last calibration
int state = 1;                   //Analyze state:
//1: Main
//2: calibrate
//3:hold
int t0 = 0;                      // Button timer
int buttonState[2] = {
  0};             // Button state (old and new)
int buttonDur = 0;              // Duration of push
unsigned long lasttime = 0;      // last update time
boolean hold = false;            //hold activated=
boolean printHold = false;        //did hold value print
float O2readings[20] = {          // Keep last 20 readings in memory
  0};
boolean stable = false;          //Is reading stable?
float lastStable[2] = {
  0};
int nReadings = 20;


int bg[3] = {                  //default bg coloar
  0,0,0};
int txt[3] = {                //txt
  255,255,255};

// create default colors
int white[] = {
  255,255,255};
int red[] = {
  255,0,0};
int green[] = {
  0,255,0};
int blue[] = {
  0,0,255};

unsigned long lastActive = 0;
//PImage logo;
void setup() {

  //Start serial communication
  Serial.begin(9600);

  // initialize the pushbutton pin as an input:
  pinMode(BUTTON, INPUT_PULLUP);     


  //Start adc
  adc.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  adc.begin();
  #define GAIN 0.015625  // mV/bit

  // initialize the screen
  TFTscreen.begin();
  // clear the screen with black
  Serial.println("Clear screen");
  TFTscreen.background(0,0,0); 

  /*
  //Initialize SD
   Serial.print("Initializing SD card...");
   // make sure that the default chip select pin is set to
   // output, even if you don't use it:
   pinMode(SD_CS, OUTPUT);
   // see if the card is present and can be initialized:
   if (!SD.begin(SD_CS)) {
   Serial.println("Card failed, or not present");
   sd = false;
   } else {
   Serial.println("card initialized.");
   sd = true;
   }
   */

  // Read calibration factor from EEPROM
  EEPROM_readAnything(0,cal);
  calibrated = true;

}

void loop() {
  // Check if someone pushed the button
  readButton();

/*
  //Check if iet is time to power down
  //Serial.print("Active Time: ");Serial.println(millis()-lastActive);
  if (millis()-lastActive > 10000){
  
    Serial.println("");    Serial.println("");    Serial.println("");
        Serial.println("-----------------------------------");
            Serial.println("Powering Down.......");
            delay(2000);
    
    //Power down
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable(); 
//    attachInterrupt(0,wakeUpNow, LOW);
    sleep_mode();            // here the device is actually put to sleep!!
                             // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP
                            
 }
*/
  // Check if it is time to update anything
  if (millis() - lasttime < 250) {
    //do nothing
    return;
  }


  if (hold == false) {
    //Read sensor
    O2 = getO2value(cal);

    //store in array og the 20 last readings

    for (int i=0 ; i <= nReadings-2 ; i++) {
      O2readings[i] = O2readings[i+1];      
    }
    O2readings[nReadings-1] = O2;
  }


  stable = isStable(O2readings,nReadings);
  if (stable == true) {
    lastStable[0] = lastStable[1];
    lastStable[1] = O2;
  }

  //Print serial
  Serial.print("O2: ");
  Serial.println(O2,5);


  if (calibrated == true){
    printO2toTFT(O2,lastO2,stable); 
  } 
  else {
    printRAWValueToTFT(O2,lastO2);
  }
  //update last printed value
  lastO2 = O2;
  lasttime = millis();
}

/*----------------------------------------------------
 Read 02 sensor from ADC
 -----------------------------------------------------*/
float readSensor() {
  //Read 16 bit value from pin 0 and multiply by gain
  float val = adc.readADC_SingleEnded(0)*GAIN;

  //This is reading in mV
  return val;
}


/*-------------------------------------------------------
 Reads value from o sensor. 
 If sensor is calibrated, the outut is calibrated value
 ---------------------------------------------------------*/
float getO2value(float cal) {
  //Read sensor
  float rawO2 = readSensor();

  //Print serial
  Serial.print("Raw O2: ");
  Serial.println(rawO2);
  Serial.println(calibrated);

  //Output value
  if (calibrated == true) {
    O2 = rawO2 * cal;
    // round to one decimal point
    O2 = ((int)(O2 * 10 ))/ 10.0;
  } 
  else {
    O2 = rawO2;
  }


  return O2;
}


/*-----------------------------------------
 Calibrates the sensor:
 Returns the slope, so current value is 20.9
 ------------------------------------------*/
float calibrate() {
  oldCal = cal;
  int nSample = 20;      //Number of samples
  int n = 0;              //Sample counter
  int delayTime = 250;  //Delay beween sampe
  unsigned long readTime = 0;     //Time of last reading

  Serial.println("Calibrating sensor to 20.9");


  TFTscreen.background(0,0,0);
  TFTscreen.stroke(255,255,255);  //While text
  TFTscreen.setTextSize(2);
  TFTscreen.text("Calibrating",0,0);
  screenState = 3;

  while (n < nSample) {
    if (millis()-readTime >= delayTime) {
      //store in array, log the 20 last readings
      O2readings[n] = readSensor();
      Serial.print(O2readings[n]);
      Serial.print("\t");
      //increment counter
      n++;
      //update read time
      readTime = millis();

    }
  }
  Serial.println(" ");

  //Determine if reading is stable
  float stdRead = stddev(O2readings,nSample);
  float avgRead = mean(O2readings,nSample);
  Serial.print("STD: "); 
  Serial.println(stdRead);
  Serial.print("AVG: "); 
  Serial.println(avgRead);
  //If std is less than 0.02 mV then reading is ok!
  if (stdRead < 0.02) {
    stable = true;
  } 
  else{
    stable = false;
  }

  //Calculate calibration factor
  if (stable == true) {
    cal = 20.9/avgRead;
    Serial.print("Cal: "); 
    Serial.println(cal);
    calibrated = true;
    TFTscreen.stroke(green[0],green[1],green[2]);
    TFTscreen.text("OK!",0,30);
  } 
  else {
    // or return to uncalibrated state
    calibrated = false;
    cal = 1.0; 
    TFTscreen.stroke(red[0],red[1],red[2]);
    TFTscreen.text("Error!",0,30);
  }
  // Show result for a short time
  delay(800);

  // force screen to update
  lastO2 = 0;
  lastStable[1] = 0;

  /*
  // Save calibration to SD card
   //Open file
   File dataFile = SD.open("cal.log", FILE_WRITE);
   // if the file is available, write to it:
   if (dataFile) {
   dataFile.println(cal,6);
   dataFile.close();
   }
   
   */

  EEPROM_writeAnything(0,cal);

  //Save time for calibration
  calTime = millis();
  //Retun calibration factor
  return cal;   

}

/*--------------------------------------------------------
 Prints calibrated O2 level to TFT
 --------------------------------------------------------*/

void printO2toTFT (float O2,float lastO2,boolean stable) {
  if (screenState != 2) {
    //Initiate main screen
    TFTscreen.background(0,0,0);    //Clear screen
    TFTscreen.stroke(255,255,255);  // Txt color white
    TFTscreen.setTextSize(2);
    TFTscreen.text("pO2:",0,0);    
    //Write calibration factor    
    TFTscreen.setTextSize(1);
    TFTscreen.text("Cal:",0,120);

    float2TFT(0,cal,
    25,120,
    6,5,
    white,
    bg);

    if (oldCal != 0){
      TFTscreen.stroke(255,255,255);  // Txt color white
      TFTscreen.text("(",75,120);

      float2TFT(-1,oldCal,
      81,120,
      6,5,
      white,
      bg);    
      TFTscreen.stroke(255,255,255);  // Txt color white
      TFTscreen.text(")",123,120);
    }

    screenState = 2;     
  }

  Serial.print("is stable: ");    
  Serial.println(stable);


  if (stable == true){
    txt[0] = 0;
    txt[1] = 255;
    txt[2] = 0; //txt color green
    Serial.println("Text green");
  } 
  else {
    txt[0] = 255;
    txt[1] = 0;
    txt[2] = 0; //txt color green

    Serial.println("Text red");
  }

  /*
  Serial.print("Text color ");
   Serial.print(txt[0]);
   Serial.print("  ");
   Serial.print(txt[1]);
   Serial.print("  ");
   Serial.println(txt[2]);
   */

  TFTscreen.setTextSize(6);
  float2TFT(O2,lastO2,     //New and old value
  0,25,          //Xpos, Ypos
  4,1,           //Width, precision
  bg,         //Bg color
  txt);  //txt color; 

  if (lastStable[1] != 0 && lastStable[1] != lastStable[0] && (lastStable[1] < 20.6 || lastStable[1] >21.4)) {
    Serial.println("Last stable value: "); 
    Serial.println(lastStable[1]);
    TFTscreen.setTextSize(4);
    float2TFT(lastStable[1],lastStable[0],
    0,82,
    4,1,
    bg,
    white);
  }
}


/*-------------------------------------------------------
 Print mV reading to TFT
 --------------------------------------------------------*/

void printRAWValueToTFT (float O2,float lastO2) {
  if (screenState != 1) {
    //Initiate main screen
    TFTscreen.background(0,0,0);  //Clear screen
    TFTscreen.stroke(255,255,255); // Txt color white
    // set the text to size 2
    TFTscreen.setTextSize(2);
    // start the text at the top left of the screen
    // this text is going to remain static
    TFTscreen.text("Sensor mV:\n ",0,0);


    TFTscreen.setTextSize(1);
    TFTscreen.text("Push and hold to calibrate",0,116);        
    screenState = 1;

  }

  if (hold == false){
    int txt[] = {
      255,255,255                                };
  }  // Txt color white
  else { 
    int txt[] = {
      0,0,255                                };
  }  // Txt color blue

  if (O2 != lastO2) {
    Serial.print(O2);
    TFTscreen.setTextSize(5);
    float2TFT(O2,lastO2,0,30,3,0,bg,txt);    
  }
}



/*----------------------------------------------------
 Print value to TFT
 ------------------------------------------------------*/
void float2TFT(float value,float oldValue,int posX, int posY, int width, int precision, int*  bg, int* txt) {
  //Prepare char arrays
  char valPrintout[width+1];
  dtostrf(value, width, precision, valPrintout);
  char oldValPrintout[width+1];
  dtostrf(oldValue, width, precision, oldValPrintout);

  //If arrays has changed, update
  Serial.print(value); 
  Serial.print("\t"); 
  Serial.println(oldValue);
  if (value != oldValue) {
    Serial.print("Different!!");
  }
  Serial.println(" ");


  if (value != oldValue) {
    //Update screen
    Serial.println("Writing to TFT");
    //Clear screen of old print
    TFTscreen.stroke(bg[0],bg[1],bg[2]);
    TFTscreen.text(oldValPrintout,posX,posY);
  }
  //write new print
  Serial.print("Text color ");
  Serial.print(txt[0]);
  Serial.print("  ");
  Serial.print(txt[1]);
  Serial.print("  ");
  Serial.println(txt[2]);
  TFTscreen.stroke(txt[0],txt[1],txt[2]);
  TFTscreen.text(valPrintout,posX,posY);




}
/*********************************************************
 * Determine if last x seconds reading is stable
 ********************************************************/

boolean isStable(float * O2readings,int nReadings) {

  // get parameters
  float minRead = minimum(O2readings,nReadings);
  float maxRead = maximum(O2readings,nReadings);
  float stdRead = stddev(O2readings,nReadings);
  float avgRead = mean(O2readings,nReadings);

  //Determine if reading is stable (Std less than 0.1)
  if (stdRead <= 0.05) {
    return true;  
  } 
  else {
    return false;
  }
}



/*********************************************************
 * Read Button
 *********************************************************/
void readButton(){
  //Read button
  buttonState[0] = buttonState[1];
  buttonState[1] = digitalRead(BUTTON);

  //If button went from high to low, a push has started
  if (buttonState[0] == HIGH && buttonState[1] == LOW){
    //If button went from low to high, a push has started
    //Save start time
    t0 = millis();
      lastActive = millis();
  } 
  else if (buttonState[0] == LOW && buttonState[1] == LOW){
    //Button is still pressed. How long time now?
    buttonDur = millis()-t0;

    if (buttonDur > 1500 ) {
      //If long push: Calibrate
      calibrate();
      // Reset time
      buttonDur = 0;
      t0 = 0;
      hold = false;
    }
      lastActive = millis();
  } 
  else if (buttonState[0] == LOW && buttonState[1] == HIGH){
    buttonDur = millis()-t0;
    // Button is released in a short press
    // Change hold state
    if (buttonDur < 1500){
      if (hold == false) {
        hold = true;
        lastO2 = 0.0;
      } 
      else {
        hold = false;
      }
      //Reset timer 
      buttonDur = 0;
      t0 = 0;
    }
    lastActive = millis();  
  }
  

}



