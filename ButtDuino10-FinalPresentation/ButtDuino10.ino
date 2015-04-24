/*
  The Mighty Butt(ler)duino!!!!
 */

// Libraries for comms
#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LDateTime.h>
#include <ArduinoJson.h>

// Libraries for MPU and LCD
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU9250.h"
#include "rgb_lcd.h"

// Definitions for MPU
#define LOW_HYST  0.25  // Lowest boundary of the hysteresis
#define HIGH_HYST  0.45  // Highest boundary of the hysteresis

#define STEPWAY_POS  true    // To know which way we are moving along the hysteresis (increasing)
#define STEPWAY_NEG  false   // To know which way we are moving along the hysteresis (decreasing)

// Wifi definitions
#define WIFI_AP "BASH"
#define WIFI_PASSWORD "BASH@infocomm"
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.
#define SITE_URL "thebutlers.tk"


// Variable for MPU/LCD
MPU9250 accelgyro;     // The class for the IMU, default address is 0x68
rgb_lcd lcd;           // The class for the LCD, default address is 0x7c (LCD_ADDRESS) and 0xc4 (RGB_ADDRESS)

float Axyz[3];         // Storage for the accelerations on 3 axes
float Gxyz[3];         // Storage for the gyro on 3 axes (not used for the moment)

int g_nStepCount;      // Number of steps performed
int g_nPrevStepCount;  // Previous step count to know whether it has to be updated
bool g_bStepWay;       // Which way we are moving along the hysteresis (to know which boundary is going to trigger the step count).

int colorR = 255;
int colorG = 255;
int colorB = 255;

// Comms Variables
LWiFiClient c;
datetimeInfo CurrentTime;

boolean disconnectedMsg = false;

// Multitasking variables

unsigned long previousCommMillis = 0;
unsigned long previousNFCMillis = 0;
unsigned long previousLCDMillis = 0;

const int CommInterval = 5000;
const int NFCInterval = 1000;
const int LCDInterval = 500;


// Comm States
enum {
  CommIdle,
  CommStart,
  CommUpdate,
  CommFinish
  
};

// NFC States
enum {
  NFCWaiting,
  NFCTapped
};

// LCD States
enum {
  LCDStartWorkout,
  LCDShowSteps,
  LCDWorkoutDone
};

// Time States
enum {
  TimeUnknown,
  TimeKnown
};



// Initial States
int CommState = CommIdle;
int NFCState = NFCWaiting;
int LCDState = LCDStartWorkout;
int TimeState = TimeUnknown;

int WorkOutID;

int UserID = 3;
int CardTapped = 0;
int PreviousCardTapped = 0;

void setup()
{
  LTask.begin();
  LWiFi.begin();
  // join I2C bus (I2Cdev library doesn't do this automatically)
  Wire.begin();
  
  Serial.begin(9600);
  Serial1.begin(9600);

  
  // Setup step count
  g_nStepCount = 0;
  g_nPrevStepCount = 0;
  g_bStepWay = STEPWAY_NEG;
  setStepCount(0);
  
  // Setup LCD
  lcd.begin(16,2);
  lcd.setRGB(colorR, colorG, colorB);
  
  
  // Setup IMU
  lcd.setRGB(255, 0, 0);
  // Initialize IMU
  lcd.print("IMU init...");
  accelgyro.initialize();

  lcd.setRGB(0, 255, 0);
  lcd.setCursor(8,1);
  lcd.print("Success!");

  delay(2000);
  Serial.println("     ");
  
  lcd.clear();

  // Connect to wifi  
  ConnectToWifi();
  GetTimeAPI();

}


