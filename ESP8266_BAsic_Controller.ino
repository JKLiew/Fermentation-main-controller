//ESP8266 library
#include <ESP8266WiFi.h>

//Firebase library
#include <FirebaseArduino.h>
#include <ArduinoJson.h>

//Servo motor library
#include <Servo.h>

//Firebase databse secret
#define FIREBASE_HOST "jk-fyp-1234.firebaseio.com"
#define FIREBASE_AUTH "brhDDULhPFbIpgEd9McHx5kfLVf7oYjCA9AhVoS2"

//SSID and password
#define WIFI_SSID "JK Honor"
#define WIFI_PASSWORD "technical"

//Interrupt flag and variable
int calAvgPeriod = 5;
bool calAvgFlag = false;
int counter = 0;

//Pin setup
const int statusTempPin = D0;
const int statusHumidPin = D1;
const int statusWaterLvlPin = D2;
const int servoValvePin = D3;
const int buzzerPin = D4;
const int waterLvlTopPin = D5;
const int waterLvlBottomPin = A0;
const int fanPin = D6;
const int humidifierPin = D7;
const int heaterPin = D8;

//Variable and output setup
float valueTemp[5] = {0, 0, 0, 0, 0};
float valueHumid[5] = {0, 0, 0, 0, 0};
bool outputHeater = true;
int outputValve = 0;

//Servo motor setup
Servo servoValve;

void setup() {
//  Setuo for all pin
  pinMode(buzzerPin, OUTPUT);
  pinMode(statusTempPin, OUTPUT);
  pinMode(statusHumidPin, OUTPUT);
  pinMode(statusWaterLvlPin, OUTPUT);
  pinMode(waterLvlTopPin, INPUT);
  pinMode(waterLvlBottomPin, INPUT);
  servoValve.attach(servoValvePin);
  pinMode(heaterPin, OUTPUT);
  pinMode(humidifierPin, OUTPUT);
  pinMode(fanPin, OUTPUT);

//  Setup for timer 1
  timer1_disable();
  timer1_attachInterrupt(timer1_callISR);
  timer1_isr_init();
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); // 5MHz
  timer1_write(5000000);  // Tick every 1 seconds
//  ESP.wdtDisable();
//  ESP.wdtEnable(WDTO_8S);

//  Write the valve initial position.
  servoValve.write(40);
  
//  Prepare serial monitor
  Serial.begin(74880);

//  Connect to Wi-fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

//  Delete previos record on Firebase Realtime Database
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.remove("Sensors/Sensor_1/Humidity");
  Firebase.remove("Sensors/Sensor_2/Humidity");
  Firebase.remove("Sensors/Sensor_3/Humidity");
  Firebase.remove("Sensors/Sensor_4/Humidity");
  Firebase.remove("Sensors/Sensor_1/Temperature");
  Firebase.remove("Sensors/Sensor_2/Temperature");
  Firebase.remove("Sensors/Sensor_3/Temperature");
  Firebase.remove("Sensors/Sensor_4/Temperature");

//  Stand by the Emergency Stop button
  Firebase.setString("Emergency Stop","Stand By");
}

void loop(){
//  Write the water level to the database. 
  waterLvl();       // Check the water level.
  
//  ALL Parameter is based on the value in the database. 
  setValve();       // Set the position of the valve.  
  heater();         // Turn on/off the heater.  
  humidifier();     // Turn on/off the humidfier and the fan. 
  monitorParam();   // Monitor the fault. 
  EStop();          // Check for Emergency Stop Button. 
}

void EStop(){
  String ESTOP = (Firebase.getString("Emergency Stop"));
  if (ESTOP.equals("Triggered")){
    digitalWrite(humidifierPin, LOW);
    digitalWrite(heaterPin, LOW);
    Firebase.setString("Actuators/Humidifier Relay","OFF");
    Firebase.setString("Actuators/Heater Relay","OFF");
  }
}

