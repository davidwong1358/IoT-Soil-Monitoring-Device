#include "FS.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "ESPAWSClient.h"
#include <coredecls.h>

#define TZ              +8      // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

const int device_id = 1;

//Setup Class
SoftwareSerial ArduinoUno(4, 5);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;

//Wifi setting
const char* ssid = "B2F51RmA_1";
const char* password = "yefung1983";

//MQTT setting
const char* send_data_topic = "SensorData";
const char* AWS_endpoint = "a18ls9ddgfsbwy-ats.iot.us-east-1.amazonaws.com"; //MQTT broker ip

//API Gateway settting
//const char* host = "31ukkhczse.execute-api.us-east-1.amazonaws.com";
const char *host = "31ukkhczse"; //API Gateway
const char *service = "execute-api";
String url = "/dev/registerState";
const char *key = "AKIAU2NX2MFNPANZOVNW";
const char *secret = "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";

//Callback function for subscribe MQTT topic to receive command and send to Arduino
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");

  Serial.print((char)payload[0]);
  while(ArduinoUno.available()){yield();};//Wait until ArduinoUno available (= 0)
  ArduinoUno.print((char)payload[0]);
  Serial.println();
}

//Set MQTT Client
PubSubClient mqtt_client(AWS_endpoint, 8883, callback, espClient);

//Set time for ESPAWS API Gateway
bool cbtime_set = false;

void time_is_set (void){
  cbtime_set = true;
}

//HTTP connection
static uint32_t requests = 0; 
ESPAWSClient aws = ESPAWSClient(service, key, secret, host);

void setup_wifi() {
  delay(10);
  // Connecting to a WiFi network
  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  timeClient.begin();
  timeClient.setTimeOffset(28800); //GMT +8 for HK Time
  
  while(!timeClient.update()){
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());
}

//MQTT reconnect
void reconnect() {
// Loop until reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
  // Attempt to connect
    if (mqtt_client.connect("ESPthing")) {
      Serial.println("connected");
      // Once connected, publish an announcement and resubscribe
      String device_topic = String(device_id, DEC);
      mqtt_client.subscribe(device_topic.c_str());
      Serial.println(String("") + "Subscribed Topic: " + device_topic);
      /*
      mqtt_client.subscribe(receive_data_topic);
      Serial.println(String("") + "Subscribed Topic :" + receive_data_topic);
      */
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
    
      char buf[256];
      espClient.getLastSSLError(buf,256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {

  Serial.begin(9600);
  ArduinoUno.begin(9600);
  
  //Serial.setDebugOutput(true);
  setup_wifi();
  delay(1000);
  
  //Time setting for ESPAWS
  /*
  settimeofday_cb(time_is_set);
  Serial.print("Getting time.");
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  while(!cbtime_set) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("Time setting done");
  */
  
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r"); //replace cert.crt eith your uploaded file name
  
  if (!cert)
    Serial.println("Failed to open cert file"); 
  else
    Serial.println("Success to open cert file");

  delay(1000);

  if (espClient.loadCertificate(cert))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r"); //replace private eith your uploaded file name
  
  if (!private_key)
    Serial.println("Failed to open private cert file");
  else
    Serial.println("Success to open private cert file");

  delay(1000);

  if (espClient.loadPrivateKey(private_key))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");

  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
  
  if (!ca)
    Serial.println("Failed to open ca ");
  else
    Serial.println("Success to open ca");

  delay(1000);

  if(espClient.loadCACert(ca))
    Serial.println("ca loaded");
  else
    Serial.println("ca failed");

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
}

void loop() {
  
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();
  
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  
  //aws.setInsecure();
  
  String r_str = "";
  
  float temp, humid, soil, pH;
  int light, splitT, id;
  String time_formatted, dayStamp , timeStamp;
  
  //Receive data from Arduino Uno Board and Publish to AWS via MQTT
  if (ArduinoUno.available() > 0 ) 
  { 
    StaticJsonDocument<256> reg;
    char msg[256];
    
    r_str = ArduinoUno.readString();
    Serial.println(String("") + "To str format:\t" + r_str);
    
    
    //Get Parameters
    temp = getValue(r_str,',',0).toFloat();
    humid = getValue(r_str,',',1).toFloat();
    soil = getValue(r_str,',',2).toFloat();
    light = getValue(r_str,',',3).toInt();
    pH = getValue(r_str,',',4).toFloat();
    id = getValue(r_str,',',5).toInt();

    if(id != device_id){
      Serial.println("Device ID not match.");
      return;
    }
        
    time_formatted = timeClient.getFormattedDate();
    splitT = time_formatted.indexOf("T");
    dayStamp = time_formatted.substring(0, splitT);
    timeStamp = time_formatted.substring(splitT + 1, time_formatted.length() - 1);
    
    //Serial.println(dayStamp);
    //Serial.println(timeStamp);
    
    reg["id"] = String(id, DEC);
    reg["soilTemp"] = temp;
    reg["soilHumd"] = soil;
    reg["soilPh"] = pH;
    reg["envTemp"] = temp;
    reg["envHumd"] = humid;
    reg["envLight"] = light;
    reg["date"] = dayStamp;
    reg["time"] = timeStamp;
    
    serializeJson(reg, msg);
    
    //Send as JSON format via MQTT
    Serial.print("Publish message: "); Serial.println(msg);
    mqtt_client.publish(send_data_topic, msg);

    //Send JSON via HTTP
    /*AWSResponse resp = aws.doPost(url, msg);
    if (resp.status != 200) {
      Serial.printf("Error: %s", resp.body.c_str());
      Serial.println();
    }*/
    
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap()); //Low heap may cause problems
  }
  
}

//Extract Substring
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;
 
    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
