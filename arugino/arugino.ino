#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <EEPROM.h>
#include "arugino.h"

int BasePin    = 13; // output pin for NPN base
int MoistPin   = A0;
int MoistVal   = 0;
int FloatPin   = 4;
int FloatState = 0;
int ButtonPin  = 2;
int Watrd = 0;
const int PumpOnUs    = 5000;
const int MoistThresh = 190;  // MOISTURE SENSOR THRESHOLD
//const unsigned long interval=60000; // the time we need to wait - 8 sec for debug 
//const unsigned long interval=86400000;   // the time we need to wait - 24 hours
const unsigned long interval=86400000/2; // the time we need to wait - 12 hours
unsigned long previousMillis; // millis() returns unsigned long
unsigned long currentMillis;
int ButtonFlag = 0;
int CheckMoist = 1;
DS3231 rtc(SDA, SCL);
LiquidCrystal_I2C lcd(LCD_ADDR,LCD_En_pin,LCD_Rw_pin,LCD_Rs_pin,LCD_D4_pin,LCD_D5_pin,LCD_D6_pin,LCD_D7_pin);

// Declare Mem Units stuff:
// Memory is split into units - a unit per moist sensor reading.
// It holds the following information:
// - Time stamp                       
// - Sensor read value                
// - Boolean flag - was irrigated or not
// In order to access the units information easily - we'll hold an array of unit base addresses.
uint8_t CurrMemUnit = 0;                                        // There are ~93 units (of 11 bytes) in EEPROM, so uint8_t is enough 
const int MemUnitLen         = 11;                              // Sens - 1B, Tstamp-8B, Watrd flag - 1B
const int MemUnitBarrier     = EEPROM.length()/MemUnitLen-2;    // Last Memory unit is reserved for Curr unit address & Last irrigation Tstamp
const int NumMemUnits        = 90;                              // NumMemUnits < MemUnitBarrier/MemUnitLen;         
// 2 Special addresses:
const int MemAddr_WatrdTs    = EEPROM.length()-(1*MemUnitLen);  // Address of Last Irrigation's Tstamp
const int MemAddr_CurrUnit   = EEPROM.length()-1;               // Address of Last written memory unit  
int MemUnitArr[NumMemUnits];
int MemReadVal;