void monitorParam(){
  FirebaseObject jsonParam = (Firebase.get("Summary"));
  String rangeHumid = jsonParam.getString("Humidity Range");
  String rangeTemp = jsonParam.getString("Temperature Range");
  String rangeWaterLvl = jsonParam.getString("Water Level");
  int buzzerHumidFlag = 0, buzzerTempFlag = 0, buzzerWaterLvlFlag = 0;
  if (rangeHumid != "In Range"){
     Serial.println(rangeHumid);
     digitalWrite(statusHumidPin, HIGH);
     buzzerHumidFlag = 1;
  }
  else {
    Serial.println("Humidity is in range. ");
    digitalWrite(statusHumidPin, LOW);
    buzzerHumidFlag = 0;
  }

  if (rangeTemp != "In Range"){
     Serial.println(rangeTemp);
     digitalWrite(statusTempPin, HIGH);
     buzzerTempFlag = 1;
  }
  else {
    Serial.println("Temperature is in range. ");
    digitalWrite(statusTempPin, LOW);
    buzzerTempFlag = 0;
  }

  if(rangeWaterLvl == "Low"){
    Serial.println("Water Level is LOW!");
    digitalWrite(statusWaterLvlPin, HIGH);
    buzzerWaterLvlFlag = 1;
  }
  else{
    Serial.println("Water Level is in range. \n");
    digitalWrite(statusWaterLvlPin, LOW);
    buzzerWaterLvlFlag = 0;
  }
  if (buzzerHumidFlag || buzzerTempFlag || buzzerWaterLvlFlag) digitalWrite(buzzerPin, HIGH);
  else digitalWrite(buzzerPin, LOW);

}

void humidifier(void){
  String humidifierInput = (Firebase.getString("Actuators/Humidifier Relay"));
  if (humidifierInput.equals("ON")){
    Serial.print("Humidifier Relay : ");
    Serial.println(humidifierInput);
    digitalWrite(humidifierPin, HIGH);
    Serial.print("Fan Relay : ");
    Serial.println(humidifierInput);
    digitalWrite(fanPin, HIGH);
  }
  else if (humidifierInput.equals("OFF")){
    Serial.print("Humidifier Relay : ");
    Serial.println(humidifierInput);
    digitalWrite(humidifierPin, LOW);
    Serial.print("Fan Relay : ");
    Serial.println(humidifierInput);
    digitalWrite(fanPin, LOW);
  }
}

void heater(void){
  String heaterInput = (Firebase.getString("Actuators/Heater Relay"));
  if (heaterInput.equals("ON")){
    Serial.print("Heater Relay : ");
    Serial.println(heaterInput);
    digitalWrite(heaterPin, HIGH);
  }
  else if (heaterInput.equals("OFF")){
    Serial.print("Heater Relay : ");
    Serial.println(heaterInput);
    digitalWrite(heaterPin, LOW);
  }
}

void waterLvl(void){
  bool top = digitalRead(waterLvlTopPin);
  int val = analogRead(waterLvlBottomPin);
  bool bottom;
  if (val > 500) bottom = true;
  else bottom = false;
  
  if(top == true && bottom == true)         {
//    Serial.println("1");  
    Firebase.setString("Sensors/Water Level","Normal");
    if (Firebase.success()) Serial.println("Water level : Normal");
    else if (Firebase.failed())  Serial.println("Water level : FAILED");
  }
  else if(top == true && bottom == false)   {
//    Serial.println("2");  
    Firebase.setString("Sensors/Water Level","Low");
    if (Firebase.success()) Serial.println("Water level : Low");
    else if (Firebase.failed())  Serial.println("Water level : FAILED");
  }
  else if(top == false && bottom == true)   {
//    Serial.println("3");  
    Firebase.setString("Sensors/Water Level","Full");
    if (Firebase.success()) Serial.println("Water level : Full");
    else if (Firebase.failed())  Serial.println("Water level : FAILED");
  }
//  else if(top == false && bottom == false)  Serial.println("4");

  Firebase.setString("Refresh Control/Water Level","0");
}

void setValve(void){
  String valveInput = (Firebase.getString("Actuators/Valve Relay"));
  if (valveInput.equals("ON")){
    Serial.print("Valve Relay : ");
    Serial.println(valveInput);
    servoValve.write(40);
  }
  else if (valveInput.equals("OFF")){
    Serial.print("Valve Relay : ");
    Serial.println(valveInput);
    servoValve.write(75);
  }
}

void timer1_callISR(void) {
//  wdt_reset();
  counter ++;
  if (counter == calAvgPeriod){
    calAvgFlag = true;
    counter = 0;
  }
}
