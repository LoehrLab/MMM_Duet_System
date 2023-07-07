#include "driver/rtc_io.h"
#include "BluetoothSerial.h" 
#include <OSCMessage.h>


#define IS 32 // Index Signal
#define AS 15 // A Signal
#define BS 33 // B Signal
#define Light 13 // LED 
#define BatteryVoltage 35 // Battery voltage has to be read from a non-exposed pin A13/35

const uint16_t maxJumpInTime = 5000; 
int stateA;
int stateB;
int previousStateA;

uint16_t val; // The difference between current time from start and last recorded movement
uint16_t prevval; // Previous difference of val
uint16_t wait = 0;       //new
unsigned long currentTimeSinceStart = 0; // Number of milliseconds since program start
unsigned long lastRecordedTime = 0;  
unsigned long timeOfLastRecordedMovement = 0; // Milliseconds from start of last recorded movement

const unsigned long inactionMaxDuration = 150000; //Maximum length of time before entering deep sleep mode (5min)

float currentVoltage;
const float adcReferenceVoltage = 1.1; // Specific to the ESP32 pin, converts anolog voltage to a digital number (1100mV).
const float esp32ReferenceVoltage = 3.3; // ESP32 board specific
const int adcConversion = 4028; // ADC value of the ESP32 is a 12-bit number and this converts it to decimal
const float minimumVoltageAlert = 3.3; // ESP32 disables battery at this level 
bool ledErrorCode[] = {false,false}; // Variable to set the amount of blinking for LED light. Array position: 0 = bluetooth connection established, sufficient battery 
unsigned long ledRecordedTime = 0; // Used for LED flashing, not rotation calculation
unsigned long batteryTestInterval = 60000;
unsigned long ledInterval; //Interval between blinks based on error code
int ledState; //State of LED, High or low

BluetoothSerial ESP_BT; //Object for Bluetooth

void setup(){
  Serial.begin(115200);
  
  
  if(!ESP_BT.begin("MMM_Box_Blue")){ // Bluetooth Name    
    ledErrorCode[0] = false;
  } else {
    // More conditions need to be added for connectivity error code to be considered implemented
    ledErrorCode[0] = true;
  }
  
  pinMode (AS, INPUT); 
  pinMode (BS, INPUT);
  pinMode(Light, OUTPUT); 
 
  previousStateA = digitalRead(AS); // Read the initial state
  Serial.print("Current voltage: "); Serial.println(checkVoltage()); //Get initial voltage of battery and provide errorcode if necessary

  esp_sleep_enable_ext0_wakeup((gpio_num_t)GPIO_NUM_14, 1);  //Sets the 14 pin as the wakeup pin
  
}


/**
 * Returns current voltage of battery and if below threshold update error code. 
 */
float checkVoltage(){
    // Voltage of battery (Needs to be doubled from the value at the pin. You can compare this value to that read from the GND and BAT pins
   currentVoltage = 2* analogRead(BatteryVoltage) * adcReferenceVoltage * esp32ReferenceVoltage / adcConversion ; 
   if (currentVoltage < minimumVoltageAlert){
    ledErrorCode[1] = false;
   } else {
    ledErrorCode[1] = true;
   }
   return currentVoltage; 
 }


/**
 * Modifies ledInterval to reflect the error code
 */
void ledPeriod(){
  if (ledErrorCode[0] == false){
    ledInterval = 500; // If there is no bluetooth connection
  } else if (ledErrorCode[0]  && !ledErrorCode[1]){
    ledInterval = 2500; // If there there is a low battery
  } else { // All other cases
    ledInterval =0;
  }
}

/**
 * Checks if sufficient time has elapsed between last recorded movement and current loop
 * to merit entering sleep mode
 */

void checkSleep(){
   //Enter sleep mode if time between spins is greater than inaction max duration
  if (inactionMaxDuration < (currentTimeSinceStart - timeOfLastRecordedMovement)){
    esp_deep_sleep_start();
    }
}

/**
 * Set LED to blink in accordance with error code
 */
void setLEDState (){
  // If both errorcodes are true i.e. no error
  if (ledErrorCode[0] && ledErrorCode[1]){
      digitalWrite(Light, HIGH);
      ledState = HIGH;
    } else {
      ledPeriod();
      if (currentTimeSinceStart - ledRecordedTime >= ledInterval) {
        if (ledState == LOW) {
          digitalWrite(Light, HIGH);
          ledState = HIGH;
        } else {
          digitalWrite(Light, LOW);
          ledState = LOW;
        }
      }
    }
}

void loop(){
  

  currentTimeSinceStart = millis(); // Get milliseconds from start  
  checkSleep(); // Check inactivity
  
  // Update the battery errorcode periodically based on batteryTestInteval
  if ( batteryTestInterval < (currentTimeSinceStart - ledRecordedTime)){
    checkVoltage();
    Serial.println(currentVoltage = 2* analogRead(BatteryVoltage) * adcReferenceVoltage * esp32ReferenceVoltage / adcConversion);
    ledRecordedTime = currentTimeSinceStart; // Set recorded time
    }
    
  setLEDState(); // Set LED to blink if an error code is present
    
  
  
  
  // Get current state
  stateA = digitalRead(AS); // Get state of pin A
  stateB = digitalRead(BS); // Get state of pin B
     


    // If the pin A is up or down and pin B is the opposite consider it a position. 
    // Pin A has to be in a different state than it was previously to consider that movement has occured. 
  if (stateA != stateB && stateA != previousStateA)
    {        
        val = currentTimeSinceStart - timeOfLastRecordedMovement;
        if (val != prevval && val != 0 && val < 40) {       //No need to repeat sending out same info as before. & Pracitcally physically impossible to spin slower than once every 30 msec.
          OSCMessage oscMsg("/a"); // The 30 MSEC ABOVE MAY NEED TO BE CHANGED FOR A SLOWER MUSICBOX. CHECK SLOWEST SPINNING SPEED AND ADJUST TO THAT.
          oscMsg.add(val);  
          oscMsg.send(ESP_BT); // send the bytes
          oscMsg.empty(); // free space occupied by message
        }

        timeOfLastRecordedMovement = currentTimeSinceStart;
        previousStateA = stateA;
        wait = 0;
        prevval = val;
  } 
  else if (currentTimeSinceStart - lastRecordedTime >= 320) {
      if (wait == 0) {                //if wait time is greater than 320 msec.
        wait = 1;
        OSCMessage oscMsg("/D");       //sends out message "D" after 320 msec.
        oscMsg.send(ESP_BT);          // send the bytes
        oscMsg.empty(); 
      }
      else if (wait == 1) {
        wait = 2;
        OSCMessage oscMsg("/E");               //sends out message "E" after 640 msec.
        oscMsg.send(ESP_BT); 
        oscMsg.empty(); 
      }
      else if (wait ==2) {                  // if greater than 960 msec, reset and send out value 
        OSCMessage oscMsg("/F");           //if still connected but no motion, then sends out message "F" every 320 msec.
        oscMsg.send(ESP_BT);              // send the bytes
        oscMsg.empty();                  // Max seems to stop listening after a while so this won't work as a way to measure if bluetooth still connected ... 
      }

      lastRecordedTime = currentTimeSinceStart;
    }

 }
