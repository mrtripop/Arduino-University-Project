#include <WiFi.h>
#include <IOXhop_FirebaseESP32.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"

#define FIREBASE_HOST "door-project-2-default-rtdb.firebaseio.com"                         
#define FIREBASE_AUTH "M7rU6NAf8JigMzHjNcykobMOWk9zZuQDKkSCfIsk" 

#define led 15
#define CTSW2 2
#define CTSW1 0
#define CL1 4
#define CL2 16
#define Buzzer 17

const char* pref_ssid = "";
const char* pref_pass = "";
const char* pref_uid = "";
String client_wifi_ssid;
String client_wifi_password;
String client_uid;
String client_wifi_Input;

String connected_string;
String fireStatus = "";

bool bluetooth_disconnect = false;

long start_wifi_millis;
long wifi_timeout = 10000;

enum wifi_setup_stages { NONE, START, WAIT_PASS, PASS_ENTERED, WAIT_CONNECT, LOGIN_FAILED, WIFI_CONNECTED, ONLINE_STATE, MAIN_LOOP};
enum wifi_setup_stages wifi_stage = NONE;

BluetoothSerial SerialBT;
Preferences preferences;

void setup()
{
  Serial.begin(115200);
  Serial.println("Booting...");

  preferences.begin("wifi_access", false);

  if(!init_wifi()){ 
    SerialBT.register_callback(callback);
  }else{
    wifi_stage = ONLINE_STATE;
    Serial.println(WiFi.localIP()); 
    init_firebase();
    bluetooth_disconnect = true;
  }
  
  SerialBT.begin("ESP32-Door");

  pinMode(led, OUTPUT); 
  pinMode(CTSW2,OUTPUT);
  pinMode(CTSW1,OUTPUT);
  pinMode(CL1,OUTPUT);
  pinMode(CL2,OUTPUT);
  pinMode(Buzzer,OUTPUT);

  digitalWrite(CTSW2,HIGH); //all close
  digitalWrite(CTSW1,HIGH);
  digitalWrite(CL1,HIGH);
  digitalWrite(CL2,HIGH);
  digitalWrite(Buzzer,HIGH); 
}

void init_firebase(){
  String temp_pref_uid = preferences.getString("pref_uid");
  pref_uid = temp_pref_uid.c_str();

  client_uid = "/"+String(pref_uid)+"/command/stateDoor";
  Serial.println(client_uid);
  
  if(client_uid == ""){
    preferences.putString("pref_ssid", "");
    preferences.putString("pref_pass", "");
    Serial.print("clear");
    ESP.restart(); 
  }else{
//      preferences.putString("pref_ssid", "");
//      preferences.putString("pref_pass", "");
//      preferences.putString("pref_uid", "");
//      
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.stream(client_uid, [](FirebaseStream stream) {
      fireStatus = stream.getDataString();
    }); 
  }  
}

bool init_wifi()
{
  String temp_pref_ssid = preferences.getString("pref_ssid");
  String temp_pref_pass = preferences.getString("pref_pass");
  pref_ssid = temp_pref_ssid.c_str();
  pref_pass = temp_pref_pass.c_str();

  Serial.println(pref_ssid);
  Serial.println(pref_pass);

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

  start_wifi_millis = millis();
  WiFi.begin(pref_ssid, pref_pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start_wifi_millis > wifi_timeout) {
      WiFi.disconnect(true, true);
      return false;
    }
  }
  return true;
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    if (event == ESP_SPP_SRV_OPEN_EVT) {
        wifi_stage = WAIT_PASS;
        Serial.println("WIFI state WAIT_INPUT");
    }

    if (event == ESP_SPP_DATA_IND_EVT && wifi_stage == WAIT_PASS) { 
        client_wifi_Input = SerialBT.readString();
        client_wifi_Input.trim();

        DynamicJsonBuffer  jb;
        JsonObject& doc = jb.parseObject(client_wifi_Input);

        String ssid = doc["ssid"];
        String pass = doc["password"];
        String uid = doc["uid"];
        
        client_wifi_ssid = ssid;
        client_wifi_password = pass;
        client_uid = uid;

        Serial.println(client_wifi_ssid);
        Serial.println(client_wifi_password);
        Serial.println(client_uid);
        
        wifi_stage = PASS_ENTERED;
    }
}

void loop(){
  if(bluetooth_disconnect){
    SerialBT.flush();
    SerialBT.disconnect();
    SerialBT.end();
    bluetooth_disconnect = false;
  }
  
  switch (wifi_stage)
  {
    case START:
      Serial.println("Please enter ssid your wifi");
      wifi_stage = WAIT_PASS;
      break;

    case PASS_ENTERED:
      Serial.println("Please wait for Wi_Fi connection...");
      wifi_stage = WAIT_CONNECT;
      preferences.putString("pref_ssid", client_wifi_ssid);
      preferences.putString("pref_pass", client_wifi_password);
      preferences.putString("pref_uid", client_uid);
      if (init_wifi()) {
        connected_string = "ESP32 IP: ";
        connected_string = connected_string + WiFi.localIP().toString();
        SerialBT.println(connected_string);
        Serial.println(connected_string);
        bluetooth_disconnect = true;
        
        Serial.println("Rebooting...");
        ESP.restart(); 
      } else { 
        wifi_stage = LOGIN_FAILED;
      }
      break;

    case LOGIN_FAILED:
      SerialBT.println("Wi-Fi connection failed");
      Serial.println("Wi-Fi connection failed");
      delay(2000);
      wifi_stage = WAIT_PASS;
      break;

    case ONLINE_STATE:
      if(fireStatus == "OPEN"){
        Serial.println(fireStatus);
        wifi_stage = MAIN_LOOP;
        mainWorkLoad(); 
      }
      break;  
  }
}

void mainWorkLoad(){                                                                           
  digitalWrite(CL1,LOW); //switch line between keycard & app
  digitalWrite(CL2,LOW);
  digitalWrite(CTSW2,HIGH); 
  digitalWrite(CTSW1,HIGH);
      
  openDoor(); 
  delay(3000);
  closeDoor();
      
  digitalWrite(CL1,HIGH); //all close
  digitalWrite(CL2,HIGH);
  digitalWrite(CTSW1,HIGH);
  digitalWrite(CTSW2,HIGH);

  Serial.println("CLOSE");
  Firebase.setString(client_uid, "CLOSE");
  fireStatus = "";
  wifi_stage = ONLINE_STATE;
}

void openDoor(){
    digitalWrite(led, HIGH); //led
    
    digitalWrite(Buzzer, LOW); //Sound on
    delay(650);
    digitalWrite(Buzzer, HIGH); //Sound off
                                                             
    digitalWrite(CTSW1,LOW); //motor open door int1 running
    digitalWrite(CTSW2,HIGH);
    delay(500);
    digitalWrite(CTSW1,HIGH);
}

void closeDoor(){
    digitalWrite(led, LOW); //led

    digitalWrite(Buzzer, LOW); //Sound on
    delay(100);
    digitalWrite(Buzzer, HIGH); //Sound off
    delay(100);
    digitalWrite(Buzzer, LOW); //Sound on
    delay(100);
    digitalWrite(Buzzer, HIGH); //Sound off
    
    digitalWrite(CTSW2,LOW); //motor close door int2 running
    digitalWrite(CTSW1,HIGH); 
    delay(500);
}
