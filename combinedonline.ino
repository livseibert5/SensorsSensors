#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "LiviaiPhone"
#define WIFI_PASSWORD "liviaseibert"

// Insert Firebase project API Key
#define API_KEY "AIzaSyComL1C7ysYuAE5UzTZwpOL5z8PcXU8WJM"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "ece449-b1145-default-rtdb.firebaseio.com"

#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 21

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  3600        /* Time ESP32 will go to sleep (in seconds) */

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

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
  
  pinMode(36, INPUT);
  pinMode(34, INPUT);
  sensors.begin();
}

void loop(){
  // If website powers device off, put it to sleep.
  //Serial.println(Firebase.RTDB.getString(&fbdo, "switch"));
  if (Firebase.RTDB.getString(&fbdo, "switch")) {
    Serial.println(fbdo.to<String>());
    if (fbdo.to<String>() == "off") {
      //Serial.println(fbdo.to<String>());
      esp_deep_sleep_start();
    }
  }
  // Collect sensor data for 5 minutes.
  Serial.println("collecting data");
  while (millis() - sensorDataMillis < (300 * uS_TO_S_FACTOR * .001)) {
    val = analogRead(36) / 4095;
    sensors.requestTemperatures();
    temp = (sensors.getTempCByIndex(0) * (9/5)) + 32;
    light = analogRead(34);
  }
  sensorDataMillis = millis();

  FirebaseJson json;
  json.set("date", String(millis()));
  json.set("moisture", String(val));
  json.set("temp", String(temp));
  json.set("light", String(light));
  delay(1000);
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    // Write an Int number on the database path test/int
    
    if (Firebase.RTDB.pushJSON(&fbdo, "data/", &json)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }

  // Set sleep/wake cycle to an hour.
  Serial.println("sleeping");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}
