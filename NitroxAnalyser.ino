#include <Esplora.h>
#include <TFT.h>  // Arduino LCD library
#include <SPI.h>



float O2 = 0;
float lastO2 = 1;
boolean calibrated = false;
int screenState = 0;
float cal = 1.0;
int state = 1; //Analyze state: 1: Main, 2: calibrate, 3:hold
int t0 = 0; // Button timer
int buttonDur = 0;
unsigned long lasttime = 0;
boolean hold = false;
boolean printHold = false;
float O2readings[19];

int txtR = 255;
int txtG = 255;
int txtB = 255;





void setup() {
  Serial.begin(9600);
  
  // initialize the screen
  EsploraTFT.begin();
  // clear the screen with black
  Serial.println("Clear screen");
  EsploraTFT.background(0,0,0); 
}

void loop() {
  while (Esplora.readButton(1) == LOW) {
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
       } else {
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
    int i;
    for (i=0 ; i <= 18 ; i++) {
      O2readings[i] = O2readings[i+1];      
      Serial.print(O2readings[i]);
      Serial.print("\t");
    }
    O2readings[19] = O2;
    Serial.println(O2readings[19]);
 }
    
    //Print serial
    Serial.print("O2: ");
    Serial.println(O2);
    
    
if (calibrated == true){
    printO2toTFT(O2,lastO2); 
} else {
    printRAWValueToTFT(O2,lastO2);
}
    //update last printed value
    lastO2 = O2;
    lasttime = millis();
}


/*-------------------------------------------------------
Reads value from o sensor. 
If sensor is calibrated, the outut is calibrated value
---------------------------------------------------------*/
float getO2value(float cal) {
  //Read sensor
  int rawO2 = Esplora.readLightSensor();
  
  //Print serial
  Serial.print("Raw O2: ");
  Serial.println(rawO2);
  Serial.println(calibrated);
  
  //Output value
  if (calibrated == true) {
   O2 = rawO2 * cal;
   // round to one decimal point
   O2 = ((int)(O2 * 10 ))/ 10.0;
  } else {
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
EsploraTFT.background(0,0,0);  //Clear screen
EsploraTFT.stroke(255,255,255);  //While text
EsploraTFT.setTextSize(2);
EsploraTFT.text("Calibrating",0,0);
screenState = 3;


// Read sensor
int valMin = 1023;
int valMax = 0;
boolean calOK = false;

for (int i = 1; i<=nCount;i++) {
int val = Esplora.readLightSensor();

if (val < valMin) {valMin = val;}
if (val > valMax) {valMax = val;}


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
    EsploraTFT.stroke(0,255,0);
    EsploraTFT.text("OK!",0,30);
  } else {
    // or return to uncalibrated state
   calibrated = false;
  cal = 1.0; 
      EsploraTFT.stroke(255,0,0);
  EsploraTFT.text("Error!",0,30);
  }
  delay(800);
  
  lastO2 = 0;
  
  return cal;   
}

/*--------------------------------------------------------
Prints calibrated O2 level to TFT
--------------------------------------------------------*/

void printO2toTFT (float O2,float lastO2) {
  if (screenState != 2) {
    //Initiate main screen
        EsploraTFT.background(0,0,0);    //Clear screen
        EsploraTFT.stroke(255,255,255);  // Txt color white
        EsploraTFT.setTextSize(2);
        EsploraTFT.text("pO2:",0,0);    
        //Write calibration factor    
        EsploraTFT.setTextSize(1);
        EsploraTFT.text("Cal:",0,120);        
        float2TFT(0.0,cal,
                  30,120,
                  6,5,
                  255,255,255,
                  0,0,0);
//        char calprintout[6];
//        dtostrf(cal, 1, 5, calprintout);
//        EsploraTFT.text(calprintout,30,120);
        screenState = 2;     
  }
  
  if (hold == false){
        txtR = 255;
        txtG = 255;
        txtB = 255; }  // Txt color white
        else { 
        txtR = 0;
        txtG = 255;
        txtB = 0; }  // Txt color blue

        
       
  
  
  if (O2 != lastO2) {
    
    EsploraTFT.setTextSize(6);
    float2TFT(O2,lastO2,     //New and old value
              0,30,          //Xpos, Ypos
              4,1,           //Width, precision
              0,0,0,         //Bg color
              txtR,txtG,txtB);  //txt color;    
              

  }
  }
  
  void printRAWValueToTFT (float O2,float lastO2) {
  if (screenState != 1) {
    //Initiate main screen
        EsploraTFT.background(0,0,0);  //Clear screen
        EsploraTFT.stroke(255,255,255); // Txt color white
        // set the text to size 2
        EsploraTFT.setTextSize(2);
        // start the text at the top left of the screen
        // this text is going to remain static
        EsploraTFT.text("Sensor mV:\n ",0,0);

        
        EsploraTFT.setTextSize(1);
        EsploraTFT.text("Push and hold to calibrate",0,116);        
        screenState = 1;
        
  }
  
  if (hold == false){
        txtR = 255;
        txtG = 255;
        txtB = 255; }  // Txt color white
        else { 
        txtR = 0;
        txtG = 255;
        txtB = 0; }  // Txt color blue
  
  if (O2 != lastO2) {
    Serial.print(O2);
    EsploraTFT.setTextSize(5);
    float2TFT(O2,lastO2,0,30,3,0,0,0,0,txtR,txtG,txtB);    
  }
  }
  
  
  
  // Print value to TFT
  void float2TFT(float value,float oldValue,int posX, int posY, int width, int precision, int bgR, int bgG, int bgB, int txtR, int txtG, int txtB) {
    //Prepare char arrays
    char valPrintout[width+1];
    dtostrf(value, width, precision, valPrintout);
    char oldValPrintout[width+1];
    dtostrf(oldValue, width, precision, oldValPrintout);
    
    //If arrays has changed, update
    Serial.print(valPrintout);
    Serial.print("\t");
    Serial.print(oldValPrintout);
    if (valPrintout != oldValPrintout) {
      Serial.print("\t Different!!");
    }
    Serial.println();
    if (valPrintout != oldValPrintout) {
      Serial.println("Writing to TFT");
      //Clear screen of old print
      EsploraTFT.stroke(bgR,bgG,bgB);
      EsploraTFT.text(oldValPrintout,posX,posY);
      EsploraTFT.stroke(txtR,txtG,txtB);
      EsploraTFT.text(valPrintout,posX,posY);
    }
        

    
  }
