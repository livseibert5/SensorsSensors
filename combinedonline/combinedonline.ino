#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Time.h>

#include <ESP32Servo.h>
 
Servo myservo;  // create servo object to control a servo
// 16 servo objects can be created on the ESP32
 
int pos = 0;    // variable to store the servo position
// Recommended PWM GPIO pins on the ESP32 include 2,4,12-19,21-23,25-27,32-33 
int servoPin = 32;

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 21

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
// change to 3600
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

ESP32Time rtc;

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

String jsonStr;

unsigned long sendDataPrevMillis = 0;
unsigned long sensorDataMillis = 0;
int count = 0;
float val = 0;
float temp = 0;
float light = 0;
bool signupOK = false;

void setup(){
  Serial.begin(9600);
  pinMode(39, INPUT);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    while (fbdo.to<String>() == "off") {
      //Serial.println(fbdo.to<String>());
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  // Set device to initially on.
  
  //FirebaseJson json;
  //json.set("switch", "on");
  if (Firebase.ready() && signupOK){
    if (Firebase.RTDB.setString(&fbdo, "switch/", "on")){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);    // standard 50 hz servo
  myservo.attach(servoPin, 500, 2400);
  
  //pinMode(4, INPUT);
  pinMode(A3, INPUT);
  //pinMode(21, INPUT);
  sensors.begin();
  //pinMode(4, INPUT);
}

void loop(){
  // If website powers device off, put it to sleep.
  delay(1000);
  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    while (fbdo.to<String>() == "off") {
      //Serial.println(fbdo.to<String>());
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  // Collect sensor data for 5 minutes.
  
  Serial.println("collecting data");
  Serial.println((float(analogRead(39)) / 4095.0) * 100.0);
  delay(1000);
  
  //while (millis() - sensorDataMillis < (100/*300*/ * uS_TO_S_FACTOR * .001)) {
  val = (float(analogRead(39)) / 4095.0) * 100.0;
    //delay(1000);
  delay(100);
  sensors.requestTemperatures();
  temp = (sensors.getTempCByIndex(0) * 1.8) + 32;
  delay(100);
  light = (float(analogRead(A3)) / 4095.0) * 100.0;
  delay(100);
 //}
  sensorDataMillis = millis();
  delay(1000);
  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    while (fbdo.to<String>() == "off") {
      //Serial.println(fbdo.to<String>());
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  FirebaseJson json;
  json.set("date", rtc.getDateTime());
  //Serial.println(val);
  json.set("moisture", String(val));
  json.set("temp", String(temp));
  json.set("light", String(light));
  delay(1000);
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    delay(1000);
    if (Firebase.RTDB.pushJSON(&fbdo, "data/", &json)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }

  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    while (fbdo.to<String>() == "off") {
      //Serial.println(fbdo.to<String>());
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  if (Firebase.RTDB.getString(&fbdo, "sunlim")) {
    Serial.println((fbdo.to<String>()).toInt());
    if ((fbdo.to<String>()).toInt() < light) {
      for (pos = 0; pos < 360; pos += 1) {
        myservo.write(pos);
        delay(15);
      }
    }
  }

  // Set sleep/wake cycle to an hour.
  Serial.println("sleeping");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}
