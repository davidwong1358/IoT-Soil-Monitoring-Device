#include <Wire.h>
#include <Thinary_AHT10.h>
#include <SoftwareSerial.h>

//global variable
AHT10Class AHT10;
SoftwareSerial Nodemcu(2, 3); 

#define PUMP_PIN 8
#define SOIL_PIN A0
#define SOIL_POWER_PIN 13
#define LIGHT_PIN A1
#define PH_PIN A2

const int device_id = 1;

const int sensorInterval = 10*60*1000 ;//10000; //Sensor will send data every n/1000 sec
const int pump_period = 10000; //10 sec as a cycle of pump
const int pump_off_time = 5000; //turn off for 5 sec

unsigned long startTime;
unsigned long pumpStartTime;
unsigned long pumpEndTime;

bool sensors_ON;
bool pH_Ready;
bool pump_ON;
/////////////////

void setup() {
  
  Wire.begin();
  Serial.begin(9600);
  Nodemcu.begin(9600);
  
  
  pinMode(PH_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(SOIL_POWER_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH);
  digitalWrite(SOIL_POWER_PIN, LOW);
  
  sensors_ON = false;
  pump_ON = false;
  pH_Ready = false;
  
  pumpEndTime = 0;
  
  if (AHT10.begin(eAHT10Address_Low))
    Serial.println("Init AHT10 Success.");
  else
    Serial.println("Init AHT10 Failure.");
    
  Serial.println(String("") + "PH SENSOR WILL START AFTER 2 MINS");
  Serial.println(String("") + "Device ID: " + device_id);
}

void loop() {

  char r_command = 'z';
  unsigned long currentMillis; 
  float temp, humid, soil, ph;
  int light;
  
  //Receive Command from Nodemcu ESP8266
  if (Nodemcu.available() > 0) 
  {
    r_command = Nodemcu.read();
    //Serial.println(String("") + r_command);

    if (r_command == 'a') 
    {
      sensors_ON = true;
      Serial.println(String("") + "SENSOR: ON");
      Serial.println(String("") + "THE SENSOR WILL READ VALUE AFTER 10 SECS");
      startTime = millis();
    } 
    else if (r_command == 'b') 
    {
      pump_ON = true;
      controlPump(true);
      pumpStartTime = millis(); 
      pH_Ready = false; 
      Serial.println(String("") + "PUMP: ON");
      Serial.println(String("") + "PH SENSOR WILL BE DISABLED TEMPORARY.");  
    } 
    else if (r_command == 'c') 
    {
      sensors_ON = false;
      Serial.println(String("") + "SENSOR: OFF");
    } 
    else if (r_command == 'd') 
    {
      if(pump_ON == true)
      {
        pumpEndTime = millis();
        Serial.println(String("") + "PH SENSOR WILL BE ACTIVATED AFTER 2 MINS");
      }
      pump_ON = false;   
      controlPump(false);
      Serial.println(String("") + "PUMP: OFF");
    }
    /*
    else if (r_command == 'e') //Everything turn off
    {
      sensors_ON = false;
      Serial.println(String("") + "SENSOR: OFF");
      
      if(pump_ON == true)
      {
        pumpEndTime = millis();
        Serial.println(String("") + "PH SENSOR WILL BE ACTIVATED AFTER 2 MINS");
      }
      pump_ON = false;   
      controlPump(false);
      Serial.println(String("") + "PUMP: OFF");
    }
    else if (r_command == '+'){ //For demo
      device_id++;
      Serial.println(String("") + "Device ID: " + device_id);
    }
    else if (r_command == '-'){ //For demo
      device_id--;
      Serial.println(String("") + "Device ID: " + device_id);
    }
    */
  }
  
  currentMillis = millis();
  
  //pH sensor activate
  if(pump_ON == false && currentMillis - pumpEndTime > 120000 && pH_Ready == false) //2 mins
  {
    pH_Ready = true;
    Serial.println(String("") + "PH SENSOR IS ACTIVATED.");
  }
  
  //Sensor Interval
  if (currentMillis - startTime > sensorInterval && sensors_ON == true) 
  {
      startTime = millis();
      
      temp = getTemp();
      humid = getHumid();
      soil = getSoilMoisture();
      light = getLightLevel();
      
      if(pH_Ready == true)
      {
        ph = getPH();
      } else {
        ph = -1;
        Serial.println(String("") + "pH value:\t\t" + ph);
      }
      
      String str = toStr(temp,humid,soil,light,ph,device_id);
      
      Nodemcu.print(str);
  }
  
  //Pump ON Period Setup
  if(currentMillis - pumpStartTime > pump_period && pump_ON == true)
  {
    controlPump(true);
    pumpStartTime = millis();
  } 
  else if(currentMillis - pumpStartTime > pump_off_time && pump_ON == true)
  {
    controlPump(false);
  }

} //End of Loop

//Functions
float getTemp() {
  float temp = AHT10.GetTemperature();
  Serial.println(String("") + "Temperature(℃):\t\t" + temp + "℃");
  return temp;
}

float getHumid() {
  float humid = AHT10.GetHumidity();
  Serial.println(String("") + "Humidity(%RH):\t\t" + humid + "%");
  return humid;
}

float getSoilMoisture() {
  digitalWrite(SOIL_POWER_PIN, HIGH);  // Turn the sensor ON
  delay(20);
  int soil_output = analogRead(SOIL_PIN);
  float soilMoist = (0.00056 * pow(soil_output, 2)) - (0.960971 * soil_output) + 399.265;
  if(soilMoist <= 0)
  {
    soilMoist = 0;
  }
  if(soilMoist >= 100)
  {
    soilMoist = 100;
  }
  digitalWrite(SOIL_POWER_PIN, LOW);
  Serial.println(String("") + "Soil:\t\t\t" + soilMoist + "%" + "\nAnalog Read for Debug:\t" + soil_output);
  return soilMoist;
}

int getLightLevel() {
  int lightVal = analogRead(LIGHT_PIN);
  float light_level = -0.0223 * lightVal + 16.102;
  int round_light = int(light_level + 0.5);
  
  if (round_light >= 12){
    round_light = 12;
  } 
  else if (round_light <= 0){
    round_light = 0;
  }

  Serial.println(String("") + "Light Level:\t\t" + round_light + "\nAnalog Read for Debug:\t" + lightVal);
  return round_light;
}

float getPH() {
  float pH_calibration = 21.92;
  int pH_Val = analogRead(PH_PIN);
  float ph = -5.93 * pH_Val * (5.0 / 1023.0) + pH_calibration;
  
  if(ph >= 14)
  {
    ph = 14;
  }
  
  if (ph <= 0)
  {
    ph = 0;
  }
  Serial.println(String("") + "pH value:\t\t" + ph + "\nAnalog Read for Debug:\t" + pH_Val);
  return ph;
}

void controlPump(bool set) {
  if (set) {
    digitalWrite(PUMP_PIN, LOW);
  } 
  else 
  {
    digitalWrite(PUMP_PIN, HIGH); 
  }
}

String toStr (float t, float h, float s, int l, float p, int i){
  return String(t, 2) + "," + String(h, 2) + "," + String(s, 2) + "," + String(l, DEC) + "," + String(p, 2) + "," + String(i, DEC);
}
