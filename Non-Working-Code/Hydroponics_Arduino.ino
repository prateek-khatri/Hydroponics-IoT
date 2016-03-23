#include<Arduino.h>
//********************
//PIN DEFINITIONS
//********************
//RELAY PINS
#define RELAY_PH_ACID_PUMP 4
#define RELAY_PH_BASE_PUMP 5
#define RELAY_EC_PUMP_ONE 6
#define RELAY_EC_PUMP_TWO 7
#define RELAY_WATER_PUMP 8
#define RELAY_LIGHT_SWITCH 9

//SENSOR PINS
#define EC_SENSOR_PIN_ONE 2
#define EC_SENSOR_PIN_TWO 3
#define SOIL_MOISTURE_SENSOR_PIN A3
#define LIGHT_SENSOR_PIN A2
#define SERIAL_RX 13
#define SERIAL_TX 12

//********************
//GLOBAL DEFINITIONS
//********************
//SENSOR VALUES
int soil_moisture;
int light_intensity;
float pHValue;
float ecValue;
String requestStringThresholds[6];


//THRESHOLDS
int soil_moisture_min;
int light_intensity_min;
float pH_min;
float pH_max;
float ec_min;
float ec_max;

//ACTUATOR STATE
bool phActuator;
bool ecActuator;
bool lightActuator;
bool waterActuator;

//DELTA VALUES (To be set manually depending on data frequency required)
#define SOIL_MOISTURE_DELTA 5 //(PERCENT)
#define LIGHT_INTENSITY_DELTA 5 //(PERCENT)
#define PH_DELTA 0.25 //pH scale
#define EC_DELTA 0.1 //ec scale


#include "Wire.h"
#define pHtoI2C 0x48
#define T 273.15     
#include <SoftwareSerial.h>
SoftwareSerial ESPserial(SERIAL_RX, SERIAL_TX); // RX | TX

void showInitSerialMessage()
{
  Serial.println("Serial Ports Init, System Running");
  Serial.println("");
}

void requestThresholds() // at boot
{
  //communicate with ESP - > wait for timeout
  ESPserial.println("1"); // If ESP receives 1 then boot
  Serial.println("Waiting for Thresholds...");
  while(!(ESPserial.available() >0))
  {
    Serial.print(".");
    delay(1000);
  }
//  delay(1000);
//  if(!ESPserial.available())
//  {
//    Serial.println("Waiting 1 min for response...");
//    //delay(60000);//Boot time delay
//  }
  int i=0;
  while( ESPserial.available() )   
  {
      char a = ESPserial.read();
      if(a==',')
      {
        i++;
        a= ESPserial.read();
      }
      requestStringThresholds[i] = requestStringThresholds[i]+a;  
      if(!ESPserial.available())
      {
        Serial.println();
        Serial.flush();
      }
  }

}
void setNewThresholds(String req[])
{
  //Parse String here
  /*
   * 0 - ph min
   * 1 - ph max
   * 2 - ec min
   * 3 - ec max
   * 4 - soil moisture level
   * 5 - light moisture level
   */
   pH_min = req[0].toFloat();
   pH_max = req[1].toFloat();
   ec_min = req[2].toFloat();
   ec_max = req[3].toFloat();
   soil_moisture_min = req[4].toInt();
   light_intensity_min = req[5].toInt();
  
  
  
}
void setPinModes()
{
  phActuator = false;
  ecActuator = false;
  lightActuator = false;
  waterActuator = false; 
  pinMode(RELAY_PH_ACID_PUMP,OUTPUT);
  pinMode(RELAY_PH_BASE_PUMP,OUTPUT);
  pinMode(RELAY_EC_PUMP_ONE,OUTPUT);
  pinMode(RELAY_EC_PUMP_TWO,OUTPUT);
  pinMode(RELAY_WATER_PUMP,OUTPUT);
  pinMode(RELAY_LIGHT_SWITCH,OUTPUT);
  digitalWrite(RELAY_PH_ACID_PUMP,LOW);
  digitalWrite(RELAY_PH_BASE_PUMP,LOW);
  digitalWrite(RELAY_EC_PUMP_ONE,LOW);
  digitalWrite(RELAY_EC_PUMP_TWO,LOW);
  digitalWrite(RELAY_WATER_PUMP,LOW);
  digitalWrite(RELAY_LIGHT_SWITCH,LOW);
  
}
void readAllSensors()
{
  readLightIntensity();
  readSoilMoisture();
  readEC();
  readPH();
}