void loop()
{

  unsigned long currentMillis = millis();
  
  if ((currentMillis - previousCommMillis) > CommInterval) {
    previousCommMillis = currentMillis; 
    Serial.println("COMMS DOING ITS STUFF");
    if (CommState == CommStart) {
        setStepCount(0);
        String JsonReceived = PostToServer("/exercise/create", BuildJSONCreate(1, GetUTCTime(), UserID));
        if (UserID == 3){
          UserID = 5;
        } else {
          UserID = 3;
        }
        WorkOutID = GetIDfromJSON(JsonReceived);     
        CommState = CommUpdate;
     }
    else if (CommState == CommUpdate) {
        PostToServer("/exercise/update/"+String(WorkOutID), BuildJSONUpdate(getStepCount()));
    } 
    else if (CommState == CommFinish) {
        PostToServer("exercise/update/"+String(WorkOutID), BuildJSONFinish(getStepCount(), GetUTCTime()));
    }
    else if (CommState == CommIdle) {
        Serial.println("Comms Idle");
    }
  }
  
  if ((currentMillis - previousNFCMillis) > NFCInterval) {
    previousNFCMillis = currentMillis;
    Serial.println("NFC DOING ITS STUFF");
      while (Serial1.available() > 0) {
        CardTapped = 1;
        for (int i=0 ; i<7 ; i++){
          char inByte = (char) Serial1.read();
        }
    }
  if (CardTapped == 1) {
    if (PreviousCardTapped == 0){
        if (CommState == CommIdle) {
            CommState = CommStart;
            lcd.clear();
            LCDState = LCDShowSteps;
          } else if (CommState != CommIdle) {
            CommState = CommIdle;
            setStepCount(0);
            lcd.clear();
            LCDState = LCDStartWorkout;
          }
      } 
      PreviousCardTapped = 1;
    }
    else {
      PreviousCardTapped = 0;
    }
    Serial.println("card released");    
    CardTapped = 0;
    Serial.println("");
  }

  if ((currentMillis - previousLCDMillis) > LCDInterval) {
    previousLCDMillis = currentMillis;
    
    if (LCDState == LCDShowSteps) {
      Serial.println("LCDShowSteps");
      lcd.setRGB(0, 255, 0);
      lcd.setCursor(0,0);
      
      lcd.print("Number of steps:");
      
      int nStepCount = getStepCount();
      if (nStepCount != g_nPrevStepCount)
      {
        Serial.println(nStepCount);
        lcd.setCursor(0,1);
        lcd.print(nStepCount);
        g_nPrevStepCount = nStepCount;
      }
      
      }
       else if (LCDState==LCDWorkoutDone){
      Serial.println("LCDStartWorkOut");
         Serial.println("LCD workout done");
        lcd.setRGB(0, 0, 255);
        lcd.setCursor(0,0);
        lcd.print("Workout Complete!!!");
        lcd.setCursor(0,1);
        lcd.print(getStepCount());
        delay(6000);
        LCDState = LCDStartWorkout;
        lcd.clear();
       }
       else if (LCDState==LCDStartWorkout){
        Serial.println("LCDStartWorkOut");
        lcd.setRGB(255, 0, 0);
        lcd.setCursor(0,0);
        lcd.print("Ready");
        lcd.setCursor(0,1);
        lcd.print(GetUTCTime());
       }
  }
}



// FUNCTIONS START HERE

// MPU & LCD FUNCTIONS/ CLASSES
// Public functions

int getStepCount(){
  
  // Retrieve raw data from IMU
  getIMU_Data();
  
  // Move along the hysteresis and check if a limit is crossed
  if(g_bStepWay == STEPWAY_POS && Axyz[2] > HIGH_HYST)
  {
    g_nStepCount++;
    g_bStepWay = STEPWAY_NEG;
  }
  else if(g_bStepWay == STEPWAY_NEG && Axyz[2] < LOW_HYST)
  {
    g_nStepCount++;
    g_bStepWay = STEPWAY_POS;
  }
  
  return g_nStepCount;
}


void setStepCount(int l_nStepCount){
  
  g_nStepCount = l_nStepCount;
  
}







// Private functions



void getIMU_Data(void)
{
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  Axyz[0] = (double) ax / 16384;//16384  LSB/g
  Axyz[1] = (double) ay / 16384;
  Axyz[2] = (double) az / 16384; 
  
  Gxyz[0] = (double) gx * 250 / 32768;//131 LSB(��/s)
  Gxyz[1] = (double) gy * 250 / 32768;
  Gxyz[2] = (double) gz * 250 / 32768;
}

// Comms functions/classes



int ConnectToWifi () {
 // keep retrying until connected to AP
  Serial.println("Connecting to AP");
  while (0 == LWiFi.connect(WIFI_AP, LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)))
  {
    delay(1000);
  }

  // keep retrying until connected to website
  Serial.println("Connecting to WebSite");
  while (0 == c.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }

  return 0;
}


// Helper to convert int 5 into "05"
String IntTo2String(int Input){
  if (Input<10)
  {
    return "0"+String(Input);
  }
  else {
    return String(Input);
  }
}

// Function to get time in UTC format
String GetUTCTime() {
  LDateTime.getTime(&CurrentTime);
  return String(CurrentTime.year)+"-"+IntTo2String(CurrentTime.mon)+"-"+IntTo2String(CurrentTime.day)+"T"+IntTo2String(CurrentTime.hour)+":"+IntTo2String(CurrentTime.min)+":"+IntTo2String(CurrentTime.sec)+".000+08:00";
}


// Functions to create JSON for API
String BuildJSONCreate(int Machine, String StartTime, int UserID){
  return  "{\"machine\": "+String(Machine)+", \"startTime\": \""+StartTime+"\", \"user\": "+String(UserID)+"}";
}

