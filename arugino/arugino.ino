#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <EEPROM.h>


#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

#define MEM_TS_LEN  8   // Memory time stamp length in bytes
#define MEM_WATRD_LEN 1  // Memory irrigation flag length in bytes
#define MEM_SENS_LEN 1  // Memory sensor value length in bytes
#define MEM_WORD_LEN  MEM_TS_LEN+MEM_WATRD_LEN+MEM_SENS_LEN // Memory full word length in bytes
#define MEM_END_ADDR  EEPROM.length()-(3*MEM_WORD_LEN) // 2 last Memory addresses are reserved
#define MEM_CURR_ADDR EEPROM.length()-(2*MEM_WORD_LEN) // Last written memory address (with moisture value and tstamp)
#define MEM_WATRD_TS   EEPROM.length()-(1*MEM_WORD_LEN) // tstamp of last irrigation

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
int Watrd = 0;
const int PumpOnUs    = 5000;
const int MoistThresh = 200;  // MOISTURE SENSOR THRESHOLD
//const unsigned long interval=8000; // the time we need to wait - 8 sec for debug 
const unsigned long interval=86400000; // the time we need to wait
unsigned long previousMillis; // millis() returns unsigned long
unsigned long currentMillis;
int MemAddr = 0; 
int MemReadVal;
int ButtonFlag = 0;
int CheckMoist = 1;
DS3231  rtc(SDA, SCL);
LiquidCrystal_I2C  lcd(LCD_ADDR,LCD_En_pin,LCD_Rw_pin,LCD_Rs_pin,LCD_D4_pin,LCD_D5_pin,LCD_D6_pin,LCD_D7_pin);

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
  //EEPROM.write(MEM_CURR_ADDR, 0); // AFTER 1ST TIME SHOULD BE COMMENTED (so that between log checks will not erase log)
  MemAddr = EEPROM.read(MEM_CURR_ADDR); 

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
      // Write moisture, time-stamp and watered flag in memory. For log and LCD display
      MemAddr = MemWrite(MemAddr, MoistVal, Watrd); 
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
// MemWrite gets address, sensor value and watered bool. 
// It writes sensor value to Mem(address), current time-stamp and watered flag. 
// It then writes current address inspecial mem address and returns it for next round.
int MemWrite(int addr, int sens, int watrd)
{   
  // Sensor value
  if (addr >= (MEM_END_ADDR-MEM_WORD_LEN)) // Check that we have enough space to write sensor value & ts, otherwise go to beggining
    addr = 0;    
  EEPROM.write(addr, sens); // sensor value
  MemWriteTstamp(addr+1);   // time stamp
  EEPROM.write(addr+9, watrd); // watered flag
  addr = addr + MEM_WORD_LEN;
  EEPROM.write(MEM_CURR_ADDR, addr); // current address in special mem entry
  if (watrd == 1)
    MemWriteTstamp(MEM_WATRD_TS); // time stamp of watering
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
  int curr_addr = EEPROM.read(MEM_CURR_ADDR);
  Time curr_ts;
  uint8_t curr_ts_year_byte [2];
  char dt[16];
  char tm[16]; 
  uint8_t watrd;
  
  ButtonFlag = 0;
  // LCD print
  lcd.setCursor ( 0, 0 );
  Time watrd_ts = MemReadTstamp(MEM_WATRD_TS); 
  char watrd_dt[16];
  char watrd_tm[16]; 
  // LCD - last irrigation time
  lcd.setBacklight(HIGH); // Backlight on
  sprintf(watrd_dt, "%02d/%02d/%04d", watrd_ts.date,watrd_ts.mon,watrd_ts.year);
  sprintf(watrd_tm, "%02d:%02d:%02d", watrd_ts.hour,watrd_ts.min,watrd_ts.sec);
  lcd.print(watrd_dt);
  lcd.setCursor ( 0, 1 );
  lcd.print(watrd_tm);
  delay(5000);
  lcd.setBacklight(LOW);  // Backlight off


  // Memory log print to serial monitor
  Serial.println("\n\n");
  Serial.println("Saved Moisture Values:");
  Serial.println("index\tvalue\tDate\t   Time\t     Watered");
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
    watrd = EEPROM.read(i+9);
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
    (watrd==1) ? Serial.print("True") : Serial.print("False");
    Serial.println();
  }
  Serial.println("\n\n");
}
