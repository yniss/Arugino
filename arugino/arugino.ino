#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <EEPROM.h>


#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

#define MEM_TS_LEN  8   // Memory time stamp length in bytes
#define MEM_IRIG_LEN 1  // Memory irrigation flag length in bytes
#define MEM_SENS_LEN 1  // Memory sensor value length in bytes
#define MEM_WORD_LEN  MEM_TS_LEN+MEM_IRIG_LEN+MEM_SENS_LEN // Memory full word length in bytes
#define MEM_END_ADDR  EEPROM.length()-3*MEM_WORD_LEN // 2 last Memory addresses are reserved
#define MEM_CURR_ADDR EEPROM.length()-2*MEM_WORD_LEN // Last written memory address (with moisture value and tstamp)
#define MEM_IRIG_TS   EEPROM.length()-1*MEM_WORD_LEN // tstamp of last irrigation

#define LCD_ADDR 0x3F
#define LCD_BACKLIGHT_PIN     3
#define LCD_En_pin  2
#define LCD_Rw_pin  1
#define LCD_Rs_pin  0
#define LCD_D4_pin  4
#define LCD_D5_pin  5
#define LCD_D6_pin  6
#define LCD_D7_pin  7

int BasePin    = 13; // output pin for NPN base
int MoistPin   = A0;
int MoistVal   = 0;
int FloatPin   = 4;
int FloatState = 0;
int ButtonPin  = 2;
const int PumpOnUs    = 5000;
const int MoistThresh = 225;
const unsigned long interval=8000; // the time we need to wait - 10 sec for debug 
//const unsigned long interval=86400000; // the time we need to wait
unsigned long previousMillis; // millis() returns unsigned long
unsigned long currentMillis;
int MemAddr = 0; 
int MemReadVal;
int button_isr_flag = 0;
DS3231  rtc(SDA, SCL);
LiquidCrystal_I2C  lcd(LCD_ADDR,LCD_En_pin,LCD_Rw_pin,LCD_Rs_pin,LCD_D4_pin,LCD_D5_pin,LCD_D6_pin,LCD_D7_pin);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(BasePin, OUTPUT);
  digitalWrite(BasePin, LOW); // disable pump
  attachInterrupt(0, Button, FALLING); // set button interrupt - external int0
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
  EEPROM.write(MEM_CURR_ADDR, 0); // AFTER 1ST TIME SHOULD BE COMMENTED
  MemAddr = EEPROM.read(MEM_CURR_ADDR); 

  // LCD init
  lcd.begin (16,2);
  // Switch on the backlight
  lcd.setBacklightPin(LCD_BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(LOW);
  lcd.home (); // go home
}

void loop() {
    // check if interval time between sensor checks has passed
    currentMillis = millis();    
    if (currentMillis - previousMillis < interval) { // didn't pass yet
      if (button_isr_flag == 1) // if button was pressed - print last irrigation
        log_print();
    }
    else {
      //previousMillis = currentMillis;

      // READ MOISTURE 
      MoistVal = analogRead(MoistPin) / 4; // divide by 4 to write only 1 EEPRM byte
      Serial.print("Moist Sensor value: ");
      Serial.println(MoistVal);
      // WRITE MOISTURE IN MEM
      MemAddr = MemWriteSens(MemAddr, MoistVal); 
      EEPROM.write(MEM_CURR_ADDR, MemAddr); // write last saved address (in a reserved address)
      
      // DRY SOIL
      if(MoistVal > MoistThresh) {
        Serial.println("Soil is DRY - operate pump!");
        
        // CHECK WATER LEVEL
        FloatState = digitalRead(FloatPin);
        Serial.print("Float State is: ");
        Serial.println(FloatState);
        if(FloatState){
          Serial.println("Float Switch ON - NOT floating (NOT enough water)"); 
          digitalWrite(BasePin, LOW);
          MemWriteIrig(MemAddr-1, 0); // Write irig - false //TODO: MemAddr-1 is ugly...
        }
        else {
          Serial.println("Float Switch OFF - floating (enough water)");
          // Pump 
          digitalWrite(BasePin, HIGH);
          delay(PumpOnUs); 
          digitalWrite(BasePin, LOW);
          MemWriteIrig(MemAddr-1, 1); // Write irig - true //TODO: MemAddr-1 is ugly...
          // Write irrigation time in Mem
          MemWriteTstamp(MEM_IRIG_TS);
        }
        Serial.println();   
        }
        
      // MOIST SOIL
      else
      {
        MemWriteIrig(MemAddr-1, 0); // Write irig - false //TODO: MemAddr-1 is ugly...
        Serial.println("Soil is MOIST");
      }
      previousMillis = millis(); // capture time of last irrigation, to be used in next cycle
    }
}


//------------//
//   EEPROM   //
//------------//
//TODO: replace by MemWriteMoist which writes sensor, tstamp and irig (true/false)
// MemWriteSens gets address and sensor value, 
// writes value to Mem(address) and then increments the address by 1 word. 
// Returns incremented address.
int MemWriteSens(int addr, int val)
{   
  if (addr >= (MEM_END_ADDR-MEM_WORD_LEN)) // Check that we have enough space to write sensor value & ts, otherwise go to beggining
    addr = 0;
  EEPROM.write(addr, val);
  MemWriteTstamp(addr+1); 
  addr = addr + MEM_WORD_LEN; 
  return addr;
}

