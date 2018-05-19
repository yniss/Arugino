#include <EEPROM.h>

int BasePin    = 13;          // output pin for NPN base
int MoistPin   = A0;
int MoistVal   = 0;
int FloatPin   = 4; //TODO: changed from 2 to 4 -> should re-attach on board!!!
int FloatState = 0;
int ButtonPin  = 2;
const int PumpOnUs    = 5000;
const int MoistThresh = 225;
//const unsigned long interval=5000; // the time we need to wait - 10 sec for debug 
const unsigned long interval=86400000; // the time we need to wait
unsigned long previousMillis=0; // millis() returns unsigned long
int MemAddr = 0; 
int MemReadVal;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(BasePin, OUTPUT);
  digitalWrite(BasePin, LOW); // disable pump
  attachInterrupt(0, Button, FALLING); // set interrupt - external int0
}

void loop() {
    // READ MOISTURE 
    MoistVal = analogRead(MoistPin) / 4; // divide by 4 to write only 1 EEPRM byte
    Serial.println("Moist Sensor value:");
    Serial.println(MoistVal);
    // WRITE MOISTURE IN MEM
    MemAddr = MemWriteByte(MemAddr, MoistVal); //TODO: write tstamp
    //TODO: add read memory values (0 to addr) and print, done on interrupt from button
    
    // DRY SOIL
    if(MoistVal > MoistThresh) {
      Serial.println("Soil is DRY - operate pump!");
      
      // CHECK WATER LEVEL
      FloatState = digitalRead(FloatPin);
      Serial.println("Float State is:");
      Serial.println(FloatState);
      if(FloatState){
        Serial.println("Float Switch ON - NOT floating (NOT enough water)"); 
        digitalWrite(BasePin, LOW);
      }
      else {
        Serial.println("Float Switch OFF - floating (enough water)");
        // Pump 
        digitalWrite(BasePin, HIGH);
        delay(PumpOnUs); 
        digitalWrite(BasePin, LOW);
      }
      Serial.println("\n");   
      }
      
    // MOIST SOIL
    else
      Serial.println("Soil is MOIST");
    
    // check if interval time has passed
    while (millis() - previousMillis < interval);
    previousMillis = millis();
}


//TODO: replace by MemWriteMoist which writes both sensor and tstamp
// MemWriteByte gets address and value, 
// writes value to MEem(address) and then increments the (byte) address by 1. 
// Returns incremented address.
int MemWriteByte(int addr, int val)
{
  EEPROM.write(addr, val);    
  if (addr < EEPROM.length()) //TODO: leave some space for last irrigation
      addr = addr + 1;
    else  
      addr = 0;
  return addr;
}


// TODO: check how should change to consider time stamp
// TODO: will this work when arduino in sleep? should modify that interrupt wakes it first?
// Button reads and prints all moisture sensor values and tstamp written in memory.
// It also reads tstamp of last irrigation and prints it to LCD
void Button()
{
  Serial.println("\n\n");
  for (int i=0; i<MemAddr; i++)
  {
    MemReadVal = EEPROM.read(i);
    Serial.println("Mem Moist index:");
    Serial.println(i);    
    Serial.println("Mem Moist value:");
    Serial.println(MemReadVal );    
  }
  Serial.println("\n\n");
}


