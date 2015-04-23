/*
  Web client

 This sketch connects to a website 
 using Wi-Fi functionality on MediaTek LinkIt platform.

 Change the macro WIFI_AP, WIFI_PASSWORD, WIFI_AUTH and SITE_URL accordingly.

 created 13 July 2010
 by dlf (Metodo2 srl)
 modified 31 May 2012
 by Tom Igoe
 modified 20 Aug 2014
 by MediaTek Inc.
 */

#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LDateTime.h>
#include <ArduinoJson.h>


#define WIFI_AP "BASH"
#define WIFI_PASSWORD "BASH@infocomm"
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.
#define SITE_URL "thebutlers.tk"


LWiFiClient c;
datetimeInfo CurrentTime;

boolean disconnectedMsg = false;

int ConnectToServer () {
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
String BuildJSONCreate(int Machine, String StartTime){
  return  "{\"machine\": "+String(Machine)+", \"startTime\": \""+StartTime+"\"}";
}

String BuildJSONUpdate(int numberOfSteps){
  return  "{\"numberOfSteps\": "+String(numberOfSteps)+"}";
}

String BuildJSONFinish(int numberOfSteps, String StopTime){
  return  "{\"numberOfSteps\": "+String(numberOfSteps)+", \"stopTime\": \""+StopTime+"\"}";
}

String PostToServer (String API, String PostData) {
//  char json[] = "{}";
  int TimeoutSeconds = 0;
 
  String StringToReturn = "";
  bool RecordJSON = false;
  String JSONString = "";
/*
  // Check if connected
  Serial.println("Connecting to WebSite");
  while (0 == c.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
*/
 
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
  
  
  c.println("POST "+API+" HTTP/1.1");
  c.println("Host: " SITE_URL);
  c.println("User-Agent: Arduino/1.0");
  c.println("Connection: close");
  c.print("Content-Length: ");
  c.println(PostData.length());
  c.println();
  c.println(PostData);
  

  // waiting for server response
  Serial.println("waiting HTTP response:");
  while (!c.available() && TimeoutSeconds < 10000)
  {
    int TimeoutSeconds = TimeoutSeconds + 100;
    delay(100);
    Serial.println("Milliseconds elapsed: " + TimeoutSeconds);
  }
  
  if (TimeoutSeconds >= 10000)
  {
    Serial.println("No response for 10s ");
  }
  
  
    // Make sure we are connected, and dump the response content to Serial
  while (c)
  {
    int v = c.read();
    if (v != -1)
    {
      Serial.print((char)v);
      StringToReturn = StringToReturn + (char)v;
      if ((char)v == '{'){
        RecordJSON = true;

      }
      else if ((char)v == '}'){
        
        RecordJSON = false;
        JSONString = JSONString + "} ";
      }
      if (RecordJSON == true){
        if (((char)v >= '0' && (char)v <= '9') || ((char)v >= 'A' && (char)v <= 'Z') || ((char)v >= 'a' && (char)v <= 'z') || (char)v == '.' || (char)v == ',' || (char)v == '_' || (char)v == '{'|| (char)v == '}'|| (char)v == ':'|| (char)v == '-'|| (char)v == '"') {
          JSONString = JSONString + (char)v;
        }
      }
      
      
    }
    else
    {
      Serial.println("Got all content");
    }
  }
    // Decode the mess to run JSON
    Serial.println("JSON: "+JSONString);
    
    // Find null and replace with "null"
    JSONString.replace("null", "\"null\"");
    
  if (!disconnectedMsg)
  {
    Serial.println("disconnected by server");
    disconnectedMsg = true;
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
    
    int WorkOutID = root["id"];
    return WorkOutID;
 
}

void setup()
{
  LTask.begin();
  LWiFi.begin();
  Serial.begin(9600);
  ConnectToServer();
  

}


void loop()
{
  int Steps = 0;
  String JsonReceived = PostToServer("/exercise/create", BuildJSONCreate(1, GetUTCTime()));
  int WorkOutID = GetIDfromJSON(JsonReceived);
  for (int i=0; i <= 0; i++){
    Steps = i * 10 + 10;
    PostToServer("/exercise/update/"+String(WorkOutID), BuildJSONUpdate(Steps));
    delay(5000);
  }
  PostToServer("exercise/update/"+String(WorkOutID), BuildJSONFinish(100, GetUTCTime()));
}