bool matchDeltas()
{
  bool eCState = (abs(ecValue - scanEcValue()) > EC_DELTA);
  bool pHState = (abs(pHValue - scanpHValue()) > PH_DELTA);
  bool soilState = (abs(soil_moisture - scanSoilMoisture()) > SOIL_MOISTURE_DELTA);
  bool lightState = (abs(light_intensity - scanLightIntensity()) > LIGHT_INTENSITY_DELTA);
  if(eCState || pHState || soilState || lightState)
  {
    return true;
  }
  return false;
  
}

bool matchThresholds()
{
  return false;
}
void initRequestStringThresholds()
{
  int i =0;
  for(i=0;i<6;i++)
  {
    requestStringThresholds[i] = "";
  }
}
void waitForResponse() //RECEIVES THRESHOLDS
{
  Serial.println("Waiting for Response from ESP...");
  while(!(ESPserial.available()>0));
  initRequestStringThresholds();
  int i=0;
  while(ESPserial.available()>0)
  {
    char a = ESPserial.read();
    if(a==',')
    {
      i++;
      a=ESPserial.read();
    }
    requestStringThresholds[i] +=a;
  }
  Serial.print("Response from 8266 Received: ");
  Serial.println("Updating Thresholds...");
  setNewThresholds(requestStringThresholds);
  
}
/*
 * Code 0: Normal Delta Update
 * Code 2: Threshold Violation, need to follow with actuator status
 * Code 9: Actuator status string
 */
void sendUpdate(int code)
{
  readAllSensors();
  //Send sensor values to ESP
  //Wait for new thresholds from ESP
  //pH,ec,soil,light

  String sensorVals = String(code)+","+String(pHValue,2)+","+String(ecValue,2)+","+String(soil_moisture,DEC)+","+String(light_intensity,DEC);
  Serial.print("Sending Values to ESP8266: ");
  Serial.println(sensorVals);
  ESPserial.print(sensorVals);

  waitForResponse();
}

void sendActuatorUpdate()
{
  int ph,ec,soil,light;
  if(phActuator == true) ph = 1;
  else ph = 0;
  if(ecActuator == true) ec = 1;
  else ec = 0;
  if(waterActuator == true) soil = 1;
  else soil = 0;
  if(lightActuator == true) light = 1;
  else light = 0; 
  String acVals = "9,"+String(ph)+","+String(ec)+","+String(soil)+","+String(light);
  Serial.println("Sending Actuator Values to ESP8266:");
  Serial.println(acVals);
  ESPserial.print(acVals);
  
}

boolean checkPHLevels()
{
  if(pHValue < pH_min)
  {
    Serial.println("PH BASE PUMP : ON");
    digitalWrite(RELAY_PH_BASE_PUMP,HIGH);
    digitalWrite(RELAY_PH_ACID_PUMP,LOW);
    phActuator = true;
  }
  else if(pHValue > pH_max)
  {
    Serial.println("PH ACID PUMP : ON");
    digitalWrite(RELAY_PH_ACID_PUMP,HIGH);
    digitalWrite(RELAY_PH_BASE_PUMP,LOW);
    phActuator = true;
  }
  else
  {
    digitalWrite(RELAY_PH_ACID_PUMP,LOW);
    digitalWrite(RELAY_PH_BASE_PUMP,LOW);
    phActuator = false;
  }
  return phActuator;
  
}

