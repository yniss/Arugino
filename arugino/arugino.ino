int BasePin    = 13;          // output pin for NPN base
int MoistPin   = A0;
int MoistVal   = 0;
int FloatPin   = 2;
int FloatState = 0;
//unsigned long interval=5000; // the time we need to wait - 10 sec for debug 
unsigned long interval=86400000; // the time we need to wait
unsigned long previousMillis=0; // millis() returns unsigned long

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(BasePin, OUTPUT);
  digitalWrite(BasePin, LOW); // disable pump
}

void loop() {
    // READ MOISTURE
    MoistVal = analogRead(MoistPin);
    Serial.println("Moist Sensor value:");
    Serial.println(MoistVal);

    // DRY SOIL
    if(MoistVal > 900) {
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
        delay(5000); 
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




