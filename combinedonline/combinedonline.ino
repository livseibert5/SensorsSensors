#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID // ADD HERE
#define WIFI_PASSWORD // ADD HERE

// Insert Firebase project API Key
#define API_KEY // ADD HERE

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL // ADD HERE

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
#define TIME_TO_SLEEP  3600        /* Time ESP32 will go to sleep (in seconds) */

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
float moisture = 0;
float temp = 0;
float light = 0;
bool signupOK = false;
bool advancedMode = false;
bool basicMode = false;

void setup(){
  Serial.begin(9600);

  // WIFI SETUP
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

  // FIREBASE CREDENTIALS
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // FIREBASE LOGIN
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  // FIREBASE CONNECTION
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // PUT SYSTEM TO SLEEP AGAIN IF OFF
  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    if (fbdo.to<String>() == "off") {
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  // SET DEVICE TO ON BEFORE RUNNING
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

  // SET UP SERVO
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);
  myservo.attach(servoPin, 500, 2400);

  // RESPOND TO SHADE ACTION FROM WEBSITE
  if (Firebase.RTDB.getString(&fbdo, "rollaction")) {
    String rollaction = fbdo.to<String>();
    if (rollaction == "open") {
      openShade();
      Firebase.RTDB.setString(&fbdo, "shade", "open");
    } else if (rollaction == "close") {
      closeShade();
      Firebase.RTDB.setString(&fbdo, "shade", "closed");
    }
    Firebase.RTDB.setString(&fbdo, "rollaction", "none");
  }

  // INCREMENT ADVANCED MODE TIME
  if (advancedMode && Firebase.RTDB.getInt(&fbdo, "time")) {
    int curr = fbdo.to<int>();
    // IF 6 DAYS HAVE PASSED PLANTS ARE HARDENED
    if (curr > 144) {
      Firebase.RTDB.setInt(&fbdo, "time/", 0);
      advancedMode = false;
    } else {
      Firebase.RTDB.setInt(&fbdo, "time/", curr + 1);
    }
  }

  // START ADVANCED MODE
  if (Firebase.RTDB.getInt(&fbdo, "time")) {
    if (fbdo.to<int>() >= 1) {
      advancedMode = true;
    }
  }

  // START BASIC MODE
  if (Firebase.RTDB.getString(&fbdo, "basic")) {
    if (fbdo.to<String>() == "on") {
      basicMode = true;
    } else {
      basicMode = false;
    }
  }

  // SET UP SENSOR PINS
  pinMode(A3, INPUT);
  pinMode(34, INPUT);
  sensors.begin();
}

void loop(){
  delay(1000);
  Serial.println(advancedMode);

  // PUT SYSTEM TO SLEEP IF OFF
  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    if (fbdo.to<String>() == "off") {
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  // Collect sensor data for 5 minutes.
  Serial.println("collecting data");
  delay(1000);
  
  while (millis() - sensorDataMillis < (300 * uS_TO_S_FACTOR * .001)) {
    moisture = (float(analogRead(34)) / 4095.0) * 100.0;
    //delay(1000);
    delay(100);
    sensors.requestTemperatures();
    temp = (sensors.getTempCByIndex(0) * 1.8) + 32;
    delay(100);
    light = (float(analogRead(A3)) / 4095.0) * 100.0;
    delay(100);
  }
  sensorDataMillis = millis();
  delay(1000);

  // PUT SYSTEM TO SLEEP IF OFF
  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    while (fbdo.to<String>() == "off") {
      //Serial.println(fbdo.to<String>());
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  // ADVANCED MODE
  if (advancedMode && Firebase.RTDB.getInt(&fbdo, "time")) {
    int currTime = fbdo.to<int>();
    Firebase.RTDB.getString(&fbdo, "shade");
    String shadePos = fbdo.to<String>();
    bool shadeOpen = shadePos == "open";
    if (!shadeOpen && currTime % 24 == 0) {
      openShade();
      Firebase.RTDB.setString(&fbdo, "shade", "open");
    }
    int suntoday = 0;
    if (light > 65 && shadePos == "open") {
      Firebase.RTDB.getInt(&fbdo, "suntoday");
      suntoday = fbdo.to<int>();
      Firebase.RTDB.setInt(&fbdo, "suntoday", suntoday + 1);
    }
    bool day1 = currTime < 24 && suntoday > 2 && shadeOpen;
    bool day2 = currTime > 24 && currTime < 48 && suntoday > 3 && shadeOpen;
    bool day3 = currTime > 48 && currTime < 72 && suntoday > 4 && shadeOpen;
    bool day4 = currTime > 72 && currTime < 96 && suntoday > 5 && shadeOpen;
    bool day5 = currTime > 96 && currTime < 120 && suntoday > 6 && shadeOpen;
    bool day6 = currTime > 120 && currTime < 144 && suntoday > 7 && shadeOpen;
    if (day1 || day2 || day3 || day4 || day5 || day6) {
      closeShade();
      Firebase.RTDB.setString(&fbdo, "shade", "closed");
      Firebase.RTDB.setInt(&fbdo, "suntoday", 0);
    }
  }

  // BASIC MODE
  if (basicMode) {
    Firebase.RTDB.getString(&fbdo, "sunlim");
    int sunlim = (fbdo.to<String>()).toInt();
    Firebase.RTDB.getString(&fbdo, "shade");
    String shadePos = fbdo.to<String>();
    bool shadeOpen = shadePos == "open";
    if (light > sunlim && shadeOpen) {
      closeShade();
      Firebase.RTDB.setString(&fbdo, "shade", "closed");
    }
    if (light < sunlim && !shadeOpen) {
      openShade();
      Firebase.RTDB.setString(&fbdo, "shade", "open");
    }
  }

  // TURN SENSOR DATA INTO JSON
  FirebaseJson json;
  json.set("date", rtc.getDateTime());
  json.set("moisture", String(moisture));
  json.set("temp", String(temp));
  json.set("light", String(light));

  // UPLOAD SENSOR DATA TO FIREBASE
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

  // PUT SYSTEM TO SLEEP IF OFF
  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    while (fbdo.to<String>() == "off") {
      //Serial.println(fbdo.to<String>());
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }

  // SET SLEEP/WAKE CYCLE
  Serial.println("sleeping");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void closeShade() {
  Serial.println("closing");
  myservo.detach();
  delay(2000);
  myservo.attach(32);
  myservo.write(0);
  delay(21500);
  myservo.detach();
}

void openShade() {
  Serial.println("opening");
  myservo.detach();
  delay(2000);
  myservo.attach(32);
  myservo.write(180);
  delay(22500);
  myservo.detach();
}