boolean checkECLevels()
{
  if(ecValue < ec_min)
  {
    Serial.println("NUTRIENT PUMP : ON");
    digitalWrite(RELAY_EC_PUMP_ONE,HIGH);
    digitalWrite(RELAY_EC_PUMP_TWO,LOW);
    ecActuator = true;
  }
  else if(ecValue > ec_max)
  {
    Serial.println("WATER(Nutrient) PUMP: ON");
    digitalWrite(RELAY_EC_PUMP_ONE,HIGH);
    digitalWrite(RELAY_EC_PUMP_TWO,LOW);
    ecActuator = true;
  }
  else
  {
    digitalWrite(RELAY_EC_PUMP_ONE,LOW);
    digitalWrite(RELAY_EC_PUMP_TWO,LOW);
    ecActuator = false;
  }
  return ecActuator;
}

boolean checkMoistureLevels()
{
  if(soil_moisture < soil_moisture_min)
  {
    Serial.println("SPRINKLER : ON");
    digitalWrite(RELAY_WATER_PUMP,HIGH);
    waterActuator = true;
  }
  else
  {
    digitalWrite(RELAY_WATER_PUMP,LOW);
    waterActuator = false;
  }
  return waterActuator;
  
}

boolean checkLightLevels()
{
  if(light_intensity < light_intensity_min)
  {
    Serial.println("LIGHT : ON");
    digitalWrite(RELAY_LIGHT_SWITCH,HIGH);
    lightActuator = true;
  }
  else
  {
    digitalWrite(RELAY_LIGHT_SWITCH,LOW);
    lightActuator = false;
  }
  return lightActuator;
  
}

void activateActuators()
{
  boolean prevVals[] = {phActuator,ecActuator,waterActuator,lightActuator};
  //add code 9 sendUpdate(9) and attach actuator status;
  if((prevVals[0] != checkPHLevels()) || (prevVals[1] != checkECLevels()) || (prevVals[2] != checkMoistureLevels()) || (prevVals[3] != checkLightLevels()))
  {
    sendActuatorUpdate();
  }
  
}
void readSoilMoisture()
{
  float a =(float) analogRead(SOIL_MOISTURE_SENSOR_PIN); 
  a = a*100;
  a = a/1023;
  soil_moisture = (int)a;
}
void readLightIntensity()
{
  float a =(float) analogRead(LIGHT_SENSOR_PIN); 
  a = a*100;
  a = a/1023;
  light_intensity = (int)a;
}
void readPH()
{
  
}
void readEC()
{

}

int scanSoilMoisture()
{
  float a =(float) analogRead(SOIL_MOISTURE_SENSOR_PIN); 
  a = a*100;
  a = a/1023;
  return (int)a;
}
int scanLightIntensity()
{
  float a =(float) (analogRead(LIGHT_SENSOR_PIN)-255); 
  a = a/(1023-255);
  a = a*100;
  return (int)a;
}

float scanpHValue()
{
  return 0.0;
}
float scanEcValue()
{
  return 0.0;
}

void setup() 
{
   //INIT SERIAL PORTS
   ESPserial.begin(115200);
   ESPserial.flush();
   Serial.begin(9600);
   Serial.flush();
   showInitSerialMessage();
   //SEND THRESHOLD REQUESTS -: THIS FUNCTION WILL WAIT FOR A REPLY OR KEEP SENDING REQUEST
   initRequestStringThresholds();
   while(requestStringThresholds[0].equals(""))
   {
    requestThresholds();
   }
   //SET THE THRESHOLDS TO WORK WITH
   setNewThresholds(requestStringThresholds);
   //SET PIN MODES
   setPinModes();
    
}
void loop() 
{
  readAllSensors();
  activateActuators();
  delay(10000);
  if(matchDeltas() == true)
  {
    if(matchThresholds() == true)
    {
      sendUpdate(2);
      activateActuators();
    }
    else
    {
      sendUpdate(0);
    }
  }

  
}
