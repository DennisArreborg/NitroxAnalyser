#include <TFT.h>  // Arduino LCD library
#include <SPI.h>
#include <Average.h> // math
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <SD.h>

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
float O2 = 0;
float lastO2 = 1;
boolean calibrated = false;
int screenState = 0;
float cal = 1.0;
int state = 1; //Analyze state: 1: Main, 2: calibrate, 3:hold
int t0 = 0; // Button timer
int buttonState = 0;
int buttonDur = 0;
unsigned long lasttime = 0;
boolean hold = false;
boolean printHold = false;
float O2readings[20] = {
  0};
boolean stable = false;
int nReadings = 20;


int bg[3] = {
  0,0,0};
int txt[3] = {
  255,255,255};

void setup() {
  
  //Start serial communication
  Serial.begin(9600);
  
  //Start adc
    adc.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
    adc.begin();
    #define GAIN 0.015625
    
    // Start TFT

 // initialize the screen
  TFTscreen.begin();
  // clear the screen with black
  Serial.println("Clear screen");
  TFTscreen.background(0,0,0); 

}

void loop() {
  //Read buttopn

  while (digitalRead(BUTTON) == LOW) {
    // If the button is pressed, determine the lengt of the push
    if (t0 == 0) {
      t0 = millis();
    }    
    buttonDur = millis()-t0;
    if (buttonDur > 1500 ) {
      //If long push: Calibrate
      calibrate();
      buttonDur = 0;
      t0 = 0;
      hold = false;
    }
  }
  if (buttonDur > 0) {
    // If short push: Hold value
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
    Serial.println("stable");
  }

  //Print serial
  Serial.print("O2: ");
  Serial.println(O2);


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
  int rawO2 = readSensor();

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
  int nCount = 20;      //Number of samples
  int delayTime = 250;  //Delay beween sampe
  int readings = 0;     //Sum of samples

  Serial.println("Calibrating sensor to 20.9");


  //Write to TFT: Initiate screen state 3
  TFTscreen.background(0,0,0);  //Clear screen
  TFTscreen.stroke(255,255,255);  //While text
  TFTscreen.setTextSize(2);
  TFTscreen.text("Calibrating",0,0);
  screenState = 3;


  // Read sensor
  int valMin = 1023;
  int valMax = 0;
  boolean calOK = false;

  for (int i = 1; i<=nCount;i++) {
    int val = readSensor();

    if (val < valMin) {
      valMin = val;
    }
    if (val > valMax) {
      valMax = val;
    }


    readings = readings + val;
    delay(delayTime);
  }


  // Reading is mean of readins
  float reading = readings/nCount;
  Serial.println(reading);

  // Check for variation
  if (valMax-valMin < reading*0.01 ) {
    calOK = true;
  }

  //Calculate calibration factor
  if (calOK == true) {
    cal = 20.9/reading;
    Serial.println(cal);
    calibrated = true;
    TFTscreen.stroke(0,255,0);
    TFTscreen.text("OK!",0,30);
  } 
  else {
    // or return to uncalibrated state
    calibrated = false;
    cal = 1.0; 
    TFTscreen.stroke(255,0,0);
    TFTscreen.text("Error!",0,30);
  }
  delay(800);

  lastO2 = 0;

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

    int txt[] = {255,255,255};    
    float2TFT(0.0,cal,
    30,120,
    6,5,
    txt,
    bg);
    screenState = 2;     
  }

/*
if (hold == false){
    int txt[] = {
      255,255,255    };
  } 
  else { 
    int txt[] = {
      0,100,255    };
  }  // Txt color blue
*/

  Serial.print("is stable: ");    
  Serial.println(stable);


  if (stable == true){
    int txt[] = {
      0,255,0    }; //txt color green
      Serial.println("Text green");
  } 
  else {
    int txt[] = {
      255,0,0    }; // Txt color red
      Serial.println("Text red");
  }


Serial.print("Text color ");
  Serial.print(txt[0]);
  Serial.print("  ");
    Serial.print(txt[1]);
      Serial.print("  ");
      Serial.println(txt[2]);
  
  TFTscreen.setTextSize(6);
  float2TFT(O2,lastO2,     //New and old value
  0,30,          //Xpos, Ypos
  4,1,           //Width, precision
  bg,         //Bg color
  txt);  //txt color; 
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
      255,255,255    };
  }  // Txt color white
  else { 
    int txt[] = {
      0,0,255    };
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
  Serial.println(valPrintout);
  Serial.println(oldValPrintout);
  if (valPrintout != oldValPrintout) {
    Serial.print("Different!!");
  }
  Serial.println();

  if (value != oldValue) {
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
  if (stdRead <= 0.1) {
    return true;  
  } 
  else {
    return false;
  }
}