void MemWriteTstamp(int addr)
{
  Time t = rtc.getTime();
  uint8_t t_year_byte [2];
  t_year_byte[0] = (t.year & 0xFF);
  t_year_byte[1] = (t.year >> 8);
  EEPROM.write(addr, t.dow);    // DOW
  EEPROM.write(addr+1, t.date); // DATE
  EEPROM.write(addr+2, t.mon);  // MON
  EEPROM.write(addr+3, t_year_byte[0]); // YEAR (2B)
  EEPROM.write(addr+4, t_year_byte[1]); // YEAR (2B)
  EEPROM.write(addr+5, t.hour); // HOUR
  EEPROM.write(addr+6, t.min);  // MIN
  EEPROM.write(addr+7, t.sec);  // SEC
}

Time MemReadTstamp(int addr)
{
  Time t;
  uint8_t t_year_byte [2];
  t_year_byte[0] = (t.year & 0xFF);
  t_year_byte[1] = (t.year >> 8);
  t.dow  = EEPROM.read(addr);    // DOW
  t.date = EEPROM.read(addr+1); // DATE
  t.mon  = EEPROM.read(addr+2);  // MON
  t_year_byte[0] = EEPROM.read(addr+3); // YEAR (2B)
  t_year_byte[1] = EEPROM.read(addr+4); // YEAR (2B)
  t.hour = EEPROM.read(addr+5); // HOUR
  t.min  = EEPROM.read(addr+6);  // MIN
  t.sec  = EEPROM.read(addr+7);  // SEC
  t.year = ((uint16_t)t_year_byte[1] << 8) | t_year_byte[0];
  return t;
}

void MemWriteIrig(int addr, int irig)
{
    EEPROM.write(addr, irig);
}


//------------------//
// Button Interrupt //
//------------------//
// TODO: will this work when arduino in sleep? should modify that interrupt wakes it first?
// Button reads and prints all moisture sensor values and tstamp written in memory.
// It also reads tstamp of last irrigation and prints it to LCD
void Button()
{
  button_isr_flag = 1; // flag button was pressed, for irrigation print & LCD display
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
  int curr_addr = EEPROM.read(MEM_CURR_ADDR);
  Time curr_ts;
  uint8_t curr_ts_year_byte [2];
  char dt[16];
  char tm[16]; 
  uint8_t irig;
  
  button_isr_flag = 0;
  //TODO: 1. there's a bug in the time displayed in lcd
  // LCD print
  Time irig_ts = MemReadTstamp(MEM_IRIG_TS); 
  char irig_dt[16];
  char irig_tm[16]; 
  // LCD - last irrigation time
  lcd.setBacklight(HIGH); // Backlight on
  sprintf(irig_dt, "%02d/%02d/%04d", irig_ts.date,irig_ts.mon,irig_ts.year);
  sprintf(irig_tm, "%02d:%02d:%02d", irig_ts.hour,irig_ts.min,irig_ts.sec);
  lcd.print(irig_dt);
  lcd.setCursor ( 0, 1 );
  lcd.print(irig_tm);
  delay(3000);
  lcd.setBacklight(LOW);  // Backlight off

  //TODO: first index + value are wrong
  // Memory log print to serial monitor
  Serial.println("\n\n");
  Serial.println("Saved Moisture Values:");
  Serial.println("index\tvalue\tDate\t   Time\t     Irrigated");
  Serial.println("-----\t-----\t----\t   ----\t     ---------");
  for (int i=0; i<curr_addr; i=i+MEM_WORD_LEN)
  {
    MemReadVal = EEPROM.read(i);
    curr_ts.dow  = EEPROM.read(i+1);
    curr_ts.date = EEPROM.read(i+2);
    curr_ts.mon  = EEPROM.read(i+3);
    curr_ts_year_byte[0] = EEPROM.read(i+4);
    curr_ts_year_byte[1] = EEPROM.read(i+5);
    curr_ts.year = ((uint16_t)curr_ts_year_byte[1] << 8) | curr_ts_year_byte[0];
    curr_ts.hour = EEPROM.read(i+6);
    curr_ts.min  = EEPROM.read(i+7);
    curr_ts.sec  = EEPROM.read(i+8);
    irig = EEPROM.read(i+9);
    sprintf(dt, "%02d/%02d/%04d", curr_ts.date,curr_ts.mon,curr_ts.year);
    sprintf(tm, "%02d:%02d:%02d", curr_ts.hour,curr_ts.min,curr_ts.sec);
    Serial.print(i/unsigned(MEM_WORD_LEN));   
    Serial.print("\t"); 
    Serial.print(MemReadVal);
    Serial.print("\t"); 
    Serial.print(dt);
    Serial.print(" ");
    Serial.print(tm);
    Serial.print("  ");
    irig ? Serial.print("True") : Serial.print("False");
    Serial.println();
  }
  Serial.println("\n\n");
}
