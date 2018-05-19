#include <DS3231.h>
#include <EEPROM.h>

//#include 
// Init the DS3231 using the hardware interface
//DS3231  rtc(SDA, SCL);

#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

#define MEM_TS_LEN  8   // Memory time stamp length in bytes
#define MEM_IRIG_LEN 1  // Memory irrigation flag length in bytes
#define MEM_SENS_LEN 1  // Memory sensor value length in bytes
#define MEM_WORD_LEN  MEM_TS_LEN+MEM_IRIG_LEN+MEM_SENS_LEN // Memory full word length in bytes
#define MEM_END_ADDR  EEPROM.length()-3*MEM_WORD_LEN-1 // 2 last Memory addresses are reserved
#define MEM_CURR_ADDR EEPROM.length()-2*MEM_WORD_LEN-1 // Last written memory address (with moisture value and tstamp)
#define MEM_IRIG_TS   EEPROM.length()-1*MEM_WORD_LEN-1 // tstamp of last irrigation

int BasePin    = 13; // output pin for NPN base
int MoistPin   = A0;
int MoistVal   = 0;
int FloatPin   = 4;
int FloatState = 0;
int ButtonPin  = 2;
const int PumpOnUs    = 5000;
const int MoistThresh = 225;
const unsigned long interval=5000; // the time we need to wait - 10 sec for debug 
//const unsigned long interval=86400000; // the time we need to wait
unsigned long previousMillis=0; // millis() returns unsigned long
int MemAddr = 0; 
int MemReadVal;
DS3231  rtc(SDA, SCL);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(BasePin, OUTPUT);
  digitalWrite(BasePin, LOW); // disable pump
  attachInterrupt(0, Button, FALLING); // set button interrupt - external int0

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
}

void loop() {
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
      

    // check if interval time has passed
    while (millis() - previousMillis < interval);
    previousMillis = millis();
}


//------------//
//   EEPROM   //
//------------//
//TODO: replace by MemWriteMoist which writes both sensor and tstamp
// MemWriteSens gets address and sensor value, 
// writes value to Mem(address) and then increments the address by 1 word. 
// Returns incremented address.
int MemWriteSens(int addr, int val)
{   
  if (addr < (MEM_END_ADDR-MEM_WORD_LEN)) // Check that we have enough space to write sensor value & ts, otherwise go to beggining
  {
    EEPROM.write(addr, val);
    MemWriteTstamp(addr+1); 
    addr = addr + MEM_WORD_LEN;
  }
  else
  {  
    addr = 0;
    EEPROM.write(addr, val);
    MemWriteTstamp(addr+1); 
    addr = addr + MEM_WORD_LEN; 
  }
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
  int curr_addr = EEPROM.read(MEM_CURR_ADDR);
  Time curr_ts;
  uint8_t curr_ts_year_byte [2];
  char dt[16];
  char tm[16]; 
  uint8_t irig;
  
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
    Serial.print(i/MEM_WORD_LEN);   
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