void setup() {
  Serial.begin(9600);

  pinMode(BasePin, OUTPUT);
  digitalWrite(BasePin, LOW); // disable pump
  attachInterrupt(0, ButtonISR, FALLING); // set button interrupt - external int0
  previousMillis = millis();

  // Set RTC
  // Initialize the rtc object
  rtc.begin();
  // Uncomment for setting RTC time and date
//  rtc.setDOW(THURSDAY);     // Set Day-of-Week to SUNDAY
//  rtc.setTime(22, 48, 0);     // Set the time to 12:00:00 (24hr format)
//  rtc.setDate(5, 4, 2018);   // Set the date to January 1st, 2014
  // Print current RTC time  
  PrintTimeRTC();
  Serial.println();   

  // initialize current mem address
  //EEPROM.write(MemAddr_CurrUnit, 0); // AFTER 1ST TIME SHOULD BE COMMENTED (so that between log checks will not erase log)
  CurrMemUnit = (uint8_t)EEPROM.read(MemAddr_CurrUnit);
  MemUnitInit(MemUnitArr,0,NumMemUnits,MemUnitLen); // initialize MemUnitArr with unit addresses 
  
  // LCD init
  lcd.begin (16,2);
  // Switch on the backlight
  lcd.setBacklightPin(LCD_BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(LOW);
  lcd.home (); // go home
}

void loop() {
  if (CheckMoist == 1)
    {
      // READ MOISTURE 
      MoistVal = analogRead(MoistPin) / 4; // divide by 4 to write only 1 EEPRM byte
      Serial.print("Moist Sensor value: ");
      Serial.println(MoistVal);
      
      // DRY SOIL
      if(MoistVal < MoistThresh) {
        Serial.println("Soil is DRY - operate pump!");
        
        // CHECK WATER LEVEL
        FloatState = digitalRead(FloatPin);
        Serial.print("Float State is: ");
        Serial.println(FloatState);
        if(FloatState){
          Serial.println("Float Switch ON - NOT floating (NOT enough water)"); 
          digitalWrite(BasePin, LOW);
          Watrd = 0;
        }
        else {
          Serial.println("Float Switch OFF - floating (enough water)");
          // Pump 
          digitalWrite(BasePin, HIGH);
          delay(PumpOnUs); 
          digitalWrite(BasePin, LOW);
          Watrd = 1;
        }
        Serial.println();   
        }
        
      // MOIST SOIL
      else
      {
        Watrd = 0;
        Serial.println("Soil is MOIST");
      }
      // Write moisture value, time-stamp and watered flag in memory (for log and LCD display)
      // In addition - update CurrMemUnit for next round
      CurrMemUnit = MemWrite(CurrMemUnit, MoistVal, Watrd, FloatState);
      previousMillis = millis(); // capture time of last irrigation, to be used in next cycle
    }
    currentMillis = millis();    
    if (currentMillis - previousMillis < interval) { // didn't pass yet
      CheckMoist = 0;
      if (ButtonFlag == 1) // if button was pressed - print last irrigation
         log_print();
    }
    else
      CheckMoist = 1;
}



//------------//
//   EEPROM   //
//------------//
// After declaring an array of Mem Units,
// we'll initialize the array with addresses.
void MemUnitInit(int MemUnitArr[], int BaseAddr, int ArrLen, int UnitLen) {
  for (int i=0; i<ArrLen; i++) {
    MemUnitArr[i] = BaseAddr + i*UnitLen;
  }
} 

// MemWrite gets current mem unit, sensor value and watered bool. 
// It writes the data to the memory unit.
// It then writes current address in special mem address and returns it for next round.
uint8_t MemWrite(uint8_t CurrUnit, int Sens, int Watrd, int TankEmpty)
{   
  // Sensor value
  if (CurrUnit > MemUnitBarrier) // Check that we have enough space to write sensor value & ts, otherwise go to beggining
    CurrUnit = 0;    
  
  EEPROM.write(MemUnitArr[CurrUnit], Sens);                     // sensor value
  MemWriteTstamp((MemUnitArr[CurrUnit]));                       // time stamp
  EEPROM.write((MemUnitArr[CurrUnit]+OFF_TANK), TankEmpty);     // water tank empty flag
  EEPROM.write((MemUnitArr[CurrUnit]+OFF_WATRD), Watrd);        // watered flag
  CurrUnit++;
  EEPROM.write(MemAddr_CurrUnit, CurrUnit);                     // write curr unit in special mem address
  if (Watrd == 1)
    MemWriteTstamp(MemAddr_WatrdTs);                            // time stamp of watering
  return CurrUnit;
}

void MemWriteTstamp(int addr)
{
  Time t = rtc.getTime();
  uint8_t t_year_byte [2];
  t_year_byte[0] = (t.year & 0xFF);
  t_year_byte[1] = (t.year >> 8);
  EEPROM.write(addr+OFF_DOW,    t.dow);             // DOW
  EEPROM.write(addr+OFF_DATE,   t.date);            // DATE
  EEPROM.write(addr+OFF_MON,    t.mon);             // MON
  EEPROM.write(addr+OFF_YEAR0,  t_year_byte[0]);    // YEAR (B0)
  EEPROM.write(addr+OFF_YEAR1,  t_year_byte[1]);    // YEAR (B1)
  EEPROM.write(addr+OFF_HOUR,   t.hour);            // HOUR
  EEPROM.write(addr+OFF_MIN,    t.min);             // MIN
  EEPROM.write(addr+OFF_SEC,    t.sec);             // SEC
}

Time MemReadTstamp(int addr)
{
  Time t;
  uint8_t t_year_byte [2];
  t_year_byte[0] = (t.year & 0xFF);
  t_year_byte[1] = (t.year >> 8);
  t.dow             = EEPROM.read(addr+OFF_DOW);    // DOW
  t.date            = EEPROM.read(addr+OFF_DATE);   // DATE
  t.mon             = EEPROM.read(addr+OFF_MON);    // MON
  t_year_byte[0]    = EEPROM.read(addr+OFF_YEAR0);  // YEAR (B0)
  t_year_byte[1]    = EEPROM.read(addr+OFF_YEAR1);  // YEAR (B1)
  t.hour            = EEPROM.read(addr+OFF_HOUR);   // HOUR
  t.min             = EEPROM.read(addr+OFF_MIN);    // MIN
  t.sec             = EEPROM.read(addr+OFF_SEC);    // SEC
  t.year            = ((uint16_t)t_year_byte[1] << 8) | t_year_byte[0];
  return t;
}


//------------------//
// Button Interrupt //
//------------------//
// TODO: will this work when arduino in sleep? should modify that interrupt wakes it first?
// Button reads and prints all moisture sensor values and tstamp written in memory.
// It also reads tstamp of last irrigation and prints it to LCD
void ButtonISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) 
  {
    ButtonFlag = 1; // flag button was pressed, for irrigation print & LCD display
  }
  last_interrupt_time = interrupt_time;
}


