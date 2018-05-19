int BasePin = 13;          // output pin for NPN base
int MoistPin = A0;
int MoistVal = 0;
int FloatPin = 2;
int FloatState = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(BasePin, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(BasePin, HIGH);
  while(1) {
    MoistVal = analogRead(MoistPin);
    Serial.println("Moist Sensor value:");
    Serial.println(MoistVal);
    if(MoistVal > 800) {
      Serial.println("Soil is DRY - operate pump!");
    }
    else {
      Serial.println("Soil is MOIST");
    }
    // TODO: should move inside moisture check and re-write
    FloatState = digitalRead(FloatPin);
    Serial.println("Float State is:");
    Serial.println(FloatState);
    if(FloatState){
      Serial.println("Float Switch ON - NOT floating (NOT enough water)"); 
    }
    else {
      Serial.println("Float Switch OFF - floating (enough water)");
    }
    Serial.println("\n");
    delay(1000);    
    }
}