String BuildJSONUpdate(int numberOfSteps){
  return  "{\"numberOfSteps\": "+String(numberOfSteps)+"}";
}

String BuildJSONFinish(int numberOfSteps, String StopTime){
  return  "{\"numberOfSteps\": "+String(numberOfSteps)+", \"stopTime\": \""+StopTime+"\"}";
}

String PostToServer (String API, String PostData) {
  bool RecordJSON = false;
  String JSONString = "";

  // Check if connected
  Serial.println("Connecting to WebSite");
  while (0 == c.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  // send HTTP request, ends with 2 CR/LF
  Serial.println("send HTTP POST request");
  Serial.println("POST " + API +" HTTP/1.1");
  Serial.println("Host: " SITE_URL);
  Serial.println("User-Agent: Arduino/1.0");
  Serial.println("Connection: close");
  Serial.print("Content-Length: ");
  Serial.println(PostData.length());
  Serial.println();
  Serial.println(PostData);
  
  
  c.println("POST " + API +" HTTP/1.1");
  c.println("Host: " SITE_URL);
  c.println("User-Agent: Arduino/1.0");
  c.println("Connection: close");
  c.print("Content-Length: ");
  c.println(PostData.length());
  c.println();
  c.println(PostData);
  
  // Make sure we are connected, and dump the response content to Serial
  Serial.println("waiting HTTP response"); 
  while (!c.available())
  {
    delay(100);
  }
  
  while (c)
  {
    int v = c.read();
    if (v != -1)
    {
      Serial.print((char)v);
      
      if ((char)v == '{')
      // }
      { 
        RecordJSON = true;
     }
      // {
      else if ((char)v == '}'){ 
        RecordJSON = false;
        // {
        JSONString = JSONString + "} "; 
      }
      if (RecordJSON == true) {
        JSONString = JSONString + (char)v;
      }      
    }
  }
  return JSONString;
}

String GetFromServer (String API) {
  bool RecordJSON = false;
  String JSONString = "";

  // Check if connected
  Serial.println("Connecting to WebSite");
  while (0 == c.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  // send HTTP request, ends with 2 CR/LF
  Serial.println("send HTTP GET request");
  Serial.println("GET " + API +" HTTP/1.1");
  Serial.println("Host: " SITE_URL);
  Serial.println("User-Agent: Arduino/1.0");
  Serial.println("Connection: close");
  Serial.println();
  
  
  c.println("GET " + API +" HTTP/1.1");
  c.println("Host: " SITE_URL);
  c.println("User-Agent: Arduino/1.0");
  c.println("Connection: close");
  c.println();
  
  // Make sure we are connected, and dump the response content to Serial
  Serial.println("waiting HTTP response"); 
  while (!c.available())
  {
    delay(100);
  }
  
  while (c)
  {
    int v = c.read();
    if (v != -1)
    {
      Serial.print((char)v);
      if ((char)v == '{')
      // }
      { 
        RecordJSON = true;
     }
      // {
      else if ((char)v == '}'){ 
        RecordJSON = false;
        // {
        JSONString = JSONString + "} "; 
      }
      if (RecordJSON == true) {
        JSONString = JSONString + (char)v;
      }      
    }
  }
  return JSONString;
}

int GetIDfromJSON(String JSONString){
    char json[JSONString.length()];
    JSONString.toCharArray(json, JSONString.length());
    Serial.println("JSONchar: "+String(json));
    
     StaticJsonBuffer<300> jsonBuffer;
  
    JsonObject& root = jsonBuffer.parseObject(json);
  
    if (!root.success()) {
      Serial.println("parseObject() failed");
      
    }
    
    WorkOutID = root["id"];
    return WorkOutID; 
}

void GetTimeAPI() {
   String JSONString = GetFromServer("/time");
   char json[JSONString.length()];
   JSONString.toCharArray(json, JSONString.length());
   Serial.println("JSONchar: "+String(json));
    
   StaticJsonBuffer<300> jsonBuffer;
  
   JsonObject& root = jsonBuffer.parseObject(json);
  
    if (!root.success()) {
      Serial.println("parseObject() failed");
      
    }
    
   CurrentTime.year = (root["year"]);
   CurrentTime.mon = (root["month"]);
   CurrentTime.day = (root["day"]);
   CurrentTime.hour = (root["hour"]);
   CurrentTime.min = (root["minute"]);   
   CurrentTime.sec = (root["seconde"]);   
   LDateTime.setTime(&CurrentTime);
   
   TimeState = TimeKnown;
   Serial.println("Current time: "+ GetUTCTime());
          
}