//------------//
//     RTC    //
//------------//
void PrintTimeRTC()
  {
    // Send Day-of-Week
    Serial.print(rtc.getDOWStr());
    Serial.print(" ");
  
    // Send date
    Serial.print(rtc.getDateStr());
    Serial.print(" -- ");

    // Send time
    Serial.println(rtc.getTimeStr());
  }

//------------//
//     LOG    //
//------------//
void log_print()
{
  int CurrUnit = EEPROM.read(MemAddr_CurrUnit);
  Time CurrTs;
  uint8_t CurrTsYearByte [2];
  char dt[16];
  char tm[16]; 
  uint8_t Watrd;
  uint8_t TankEmpty;
  
  ButtonFlag = 0;
  // LCD print
  lcd.setCursor ( 0, 0 );
  Time Watrd_ts = MemReadTstamp(MemAddr_WatrdTs); 
  char Watrd_dt[16];
  char Watrd_tm[16]; 
  // LCD - last irrigation time
  lcd.setBacklight(HIGH); // Backlight on
  sprintf(Watrd_dt, "%02d/%02d/%04d", Watrd_ts.date,Watrd_ts.mon,Watrd_ts.year);
  sprintf(Watrd_tm, "%02d:%02d:%02d", Watrd_ts.hour,Watrd_ts.min,Watrd_ts.sec);
  lcd.print(Watrd_dt);
  lcd.setCursor ( 0, 1 );
  lcd.print(Watrd_tm);
  delay(5000);
  lcd.setBacklight(LOW);  // Backlight off


  // Memory log print to serial monitor
  Serial.println("\n\n");
  Serial.println("Saved Moisture Values:");
  Serial.println("index\tvalue\tDate\t   Time\t     Tank Empty\t    Watered");
  Serial.println("-----\t-----\t----\t   ----\t     ----------\t    -------");
  for (int i=0; i<CurrUnit; i=i+1)
  {
    MemReadVal          = EEPROM.read(MemUnitArr[i]+OFF_SENS);
    CurrTs.dow          = EEPROM.read(MemUnitArr[i]+OFF_DOW);
    CurrTs.date         = EEPROM.read(MemUnitArr[i]+OFF_DATE);
    CurrTs.mon          = EEPROM.read(MemUnitArr[i]+OFF_MON);
    CurrTsYearByte[0]   = EEPROM.read(MemUnitArr[i]+OFF_YEAR0);
    CurrTsYearByte[1]   = EEPROM.read(MemUnitArr[i]+OFF_YEAR1);
    CurrTs.year         = ((uint16_t)CurrTsYearByte[1] << 8) | CurrTsYearByte[0];
    CurrTs.hour         = EEPROM.read(MemUnitArr[i]+OFF_HOUR);
    CurrTs.min          = EEPROM.read(MemUnitArr[i]+OFF_MIN);
    CurrTs.sec          = EEPROM.read(MemUnitArr[i]+OFF_SEC);
    TankEmpty           = EEPROM.read(MemUnitArr[i]+OFF_TANK);
    Watrd               = EEPROM.read(MemUnitArr[i]+OFF_WATRD);
    sprintf(dt, "%02d/%02d/%04d", CurrTs.date,CurrTs.mon,CurrTs.year);
    sprintf(tm, "%02d:%02d:%02d", CurrTs.hour,CurrTs.min,CurrTs.sec);
    Serial.print(i);   
    Serial.print("\t"); 
    Serial.print(MemReadVal);
    Serial.print("\t"); 
    Serial.print(dt);
    Serial.print(" ");
    Serial.print(tm);
    Serial.print("  ");
    (TankEmpty==1) ? Serial.print("True") : Serial.print("False");
    Serial.print("\t    ");
    (Watrd==1) ? Serial.print("True") : Serial.print("False");
    Serial.println();
  }
  Serial.println("\n\n");
}
