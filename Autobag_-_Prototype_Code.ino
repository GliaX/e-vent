//Include libraries
#include <Wire.h>
#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header

hd44780_I2Cexp  lcd; // declare lcd object: auto locate & auto config expander chip

const int DIR = 9; //DIR on controller to pin 9 on arduino
const int STEP = 10;  //PUL on controller to pin 10 on arduino
const int LCD_COLS = 20;
const int LCD_ROWS = 4;

//Input pins
int BPM = A0; //Desired bpm
int Ratio = A1; //Desired I/E ratio potentiometer (percentage of bag compression, from UofT code)
int VolumeT = A2; //Desired tidal volume of compression potentiometer
int switchU = 7; //Upper limit switch
int startSwitch = 8; //Start switch, latching

//Output pins
//int buzzer = 1; //Buzzer 
int contrast = A3;

//Variables to edit configurations below, you can tweak some settings here
int cycleTmin = 5; //Minimum allowable bpm
int cycleTmax = 30; //Maximum allowable bpm
float tidalVmin = 50.00; //Minimum compression percentage
float tidalVmax = 100.00; //Maximum compression percentage
int ieRatiomin = 100; //Minimum ratio of inspiration:expiration
int ieRatiomax = 300; //Maximum ratio of inspiration:expiration
float maxStrokeFinger = 42.0; //Maximum allowable stroke of finger (degrees)
float gearR = 16; //Gear Ratio between stepper and finger 
float muStep = 16; // microstep setting of motor driver (1, 2, 4, …etc) 

//Set-points
int cycleT = 0; //Cycle time duration set by user

//Timers
int cycleCount = 0; //Every 100 cycles, home

//Variables
float tidalVolume = 0; //Tidal volume setpoint
int stepperStroke = 0; //Commanded step count of stepper, calculated from tidalVolume
float ieRatio = 0; //inspiration:expiration set-point
bool estopLatch = 0; //Set E-stop to default off at bootup
int freq_in = 0;
int freq_ex = 0;
int stepsCycle = 0; //number of motor steps per cycle 
long rotTime = 0; 
float rotTime_in = 0; 
float rotTime_ex = 0; 
unsigned long startTime = 0;

//Cycle watchdog
bool activeCycle = 0;

void configureLCD() {
  lcd.begin(20,4);               // initialize the lcd 
  lcd.home();                   // go home
  lcd.clear();
}

void readInputs() {
    //Serial.println("Reading inputs"); //This will flood the Serial port; only use if debugging
    cycleT = map(analogReadAvg(BPM),0,1000,cycleTmin,cycleTmax); //Reads the analog pot value and interpolates to the target range defined by the editable variables above. This can be bpm
    ieRatio = (map(analogReadAvg(Ratio),0,1000,ieRatiomin,ieRatiomax)/100.0);
    tidalVolume = map(analogReadAvg(VolumeT),0,1000,tidalVmin, tidalVmax)/100.00;
    if (tidalVolume > 1.0) {
      tidalVolume = 1.0;
      }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("BPM:");
    lcd.setCursor(16,0);
    lcd.print(cycleT);
    lcd.setCursor(0,1);
    lcd.print("IE Ratio:");
    lcd.setCursor(16,1);
    lcd.print(ieRatio, 1);
    lcd.setCursor(0,2);
    lcd.print("Tidal Volume %:");
    lcd.setCursor(16,2);
    lcd.print(int(tidalVolume*100));
    lcd.setCursor(0,3);
    //Remap tidal volume to new floor
    tidalVolume = map(analogReadAvg(VolumeT),0,1000,70, tidalVmax)/100.00; //Remaps "50%" position of compression
    //Calculate cycle time parameters
    rotTime = (1000.0 * 60.0) / cycleT; // tone(pin, frequency, duration), where duration is in milliseconds 
    rotTime_in = rotTime / (1 + ieRatio); 
    rotTime_ex = rotTime - rotTime_in; 
    stepsCycle = (tidalVolume * maxStrokeFinger * gearR ) * (muStep / 1.8); // motor steps per cycle 
    freq_in = stepsCycle * (1000.0 / rotTime_in); // frequency for compression stroke in Hz 
    freq_ex = stepsCycle * (1000.0 / rotTime_ex); // frequency for compression stroke in Hz  
}

