int BasePin    = 13;          // output pin for NPN base
int MoistPin   = A0;
int MoistVal   = 0;
int FloatPin   = 2;
int FloatState = 0;
int FloatLedPin = 8;
//int Sleepms    = 3600000; // 1 hour //TODO: uncomment
//int Sleepms    = 600000; // 10 minutes for debug
int Sleepms    = 5000; // 1 sec for debug

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(BasePin, OUTPUT);
  pinMode(FloatLedPin, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(BasePin, LOW);
  digitalWrite(FloatLedPin, LOW);
  while(1) {
    // READ MOISTURE
    MoistVal = analogRead(MoistPin);
    Serial.println("Moist Sensor value:");
    Serial.println(MoistVal);

    // DRY SOIL
    if(MoistVal > 800) {
      Serial.println("Soil is DRY - operate pump!");
      
      // CHECK WATER LEVEL
      FloatState = digitalRead(FloatPin);
      Serial.println("Float State is:");
      Serial.println(FloatState);
      if(FloatState){
        Serial.println("Float Switch ON - NOT floating (NOT enough water)"); 
        digitalWrite(FloatLedPin, HIGH);
        digitalWrite(BasePin, LOW);
      }
      else {
        Serial.println("Float Switch OFF - floating (enough water)");
        digitalWrite(FloatLedPin, LOW);
        digitalWrite(BasePin, HIGH);
        // TODO: PUMP FOR HOW LONG? 
        delay(1000); //TODO: for longer time
        digitalWrite(BasePin, LOW);
      }
      Serial.println("\n");   
      }

    // MOIST SOIL
    else {
      Serial.println("Soil is MOIST");
    }
  delay(Sleepms);     
  }
}