int analogReadAvg(int pin) {  //Stolen from UofT
  int analogSum = 0;
  for (int i = 0; i < 5; i++) {
    analogSum += analogRead(pin);
    delay(5);
  }
  return analogSum / 5;
}

void estop() { //Not implemented in this revision
    //digitalWrite(buzzer,HIGH);
    noTone(STEP);
    if (estopLatch == 0) {
      estopLatch = 1;
      lcd.clear();
      lcd.setCursor(3,1);
      lcd.print("ERROR DETECTED");
      lcd.setCursor(3,2);
      lcd.print("PLEASE REBOOT");
    }
}

void homeStepper() {  //Homing function to be updated based on more consistent microswitch
  Serial.println("Homing Stepper");
  lcd.print("Homing Stepper");
  digitalWrite(DIR, HIGH);
  tone(STEP, 750);  //230 works for slow motion at 1/16 mu
  //Serial.println(digitalRead(switchU));
  while (digitalRead(switchU)) {
    //Serial.println("homing");
    delay(10);
  }
  noTone(STEP);
  if (digitalRead(switchU)) {
    homeStepper();
  }
}

void setup() {
    Serial.begin(9600); //Start serial communication for debugging
    configureLCD();
    Serial.println("Starting up...");
    lcd.print("Starting up...");
    pinMode(switchU, INPUT_PULLUP);
    pinMode(startSwitch, INPUT_PULLUP);
    pinMode(STEP, OUTPUT);
    pinMode(DIR, OUTPUT);
    pinMode(contrast, OUTPUT);
    analogWrite(contrast, 80);
    lcd.clear();
    homeStepper();
    Serial.println("Stepper homed");
    lcd.clear();
    lcd.print("Stepper homed"); 
}

void loop() {
    if (!digitalRead(startSwitch) && ! estopLatch) {
        if (activeCycle == 0) {
          activeCycle = 1;
        }        
        Serial.println("inspiration");
//        Serial.println(rotTime);
//        Serial.println(rotTime_in);
//        Serial.println(freq);
        digitalWrite(DIR, LOW);
        tone(STEP, freq_in); // Run stepper at calculated frequency for full compression time
        startTime = millis();
      
        // Run the following code during the compression stroke
        while (millis() <= (startTime + rotTime_in)) {
//          while (digitalRead(switchL == HIGH)) {  //Error detection, not implemented, define input if required
//            if (millis() - startTime > 3000){
//              estop();
//            }
//          }
            delay(10);
        }
//        Serial.println(millis());
//        Serial.println(startTime+rotTime_in);
        //delay(rotTime_in);
        noTone(STEP);
        delay(50);
  
        
        Serial.println("expiration");
//        Serial.println(rotTime_ex);
        //Serial.println(freq);
        digitalWrite(DIR, HIGH);  // Set direction to reverse
        tone(STEP, freq_ex);  // Run motor at calculated speed and time
        startTime = millis();
//        if (!digitalRead(switchU)) {
//          noTone(STEP);
//        }
//        delay(rotTime_ex);
        while (digitalRead(switchU) == HIGH) {
        //while (millis() <= (startTime + rotTime_ex + 100.0)) {
 
          if (!digitalRead(switchU)) {  // Stop motor if limit switch reached 
            Serial.println("Exiting loop");
            break;
          }  
          delay(50);        
        }
        Serial.println(digitalRead(switchU));
        noTone(STEP);
      }
      if (digitalRead(startSwitch)) { //No active cycle and confirm button pushed
          if (activeCycle ==1 ) {
            homeStepper();  //Runs once when transitioning from active to inactive, use the chance to open the fingers
            activeCycle = 0;
          }
          readInputs();
          delay(500);
      }
      delay(50);
}
