/*---------------------------------------------------
                 _____
                </\|/\>
          _-_-_-_ *|* _-_-_-_
         -_-_-_-_-\@/-_-_-_-_-
         
         https://freeboard.io/board/QojxS6
         
         https://thingspeak.com/channels/75239
         
         
HTTP 1.1 Temperature & Humidity Webserver for ESP8266 
for ESP8266 adapted in the Arduino IDE

Espressif SDK Functionality in the form of a dignostics page

Server Layout

http://ipaddress/                        Web Text to LCD Input
                /diag                    SDK Functionality
                /jsread                  RAW JSON Frame
                /monitor                 Google Gauges
                /graphic                 Google Line Chart
                /graphic2                Google Line Chart of BMP180 Pressure
                /pressure                Google Gauge of BMP180 Pressure
                /table                   Datalog Table
                /yfireset                Reset the WiFi Connection
                /srestart                Reboot the system(ESP must be cold booted, this will fail directly after uploading a sketch)

Credits to Stefan Thesen and The Guy's at .......http://www.ESP8266.com
With the use of http://espressif.com/new-sdk-release

The ESP8266 Arduino Reference http://arduino.esp8266.com/versions/1.6.5-1160-gef26c5f/doc/reference.html

With this release and a 1MByte Flash with 512KB SPIFFS can hold 3328 records of 120 bytes long Approx. 390KB

Set up an account at http://smtp2go.com
Encrypt your Password & Username using https://www.base64encode.org/

This evolution is the product of the Environment Monitor...... 
environmental.monitor.log@gmail.com

See a feed from 2 DHT11's on twitter at http://twitter.com/DDTMonitor

This will read various sensors and send an Email at a time or Sensor Condition
With the Data from the Sensors presented via Web Gauges, Graphs & Tables.....

Setup = 2 * DHT11 + BMP180 and an ESP8266-07 with 1MB Flash

There are various pins used to load boot code so if you change pins and it does not boot this is WHY!!

I2C  GPios 4 & 5
DHT  GPio  12 & 13


To Index to a data point, if each record is 64 bytes, there are place holders to count in the string record
to select an individual data point.
The SPIFFS Record selction.......to allow reading/modifying a value....... 

File dataFile1 = SPIFFS.open("/humidlog.CSV", "r"); //  a = append/modify, r = read or create w = write
  if(!dataFile1.seek(0, SeekSet))   //  Other Commands are seekCur or seekEnd
  {
    Serial.println(F("Rewind fail"));
    dataFile1.close();
  }
 stringholder = dataFile1.readStringUntil("[");  //Start of Record idexed to with file offset currently=0
__________________________________________________________________________________________________
......Use 5v to 3v Level Shifters as 5V devices like PIR and HC-SR04 can damage your device.......
--------------------------------------------------------------------------------------------------
 ___/'¬_         /\
{_-_ ^  \_______//
     /           \
     \ _  _____  /
     || ||     |||
     || ||     |||  
---------------------------------------------------*/
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "DHT.h"
#include <Wire.h>
#include <SFE_BMP180.h>
SFE_BMP180 pressure;               // i2c Address 77h
// WiFi connection
const char* hostts = "api.thingspeak.com";
const char* thingspeak_key = "thingspeakWriteKey"; // Set this to your own Write Key for API
const char* ssid = "Your-SSID";
const char* password = "Your-WiFiPassword";
String toaddres = "RCPT To: <example@gmail.com>";// Where your Last 24Hr Log has to be emailed To.......Time set below....
String frmaddrs = "MAIL From: <expmleSetAtSMTP2GO@gmail.com>";// Where your Log has to be emailed From.......
String b64Usernm = "Enc-Username";// B64 Encrypted smtp2go Username
String b64Passwd = "Enc-Password";// B64 Encrypted smtp2go Password
String emailbutton = "off";
String lastdata;
String lastpres;
String emailtime;
String DsTable;             //Discarded parts if the Strings......
String tmtxt1;              //Strings from the Web....
String tmtxt2;
String stheDate;            //System Start Time...
String theDate;             // The current TimeStamp from Google....
boolean got_text = false;   //Print the Text to LCD if true......
boolean barom = false;      //Print the Barometric Pressure data to Line Chart.....
double baseline;            // BMP180 baseline pressure
double T,P,p0,a;            // BMP180 Data
unsigned long ulSecs2000_timer=0;                    // Timer for loop
unsigned long ulMeasCount=0;                         // values already measured
unsigned long ulMeasDelta_ms;                        // distance to next meas time
unsigned long ulNextMeas_ms;                         // next meas time
float pfTemp,pfHum,pfTemp1,pfHum1,pfDew,pfDew1,pfVcC;    // floats for Environment/System measurements stored in SPIFFS
float pfTempold,pfHumold,pfTempold1,pfHumold1;          // floats for LAST measurements
unsigned long ulReqcount;                            // how often has a valid page been requested
unsigned long ulJReqcount;                           // how often has a JSON frame been requested 
unsigned long ulReconncount;                         // how often did we connect to WiFi
unsigned long Dfsize;                                // size of the datalog file
//NTP
char tDigit; 
String TiMeS,TMnow,MyNtP,duration1;
int tIMeh,tIMem,tIMes,newt,newt1,inChar;
// Create an instance of the server on Port 80-89 = Sensor 1 - 10
WiFiServer server(80);
WiFiClient client;
char popyahoo[] = "smtpcorp.com";  // set email service
int port = 2525;
////////////////////////////////////
// DHT21 / AMS2301 / DHT11 /DHT22 //
////////////////////////////////////
#define DHTPIN 13
#define DHTPIN1 12
ADC_MODE(ADC_VCC);
// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11 
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
// Declare each sensor
DHT dht(DHTPIN, DHTTYPE); 
DHT dht1(DHTPIN1, DHTTYPE); 
// needed to avoid link error on ram check
extern "C" 
{
#include "user_interface.h"
}
void wsysreset()
{
  //Perform challenge to user before getting here to reboot !!!
  //Make an attempt to send mail or other backup of log files then reboot
    ESP.restart();
}

/////////////////////
// the setup routine
/////////////////////
void setup() 
{
  delay(50);
  pfVcC =  ESP.getVcc();   // Only use this if Nothing connected to TOUT / ADC
  delay(150);
//  wire.begin();
  dht.begin();
  dht1.begin();
  // setup globals
  ulReqcount=0; 
  ulReconncount=0;
  // initialize the SPIFFS
  if (!SPIFFS.begin()) {
       while(1);
       }
  // clear any old data....
  if (SPIFFS.exists("/humidlog.CSV")){ SPIFFS.remove("/humidlog.CSV");}  
//  if (SPIFFS.exists("/humidlog.CSV")){ SPIFFS.rename("/humidlog.CSV", "/humidlog.old");}
       if (pressure.begin())
       {
       }
       else
  {
    // Oops, something went wrong, this is usually a connection problem,
    // see the comments at the top of this sketch for the proper connections.
    while(1); // Pause forever.
  }
   baseline = getPressure();

// inital connect
  WiFi.mode(WIFI_STA);
  delay(500);
  WiFiStart();
  server.begin();
  delay(2000);
  ulMeasDelta_ms = ( (unsigned long) 60 * 1000);  // Sample Interval 60 Seconds, this can be placed above in the declarations....
  ulNextMeas_ms = millis()+ulMeasDelta_ms;
  emailtime=theDate.substring(11,19);
    pfHum = dht.readHumidity();
    pfTemp = dht.readTemperature();
    pfHum1 = dht1.readHumidity();
    pfTemp1 = dht1.readTemperature();
   float a = 17.67;
   float b = 243.5;
   float alpha = (a * pfTemp1)/(b + pfTemp1) + log(pfHum1/100);
   pfDew1 = (b * alpha)/(a - alpha);
   a = 17.67;
   b = 243.5;
   alpha = (a * pfTemp)/(b + pfTemp) + log(pfHum/100);
   pfDew = (b * alpha)/(a - alpha); 
}
///////////////////
// (re-)start WiFi
///////////////////
void WiFiStart()
{
  ulReconncount++;
  delay(1000);
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }
  // Start the server
  server.begin();
  ///////////////////////////////
  // connect to NTP and get time
  ///////////////////////////////
  WiFiClient client;
  while (!!!client.connect("google.com", 80)) {
  }
  client.print("HEAD / HTTP/1.1\r\n\r\n");
  while(!!!client.available()) {
     yield();
  }
  while(client.available()){
  client.readStringUntil('\n');
  String theDate1 = client.readStringUntil('\r'); //Date: Tue, 10 Nov 2015 19:55:38 GMT
    if (theDate1.startsWith("Date:")) {
      TiMeS=theDate1;
      theDate = theDate1.substring(11,17);
          client.flush(); 
          client.stop();             
          }
  inChar = TiMeS.charAt(30);
  newt=inChar-48;             // Convert Numerical Char to int.....
  inChar = TiMeS.charAt(29);
  newt1=inChar-48; 
  tIMes=(newt1*10)+newt;
  
  inChar = TiMeS.charAt(27);
  newt=inChar-48;
  inChar = TiMeS.charAt(26);
  newt1=inChar-48; 
  tIMem=(newt1*10)+newt;
  
  inChar = TiMeS.charAt(24);
  newt=inChar-48;
  inChar = TiMeS.charAt(23);
  newt1=inChar-48; 
  tIMeh=(newt1*10)+newt;
  MyNtP=" ";
  MyNtP+=theDate;
  MyNtP+=" ";
  if (tIMeh<10){MyNtP+="0";}
  MyNtP+=tIMeh;
  MyNtP+=":";
  if (tIMem<10){MyNtP+="0";}
  MyNtP+=tIMem;
  MyNtP+=":";
  if (tIMes<10){MyNtP+="0";}
  MyNtP+=tIMes;
  MyNtP+=" GMT";
  String duration1 = " ";
   duration1 = "";
   int hr,mn,st;
   st = millis() / 1000;
   mn = st / 60;
   hr = st / 3600;
   st = st - mn * 60;
   mn = mn - hr * 60;
   if (hr<10) {duration1 += ("0");}
   duration1 += (hr);
   duration1 += (":");
   if (mn<10) {duration1 += ("0");}
   duration1 += (mn);
   duration1 += (":");
   if (st<10) {duration1 += ("0");}
   duration1 += (st);  
   int HRsa=hr+tIMeh;
   int MNsa=mn+tIMem;
   int SCsa=st+tIMes;
   if (SCsa>59){SCsa-=60;MNsa+=1;}
   if (MNsa>59){MNsa-=60;HRsa+=1;}
   if (HRsa>23){HRsa-=24;}
    TMnow="";
    if (HRsa<10){TMnow += "0";}
    TMnow += HRsa;
    TMnow += ":";
    if (MNsa<10){TMnow += "0";}
    TMnow += MNsa;
    TMnow += ":";
    if (SCsa<10){TMnow += "0";}
    TMnow += SCsa; 
 } 
}

double getPressure()
{
  char status;


  // You must first get a temperature measurement to perform a pressure reading.
  
  // Start a temperature measurement:
  // If request is successful, the number of ms to wait is returned.
  // If request is unsuccessful, 0 is returned.

  status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:

    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Use '&T' to provide the address of T to the function.
    // Function returns 1 if successful, 0 if failure.

    status = pressure.getTemperature(T);
    if (status != 0)
    {
      // Start a pressure measurement:
      // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
      // If request is successful, the number of ms to wait is returned.
      // If request is unsuccessful, 0 is returned.

      status = pressure.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        // Retrieve the completed pressure measurement:
        // Note that the measurement is stored in the variable P.
        // Use '&P' to provide the address of P.
        // Note also that the function requires the previous temperature measurement (T).
        // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
        // Function returns 1 if successful, 0 if failure.

        status = pressure.getPressure(P,T);
        if (status != 0)
        {
          return(P);
        }
      }
    }
  }
}
/////////////////////////////////////
// make html table for measured data
/////////////////////////////////////
unsigned long MakeTable (WiFiClient *pclient, bool bStream)
{
  unsigned long ulLength=0;
  
  // here we build a big table.
  // we cannot store this in a string as this will blow the memory   
  // thus we count first to get the number of bytes and later on 
  // we stream this out
  if (ulMeasCount==0) 
  {
    String sTable = "No data available yet.<BR>";
    if (bStream)
    {
      pclient->print(sTable);
    }
    ulLength+=sTable.length();
  }
  else
  { 
   File logF = SPIFFS.open("/humidlog.CSV", "r");
    if (!logF) {
      while(1);
    }
    String sTable;
    String DsTable="";
    sTable = "<table style=\"width:100%\"><tr><th>Time / GMT</th><th>Int Temperature &deg;C</th><th>Int Humidity &#037;</th><th>Ext Temperature &deg;C</th><th>Ext Humidity &#037;</th><th>System Vcc</th><th>BMP Temp</th><th>Pressure MilliBar</th></tr>";
    sTable += "<style>table, th, td {border: 2px solid black; border-collapse: collapse;} th, td {padding: 5px;} th {text-align: left;}</style>";
     DsTable = logF.readStringUntil('\'');
while(logF.available())
    {
      sTable += "<tr><td>";
      sTable += logF.readStringUntil('\''); 
     DsTable += logF.readStringUntil(',');
      sTable += "</td><td>";
      sTable += logF.readStringUntil(',');
      sTable += "</td><td>";
      sTable += logF.readStringUntil(',');
      sTable += "</td><td>";
      sTable += logF.readStringUntil(',');
      sTable += "</td><td>";
      sTable += logF.readStringUntil(',');
      sTable += "</td><td>";
      sTable += logF.readStringUntil(',');
      sTable += "</td><td>";
      sTable += logF.readStringUntil(']');
      sTable += "</td><td>";
      sTable += logF.readStringUntil(',');      
      sTable += "</td></tr>";
     DsTable = logF.readStringUntil('\'');
      // play out in chunks of 1k
      if(sTable.length()>1024)
      {
        if(bStream)
        {
          pclient->print(sTable);
          //pclient->write(sTable.c_str(),sTable.length());
        }
        ulLength+=sTable.length();
        sTable="";
      }
    }
    
    // remaining chunk
    sTable+="</table>";
    ulLength+=sTable.length();
    if(bStream)
    {
      pclient->print(sTable);
      //pclient->write(sTable.c_str(),sTable.length());
    }   
  }
  
  return(ulLength);
}


////////////////////////////////////////////////////
// make google chart object table for measured data
////////////////////////////////////////////////////
unsigned long MakeList (WiFiClient *pclient, bool bStream)
{
  unsigned long ulLength=0;
  
  // here we build a big list.
  // we cannot store this in a string as this will blow the memory   
  // thus we count first to get the number of bytes and later on 
  // we stream this out
  if (ulMeasCount>0) 
  { 
   File logF = SPIFFS.open("/humidlog.CSV", "r");
    if (!logF) {
      while(1);
    }
    String sTable="";
     DsTable = logF.readStringUntil('\''); 
    while (logF.available())
    { // result shall be ['18:24:08 - 21.5.2015',21.10,49.00],
      sTable += "['";
      sTable += logF.readStringUntil('\'');
      sTable += "',";
  if (barom == true){
      DsTable += logF.readStringUntil(']');// Pressure Measurements are a different Scale......hence simple separation....   
      sTable += logF.readStringUntil(',');
      }
 else{
     DsTable = logF.readStringUntil(','); 
      sTable += logF.readStringUntil(']');// Pressure Measurements are a different Scale......hence simple separation....   
      }
      sTable += "],\n";
      DsTable = logF.readStringUntil('\''); 
      // play out in chunks of 1k
      if(sTable.length()>1024)
      {
        if(bStream)
        {
          pclient->print(sTable);
          //pclient->write(sTable.c_str(),sTable.length());
        }
        ulLength+=sTable.length();
        sTable="";
      }
    }
          
    
    // remaining chunk
    if(bStream)
    {
      pclient->print(sTable);
      //pclient->write(sTable.c_str(),sTable.length());
    } 
    ulLength+=sTable.length();  
  }
  
  return(ulLength);
}


//////////////////////////
// create HTTP 1.1 header
//////////////////////////
String MakeHTTPHeader(unsigned long ulLength)
{
  String sHeader;
  
  sHeader  = F("HTTP/1.1 200 OK\r\nContent-Length: ");
  sHeader += ulLength;
  sHeader += F("\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
  
  return(sHeader);
}


////////////////////
// make html footer
////////////////////
String MakeHTTPFooter()
{
  String sResponse;
  
  sResponse  = F(" "); 
  sResponse += F("<BR>environmental.monitor.log@gmail.com<BR><FONT SIZE=-2>ESP8266 With DHT11 & BMP180 Sensors<BR><FONT SIZE=-2>Compiled Using ver. 2.0.0-rc2, built December, 2015</body></html>");
  return(sResponse);
}

/////////////
// main loop
/////////////
void loop() 
{
  ///////////////////
  // do data logging
  ///////////////////
  if (millis()>=ulNextMeas_ms) 
  {   
    ulNextMeas_ms = millis()+ulMeasDelta_ms;
     delay(500); 
    String duration1 = " ";
int hr,mn,st;
   st = millis() / 1000;
   mn = st / 60;
   hr = st / 3600;
   st = st - mn * 60;
   mn = mn - hr * 60;
   if (hr<10) {duration1 += ("0");}
   duration1 += (hr);
   duration1 += (":");
   if (mn<10) {duration1 += ("0");}
   duration1 += (mn);
   duration1 += (":");
   if (st<10) {duration1 += ("0");}
   duration1 += (st);  
int HRsa=hr+tIMeh;
int MNsa=mn+tIMem;
int SCsa=st+tIMes;
if (SCsa>59){SCsa-=60;MNsa+=1;}
if (MNsa>59){MNsa-=60;HRsa+=1;}
if (HRsa>23){HRsa-=24;}
    TMnow="";
    if (HRsa<10){TMnow += "0";}
    TMnow += HRsa;
    TMnow += ":";
    if (MNsa<10){TMnow += "0";}
    TMnow += MNsa;
    TMnow += ":";
    if (SCsa<10){TMnow += "0";}
    TMnow += SCsa; 
String  theDatet = theDate;
    theDatet.replace("2015 ", "- ");
    pfTemp = dht.readTemperature();
    pfHum = dht.readHumidity();   
     delay(2500);
    pfTemp = dht.readTemperature();
    pfHum = dht.readHumidity();    
    pfHum1 = dht1.readHumidity();
    pfTemp1 = dht1.readTemperature();
   float a = 17.67;
   float b = 243.5;
   float alpha = (a * pfTemp1)/(b + pfTemp1) + log(pfHum1/100);
   pfDew1 = (b * alpha)/(a - alpha);
   a = 17.67;
   b = 243.5;
   alpha = (a * pfTemp)/(b + pfTemp) + log(pfHum/100);
   pfDew = (b * alpha)/(a - alpha);
    lastdata="";
    lastdata += pfTemp;
    lastdata += ",hum=";
    lastdata += pfHum;
    lastdata += ",temp1=";
    lastdata += pfTemp1;
    lastdata += ",hum1=";
    lastdata += pfHum1; 
 String logdata = "[";
  logdata += "'";  
  logdata += theDatet+TMnow;
  logdata += "'";
  logdata += ","; 
  logdata += pfTemp;
if(pfTemp<10){logdata += "0";}   // Keep 64 Byte Record Size  
  logdata += ",";
  logdata += pfHum;  
  logdata += ",";
  logdata += pfTemp1; 
if(pfTemp1<10){logdata += "0";}   // Keep 64 Byte Record Size  
  logdata += ",";
  logdata += pfHum1;
  logdata += ",";
  logdata += pfVcC/1000;
  logdata += ",";
  logdata += T;   
  logdata += "]";
  logdata += P;
if(P<1000){logdata += "0";}   // Keep 64 Byte Record Size  
  logdata += ","; 
 lastpres = P; 
File logF = SPIFFS.open("/humidlog.CSV", "a");      
  if (logF) { 
    logF.print(logdata);
    logF.close();
    ulMeasCount++;   
      }
    else {
      while(1);
    }  
  }

if (millis()==(ulNextMeas_ms-(ulMeasDelta_ms/2))){
  if (got_text == true){
   if (tmtxt1!=""){     
        }
        else{  // Empty String returned CLEAR SCREEN
        got_text == false;
        }     
        
      }
  delay(50);
  pfVcC =  ESP.getVcc();   // Only use this if Nothing connected to TOUT / ADC
  delay(150);   
    char status;
  status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:

    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Use '&T' to provide the address of T to the function.
    // Function returns 1 if successful, 0 if failure.
  }
    status = pressure.getTemperature(T);
          status = pressure.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        // Retrieve the completed pressure measurement:
        // Note that the measurement is stored in the variable P.           http://bbc.in/1HIlwwK
        // Use '&P' to provide the address of P.
        // Note also that the function requires the previous temperature measurement (T).
        // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
        // Function returns 1 if successful, 0 if failure.
      }
        status = pressure.getPressure(P,T);
        
   // Use WiFiClient class to create Thingspeak Post
  WiFiClient client;
  if (!client.connect(hostts, 80)) {
    return;
  }
  String url = "/update?key=";
  url += thingspeak_key;
  url += "&field1=";
  url += pfTemp;
  url += "&field2=";
  url += pfHum;
  url += "&field3=";
  url += pfDew;
  url += "&field4=";
  url += pfTemp1;
  url += "&field5=";
  url += pfHum1;
  url += "&field6=";
  url += pfDew1;
  url += "&field7=";
  url += P;
  url += "&field8=";
  url += T;  
// This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + hostts + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(50);
  client.stop();     
        
        
   

//  Use this instead of ESP.getVcc() for modules with LDR Fitted, Alter Display Text from Volts!!!
//  This is the Start of the LDR Sensor Measurement Percent/10 Saturation of LDR
//  Keep the Scale relavant for Line Graph or select Different Axis.....
/*
delay(25);
int pfVcC = 10-(constrain(map((analogRead(A0)), 40, 900, 1, 10), 1, 10)) ;
delay(25);
//
//  This is the End of the LDR Sensor Measurement
//
  

*/
 emailtime = theDate.substring(12,17);
  if (TMnow.startsWith("00:15")) {// Set the time for the Log to be emailed
     if(sendEmail()) {
     wsysreset();
      } 
      else {
      while(1);
      }
    }
  
  }
  //////////////////////////////
  // check if WLAN is connected
  //////////////////////////////
  
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFiStart();
  }
  
  ///////////////////////////////////
  // Check if a client has connected
  ///////////////////////////////////
  WiFiClient client = server.available();
  if (!client) 
  {
    return;
  }
  
  // Wait until the client sends some data
  unsigned long ultimeout = millis()+250;
  while(!client.available() && (millis()<ultimeout) )
  {
    delay(1);
  }
  if(millis()>ultimeout) 
  { 
    return; 
  }
  
  /////////////////////////////////////
  // Read the first line of the request
  /////////////////////////////////////
  String sRequest = client.readStringUntil('\r');
  client.flush();
  
  // stop client, if request is empty
  if(sRequest=="")
  {
    client.stop();
    return;
  }
  
  // get path; end of path is either space or ?
  // Syntax is e.g. GET /?show=1234 HTTP/1.1
  String sPath="",sParam="", sCmd="";
  String sGetstart="GET ";
  int iStart,iEndSpace,iEndQuest;
  iStart = sRequest.indexOf(sGetstart);
  if (iStart>=0)
  {
    iStart+=+sGetstart.length();
    iEndSpace = sRequest.indexOf(" ",iStart);
    iEndQuest = sRequest.indexOf("?",iStart);
    
    // are there parameters?
    if(iEndSpace>0)
    {
      if(iEndQuest>0)
      {
        // there are parameters
        sPath  = sRequest.substring(iStart,iEndQuest);
        sParam = sRequest.substring(iEndQuest,iEndSpace);
      }
      else
      {
        // NO parameters
        sPath  = sRequest.substring(iStart,iEndSpace);
      }
    }
  }
 
  
  ///////////////////////////
  // format the html response
  ///////////////////////////
  String sResponse,sResponse2,sHeader;
  
  /////////////////////////////////
  //  Update LCD with text from Web
  /////////////////////////////////
  //_________________________________________________________________
  // /ajax_inputs&L1=123456789&L2=abcdefghi&nocache=968337.7823963541
  //  1-9 on Line 1 and a - i on line 2
  //¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬
if (sPath.startsWith("/ajax_inputs&L1="))
   {
     ulReqcount++;
     tmtxt1="";
     tmtxt2="";
    got_text = true;                    // print the received text to the LCD if found
File in = SPIFFS.open("/temp.txt", "w");      
  if (in) { 
    in.print(sPath);
    in.close();
      }
    else {
      while(1);
    }
     in = SPIFFS.open("/temp.txt", "r"); 
     in.setTimeout(0);
String Dtmtxt = in.readStringUntil('=');
       tmtxt1 += in.readStringUntil('&');
       Dtmtxt = in.readStringUntil('=');
       tmtxt2 += in.readStringUntil('&');
     in.close();
       tmtxt1.replace("%20", " ");
       tmtxt2.replace("%20", " ");
   }
  //////////////////////////////////
  // format the html page for gauges
  //////////////////////////////////
 if(sPath=="/monitor") 
  {
    ulReqcount++;
    sResponse  = F("<html>\n<head>\n<title>Environment Monitor</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['gauge']}]}\"></script>\n<script type=\"text/javascript\">\nvar temp=");
    sResponse += lastdata;    
    sResponse += F(";\ngoogle.load('visualization', '1', {packages: ['gauge']});google.setOnLoadCallback(drawgaugetemp);google.setOnLoadCallback(drawgaugehum);google.setOnLoadCallback(drawgaugetemp1);google.setOnLoadCallback(drawgaugehum1);\nvar gaugetempOptions = {min: -10, max: 40, yellowFrom: -10, yellowTo: 16,redFrom: 28, redTo: 40, minorTicks: 10, majorTicks: ['-10','0','10','20','30','40']};\n");
    sResponse += F("var gaugehumOptions = {min: 0, max: 100, yellowFrom: 0, yellowTo: 25, redFrom: 70, redTo: 100, minorTicks: 10, majorTicks: ['0','10','20','30','40','50','60','70','80','90','100']};\nvar gaugehum1Options = {min: 0, max: 100, yellowFrom: 0, yellowTo: 25, redFrom: 70, redTo: 100, minorTicks: 10, majorTicks: ['0','10','20','30','40','50','60','70','80','90','100']};\n");
    sResponse += F("var gaugetemp1Options = {min: -10, max: 40, yellowFrom: -10, yellowTo: 16,redFrom: 28, redTo: 40, minorTicks: 10, majorTicks: ['-10','0','10','20','30','40']};\nvar gaugetemp,gaugehum,gaugetemp1,gaugehum1;\nfunction drawgaugetemp() {\ngaugetempData = new google.visualization.DataTable();\n");  
    sResponse += F("gaugetempData.addColumn('number', 'Int \260C');\ngaugetempData.addRows(1);\ngaugetempData.setCell(0, 0, temp);\ngaugetemp = new google.visualization.Gauge(document.getElementById('gaugetemp_div'));\ngaugetemp.draw(gaugetempData, gaugetempOptions);\n}\n\n");
    sResponse += F("function drawgaugehum() {\ngaugehumData = new google.visualization.DataTable();\ngaugehumData.addColumn('number', 'Int %');\ngaugehumData.addRows(1);\ngaugehumData.setCell(0, 0, hum);\ngaugehum = new google.visualization.Gauge(document.getElementById('gaugehum_div'));\ngaugehum.draw(gaugehumData, gaugehumOptions);\n}\n");
    sResponse += F("\n\nfunction drawgaugetemp1() {\ngaugetemp1Data = new google.visualization.DataTable();\n");
    sResponse += F("gaugetemp1Data.addColumn('number', 'Ext \260C');\ngaugetemp1Data.addRows(1);\ngaugetemp1Data.setCell(0, 0, temp1);\ngaugetemp1 = new google.visualization.Gauge(document.getElementById('gaugetemp1_div'));\ngaugetemp1.draw(gaugetemp1Data, gaugetemp1Options);\n}\n\n");
    sResponse += F("function drawgaugehum1() {\ngaugehum1Data = new google.visualization.DataTable();\ngaugehum1Data.addColumn('number', 'Ext %');\ngaugehum1Data.addRows(1);\ngaugehum1Data.setCell(0, 0, hum1);\ngaugehum1 = new google.visualization.Gauge(document.getElementById('gaugehum1_div'));\ngaugehum1.draw(gaugehum1Data, gaugehum1Options);\n}\n");
    sResponse += F("</script>\n</head>\n<body>\n<font color=\"#000000\"><body bgcolor=\"#a0dFfe\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>Environment Monitor</h1><BR><BR><FONT SIZE=+1>Last Sample Taken ");
    sResponse += TMnow;

    
    sResponse2 = F("GMT<BR>\n<div id=\"gaugetemp_div\" style=\"float:left; width:200px; height: 200px;\"></div> \n<div id=\"gaugehum_div\" style=\"float:left; width:200px; height: 200px;\"></div>\n<div style=\"clear:both;\"></div>\n<div id=\"gaugetemp1_div\" style=\"float:left; width:200px; height: 200px;\"></div> \n<div id=\"gaugehum1_div\" style=\"float:left; width:200px; height: 200px;\">");
    sResponse2 += F("</div>\n<div style=\"clear:both;\"></div><p><BR><a href=\"/graphic2\">Pressure Graph Page</a><BR><a href=\"/pressure\">Pressure Gauge Page</a><BR><a href=\"/graphic\">Sensor Graph Page</a><BR><a href=\"/table\">Sensor Datalog Page</a><BR><a href=\"/diag\">Diagnostics Information Page</a><BR><a href=\"/\">Monitor LCD Text Input Page</a></p>");
   sResponse2 += MakeHTTPFooter().c_str();
    
    // Send the response to the client 
    client.print(MakeHTTPHeader(sResponse.length()+sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }
    else if(sPath=="/jsread")
  {
    duration1="";
    int hr,mn,st;
   st = millis() / 1000;
   mn = st / 60;
   hr = st / 3600;
   st = st - mn * 60;
   mn = mn - hr * 60;
   if (hr<10) {duration1 += ("0");}
   duration1 += (hr);
   duration1 += (":");
   if (mn<10) {duration1 += ("0");}
   duration1 += (mn);
   duration1 += (":");
   if (st<10) {duration1 += ("0");}
   duration1 += (st);  
int HRsa=hr+tIMeh;
int MNsa=mn+tIMem;
int SCsa=st+tIMes;
if (SCsa>59){SCsa-=60;MNsa+=1;}
if (MNsa>59){MNsa-=60;HRsa+=1;}
if (HRsa>23){HRsa-=24;}
    TMnow="";
    if (HRsa<10){TMnow += "0";}
    TMnow += HRsa;
    TMnow += ":";
    if (MNsa<10){TMnow += "0";}
    TMnow += MNsa;
    TMnow += ":";
    if (SCsa<10){TMnow += "0";}
    TMnow += SCsa; 
     ulJReqcount++;
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
     sResponse  = ("{\n\"temperature\": [\n");
     sResponse += pfTemp;
     sResponse += ("\n],\n\"humidity\": [\n");
     sResponse += pfHum;
     sResponse += ("\n],\n\"dewpoint\": [\n");
     sResponse += pfDew;
     sResponse += ("\n],\"temperature1\": [\n");
     sResponse += pfTemp1;
     sResponse += ("\n],\n\"humidity1\": [\n");
     sResponse += pfHum1;
     sResponse += ("\n],\n\"dewpoint1\": [\n");
     sResponse += pfDew1;
     sResponse += ("\n],\n\"bmtemp\": [\n");
     sResponse += T;
     sResponse += ("\n],\n\"pressur\": [\n");
     sResponse += P;
     sResponse += ("\n],\n\"Systemv\": [\n");
     sResponse += pfVcC/1000, 3;
     sResponse += ("\n],\n\"Jsreqs\": [\n");
     sResponse += ulJReqcount;
     sResponse += ("\n],\n\"Uptime\": [\n\"");
     sResponse += duration1;
     sResponse += ("\"\n],\n\"TiMestamp\": [\n\"");
     sResponse += TMnow;
     sResponse += ("\"\n]\n}");
    // Send the JSON response to the client 
    client.println(sResponse);  
   }
  else if(sPath=="/")
  {
    ulReqcount++;
    sResponse  = F("<html>\n<head>\n<title>Environment Monitor Text to LCD</title>\n<script>\n\n");    
    sResponse += F("strLine1 = \"\";\nstrLine2 = \"\";\n\nfunction SendText()\n{\nnocache = \"&nocache=\" + Math.random() * 1000000;\nvar request = new XMLHttpRequest();\n\nstrLine1 = \"&L1=\" + document.getElementById(\"txt_form\").line1LCD.value;\nstrLine2 = \"&L2=\" + document.getElementById(\"txt_form\").line2LCD.value;\n\n");
    sResponse += F("request.open(\"GET\", \"ajax_inputs\" + strLine1 + strLine2 + nocache, true);\nrequest.send(null);\n}\n</script>\n<body>\n<h1>Environment Monitor<BR>LCD Text Input</h1>\n");
    sResponse += F("<BR><a href=\"/graphic2\">Pressure Graph Page</a><BR><a href=\"/pressure\">Pressure Gauge Page</a><BR><a href=\"/monitor\">Sensor Gauge Page</a><BR><a href=\"/graphic\">Sensor Graph Page</a><BR><a href=\"/table\">Sensor Datalog Page</a><BR><a href=\"/diag\">Diagnostics Information Page</a><BR>\n");  
    sResponse += F("<body onload=\"GetESP8266IO()\">\n<h1><FONT SIZE=-1>Enter text to send to ESP8266 LCD:</h1>\n<form id=\"txt_form\" name=\"frmText\">\n<label>Line 1: <input type=\"text\" name=\"line1LCD\" size=\"16\" maxlength=\"16\" /></label><br /><br />\n<label>Line 2: <input type=\"text\" name=\"line2LCD\" size=\"16\" maxlength=\"16\" /></label>\n");
    sResponse += F("</form>\n<br />\n<input type=\"submit\" value=\"Send Text\" onclick=\"SendText()\" />\n\n");    
    sResponse2 = F("<font color=\"#000000\"><body bgcolor=\"#a0dFfe\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><BR>LCD Text Input<BR>\n");
    sResponse2 += F("</div>\n<div style=\"clear:both;\"></div><p>");
    sResponse2 += MakeHTTPFooter().c_str();
        // Send the response to the client 
    client.print(MakeHTTPHeader(sResponse.length()+sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
}
    else if(sPath=="/pressure")
  {
    
    ulReqcount++;
    sResponse  = F("<html>\n<head>\n<title>Environment Monitor</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['gauge']}]}\"></script>\n<script type=\"text/javascript\">\nvar temp=");
    sResponse += lastpres;
    sResponse += F(";\ngoogle.load('visualization', '1', {packages: ['gauge']});google.setOnLoadCallback(drawgaugetemp);\nvar gaugetempOptions = {min: 400, max: 1200, yellowFrom: 850, yellowTo: 950, greenFrom: 950, greenTo: 1050, yellowFrom: 1050, yellowTo: 1200, minorTicks: 10, majorTicks: ['400','500','600','700','800','900','1000','1100','1200']};\n");
    sResponse += F("\nvar gaugetemp,gaugehum;\n\nfunction drawgaugetemp() {\ngaugetempData = new google.visualization.DataTable();\n");
    sResponse += F("gaugetempData.addColumn('number', ' mbar');\ngaugetempData.addRows(1);\ngaugetempData.setCell(0, 0, temp);\ngaugetemp = new google.visualization.Gauge(document.getElementById('gaugetemp_div'));\ngaugetemp.draw(gaugetempData, gaugetempOptions);\n}\n\n");
    sResponse += F("</script>\n</head>\n<body>\n<font color=\"#000000\"><body bgcolor=\"#a0dFfe\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1><h1>Environment Monitor</h1><BR><BR><FONT SIZE=+1>Last Sample Taken ");
    sResponse += TMnow;

    
    sResponse2 = F("GMT<BR>\n<div id=\"gaugetemp_div\" style=\"float:left; width:300px; height: 300px;\">\n");
    sResponse2 += F("</div>\n<div style=\"clear:both;\"></div><p><BR><a href=\"/graphic2\">Pressure Graph Page</a><BR><a href=\"/monitor\">Sensor Gauge Page</a><BR><a href=\"/graphic\">Sensor Graph Page</a><BR><a href=\"/table\">Sensor Datalog Page</a><BR><a href=\"/diag\">Diagnostics Information Page</a><BR><a href=\"/\">Monitor LCD Text Input Page</a></p>");
    sResponse2 += MakeHTTPFooter().c_str();
    
    // Send the response to the client 
    client.print(MakeHTTPHeader(sResponse.length()+sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }
  else if(sPath=="/table")
  ////////////////////////////////////
  // format the html page for /tabelle
  ////////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client,false); // get size of table first
    
    sResponse  = F("<html><head><title>Environment Monitor</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#a0dFfe\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>Environment Monitor</h1>");
    sResponse += F("<FONT SIZE=+1><BR><a href=\"/monitor\">Sensor Gauges Page</a><BR><a href=\"/graphic2\">Pressure Graph Page</a><BR><a href=\"/pressure\">Pressure Gauge Page</a>");
    sResponse += F("<BR><a href=\"/graphic\">Sensor Graph Page</a><BR><a href=\"/diag\">Diagnostics Information Page</a><BR><a href=\"/\">Monitor LCD Text Input Page</a><BR><BR>Sample Interval 60 Seconds<BR>");
//    sResponse += ulMeasDelta_ms/1000;
//    sResponse += F(" Seconds<BR>");
    // here the big table will follow later - but let us prepare the end first
      
    // part 2 of response - after the big table
    sResponse2 = MakeHTTPFooter().c_str();
    
    // Send the response to the client - delete strings after use to keep mem low
    client.print(MakeHTTPHeader(sResponse.length()+sResponse2.length()+ulSizeList).c_str()); 
    client.print(sResponse); sResponse="";
    MakeTable(&client,true);
    client.print(sResponse2);
  }
  else if(sPath=="/graphic")
  ///////////////////////////////////
  // format the html page for /grafik
  ///////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeList(&client,false); // get size of list first

    sResponse  = F("<html>\n<head>\n<title>Environment Monitor</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['corechart']}]}\"></script>\n");
    sResponse += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);\nfunction drawChart() {var data = google.visualization.arrayToDataTable([\n['Time / GMT', 'InTmp', 'InHum', 'ExTmp', 'ExHum', 'SySVcc', 'SySTemp'],\n");    
    // here the big list will follow later - but let us prepare the end first
      
    // part 2 of response - after the big list
    sResponse2  = F("]);\nvar options = {title: 'Environment',vAxes:{0:{viewWindowMode:'explicit',gridlines:{color:'black'},format:\"##.##\260C\"},1: {gridlines:{color:'transparent'},format:\"##,##%\"},},series:{0:{targetAxisIndex:0},1:{targetAxisIndex:1},2:{targetAxisIndex:0},3:{targetAxisIndex:1},4:{targetAxisIndex:0},5:{targetAxisIndex:0},},curveType:'function',legend:{ position: 'bottom'}};");
    sResponse2 += F("var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));chart.draw(data, options);}\n</script>\n</head>\n");
    sResponse2 += F("<body>\n<font color=\"#000000\"><body bgcolor=\"#a0dFfe\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>Environment Monitor</h1><BR><a href=\"/graphic2\">Pressure Graph Page</a><BR><a href=\"/pressure\">Pressure Gauge Page</a><BR><a href=\"/monitor\">Sensor Gauges Page</a><BR><a href=\"/table\">Sensor Datalog Page</a>");
    sResponse2 += F("<BR><a href=\"/diag\">Diagnostics Information Page</a><BR><a href=\"/\">Monitor LCD Text Input Page</a><BR><BR>\n<div id=\"curve_chart\" style=\"width: 600px; height: 400px\"></div>");
    sResponse2 += MakeHTTPFooter().c_str();
    
    // Send the response to the client - delete strings after use to keep mem low
    client.print(MakeHTTPHeader(sResponse.length()+sResponse2.length()+ulSizeList).c_str()); 
    client.print(sResponse); sResponse="";
    MakeList(&client,true);
    client.print(sResponse2);
  } 
  else if(sPath=="/yfireset")
  {
    ulReqcount++;
    
                                client.println("HTTP/1.1 200 OK"); 
                                client.println("Content-Type: text/html");
                                client.println("Connection: close");
                                client.println();
                                client.println("<!DOCTYPE HTML>");
                                client.print("<html><head><title>Environment Monitor</title></head><body>");
                                client.print("<font color=\"#000000\"><body bgcolor=\"#a0dFfe\">");
                                client.print("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
                                client.print("<h1>Environment Monitor<BR>WiFi Reset Page </h1><BR><a href=\"/graphic2\">Pressure Graph Page</a><BR><a href=\"/pressure\">Pressure Gauge Page</a>");
                                client.print("<BR><a href=\"/monitor\">Sensor Gauges Page</a><BR><a href=\"/graphic\">Sensor Graph Page</a><BR><a href=\"/table\">Sensor Datalog Page</a><BR><BR><BR><BR>Restarted WiFiConnections = ");
                                client.print(ulReconncount);
                                client.print("<BR><FONT SIZE=-2>environmental.monitor.log@gmail.com<BR><FONT SIZE=-2>ESP8266 With DHT11 & BMP180 Sensors<BR>");
                                client.print("<FONT SIZE=-2>Compiled Using ver. 1.6.5-1160-gef26c5f, built on Sep 30, 2015<BR>");
                                client.println("<IMG SRC=\"https://raw.githubusercontent.com/genguskahn/ESP8266-For-DUMMIES/master/SoC/DimmerDocs/organicw.gif\" WIDTH=\"250\" HEIGHT=\"151\" BORDER=\"1\"></body></html>");

  WiFiStart();
  }
  else if(sPath=="/diag")
  {   
     float   servolt1 = ESP.getVcc();
     long int spdcount = ESP.getCycleCount();
     delay(1);
     long int spdcount1 = ESP.getCycleCount();
     long int speedcnt = spdcount1-spdcount; 
     FlashMode_t ideMode = ESP.getFlashChipMode();
     ulReqcount++;
File logF = SPIFFS.open("/humidlog.CSV", "r");      
     Dfsize = logF.size();
                                client.println("HTTP/1.1 200 OK"); 
                                client.println("Content-Type: text/html");
                                client.println("Connection: close");
                                client.println();
                                client.println("<!DOCTYPE HTML>");
                                client.print("<html><head><title>Environment Monitor</title></head><body>");
                                client.print("<font color=\"#000000\"><body bgcolor=\"#a0dFfe\">");
                                client.print("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
                                client.print("<h1>Environment Monitor<BR>SDK Diagnostic Information</h1><BR><a href=\"https://freeboard.io/board/QojxS6\">Sensor Gauges Freeboard IO Page</a><BR><a href=\"/jsread\">JSON Frame Download Page</a>");
                                client.print("<BR><a href=\"/monitor\">Sensor Gauges Page</a><BR><a href=\"/graphic\">Sensor Graph Page</a><BR><a href=\"/table\">Sensor Datalog Page</a><BR><a href=\"/\">Monitor LCD Text Input Page</a><BR>Restarted WiFiConnections = ");
                               String diagdat="";
                                diagdat+=ulReconncount;
                                diagdat+="<BR>  Web Page Requests = ";
                                diagdat+=ulReqcount;
                                diagdat+="<BR>  WiFi Station Hostname = ";
                                diagdat+=wifi_station_get_hostname();
                                diagdat+="<BR>  Free RAM = ";
                                client.print(diagdat);
                                client.print((uint32_t)system_get_free_heap_size()/1024);
                                diagdat=" KBytes<BR>  Logged Sample Count = ";
                                diagdat+=ulMeasCount;
                                diagdat+="<BR>  Total Sample points in 24 Hours = 1440<BR>  Minimum Sample Logging Interval = 1 Minutes<BR>  SDK Version = ";                                 
                                diagdat+=ESP.getSdkVersion();
                                diagdat+="<BR>  Boot Version = ";
                                diagdat+=ESP.getBootVersion();
                                diagdat+="<BR>  Free Sketch Space  = ";
                                diagdat+=ESP.getFreeSketchSpace()/1024;
                                diagdat+=" KBytes<BR>  Sketch Size  = ";
                                diagdat+=ESP.getSketchSize()/1024;
                                diagdat+=" KBytes<BR>";
                                client.print(diagdat);
                                client.printf("  Flash Chip id = %08X\n", ESP.getFlashChipId());
                                client.print("<BR>");
                                client.printf("  Flash Chip Mode = %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
                                diagdat="<BR>  Flash Size By ID = ";
                                diagdat+=ESP.getFlashChipRealSize()/1024;
                                diagdat+=" KBytes<BR>  Flash Size (IDE) = "; 
                                diagdat+=ESP.getFlashChipSize()/1024;
                                diagdat+=" KBytes<BR>  Flash Speed = ";
                                diagdat+=ESP.getFlashChipSpeed()/1000000;
                                diagdat+=" MHz<BR>  ESP8266 CPU Speed = ";
                                diagdat+=ESP.getCpuFreqMHz();
                                diagdat+=" MHz<BR>";
                                client.print(diagdat);
                                client.printf("  ESP8266 Chip id = %08X\n", ESP.getChipId());
                                diagdat="<BR>  System Instruction Cycles Per Second = ";
                                diagdat+=speedcnt*1000;  
                                diagdat+="<BR>  Last System Restart Time = ";
                                diagdat+=MyNtP;
                                diagdat+="<BR>  Last System Restart Reason = ";
                                diagdat+=ESP.getResetInfo();
                                diagdat+="<BR>  System Time = ";
                                diagdat+=TMnow;                                
                                diagdat+=" (Last Recorded Sample Time)<BR>  System VCC = ";
                                diagdat+=servolt1/1000, 3;
                                diagdat+=" V <BR>  Datalog File Size in Bytes = ";
                                diagdat+=Dfsize;
                                client.print(diagdat);
                                diagdat="";
                                client.print("<BR><FONT SIZE=-2>environmental.monitor.log@gmail.com<BR><FONT SIZE=-2>ESP8266 With DHT11 & BMP180 Sensors<BR><FONT SIZE=-2>Compiled Using ver. 2.0.0-rc2, built December, 2015<BR>");
                                client.println("<IMG SRC=\"https://raw.githubusercontent.com/genguskahn/ESP8266-For-DUMMIES/master/SoC/DimmerDocs/organicw.gif\" WIDTH=\"250\" HEIGHT=\"151\" BORDER=\"1\"></body></html>");
  }
   else if(sPath=="/srestart")
  {
                                client.println("HTTP/1.1 200 OK"); 
                                client.println("Content-Type: text/html");
                                client.println("Connection: close");
                                client.println();
                                client.println("<!DOCTYPE HTML>");
                                client.print("<html><head><title>Environment Monitor</title></head><body>");
                                client.print("<font color=\"#000000\"><body bgcolor=\"#a0dFfe\">");
                                client.print("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
                                client.print("<h1>Environment Monitor<BR>Please wait 10 Seconds......<BR><FONT SIZE=+2>System Reset Page !!!! </h1><BR><a href=\"/graphic2\">Pressure Graph Page</a><BR><a href=\"/pressure\">Pressure Gauge Page</a>");
                                client.print("<BR><a href=\"/monitor\">Sensor Gauges Page</a><BR><a href=\"/graphic\">Sensor Graph Page</a><BR><a href=\"/table\">Sensor Datalog Page</a><BR><a href=\"/diag\">Diagnostics Information Page</a><BR><a href=\"/\">Monitor LCD Text Input Page</a><BR><BR><BR><BR>Restarted WiFiConnections = ");
                                client.print(ulReconncount);
                                client.print("<BR><BR><BR><FONT SIZE=-2>environmental.monitor.log@gmail.com<BR><FONT SIZE=-2>ESP8266 With DHT11 & BMP180 Sensors<BR>");
                                client.print("<FONT SIZE=-2>Compiled Using ver. 1.6.5-1160-gef26c5f, built on Sep 30, 2015<BR>");
                                client.println("<IMG SRC=\"https://raw.githubusercontent.com/genguskahn/ESP8266-For-DUMMIES/master/SoC/DimmerDocs/organicw.gif\" WIDTH=\"250\" HEIGHT=\"151\" BORDER=\"1\"></body></html>");
client.stop();
wsysreset();
  }
  else if(sPath=="/graphic2")
  //////////////////////////////////////
  // format the html page for Barometer
  //////////////////////////////////////
  {
    barom = true;
    ulReqcount++;
    unsigned long ulSizeList = MakeList(&client,false); // get size of list first

    sResponse  = F("<html>\n<head>\n<title>Environment Monitor</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['corechart']}]}\"></script>\n");
    sResponse += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);\nfunction drawChart() {var data = google.visualization.arrayToDataTable([\n['Time / GMT', 'Pressure'],\n");    
    // here the big list will follow later - but let us prepare the end first
      
    // part 2 of response - after the big list
    sResponse2  = F("]);\nvar options = {title: 'Barometric Pressure',vAxes:{0:{viewWindowMode:'explicit',gridlines:{color:'black'},format:\"##.##mbar \"},},series:{0:{targetAxisIndex:0},},curveType:'function',legend:{ position: 'bottom'}};");
    sResponse2 += F("var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));chart.draw(data, options);}\n</script>\n</head>\n");
    sResponse2 += F("<body>\n<font color=\"#000000\"><body bgcolor=\"#a0dFfe\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>Environment Monitor</h1><BR><a href=\"/pressure\">Pressure Gauge Page</a><BR><a href=\"/monitor\">Sensor Gauges Page</a><BR><a href=\"/graphic\">Sensor Graph Page</a>");
    sResponse2 += F("<BR><a href=\"/table\">Sensor Datalog Page</a><BR><a href=\"/diag\">Diagnostics Information Page</a><BR><a href=\"/\">Monitor LCD Text Input Page</a><BR>\n<div id=\"curve_chart\" style=\"width: 600px; height: 400px\"></div>");
    sResponse2 += MakeHTTPFooter().c_str();
    
    // Send the response to the client - delete strings after use to keep mem low
    client.print(MakeHTTPHeader(sResponse.length()+sResponse2.length()+ulSizeList).c_str()); 
    client.print(sResponse); sResponse="";
    MakeList(&client,true);
    client.print(sResponse2);
    barom = false;
  }
  else 
////////////////////////////
// 404 for non-matching path
////////////////////////////
  {
    sResponse="<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL was not found on this server, What did you sk for?.</p></body></html>";
    ulReqcount++;
    sHeader  = F("HTTP/1.1 404 Not found\r\nContent-Length: ");
    sHeader += sResponse.length();
    sHeader += F("\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
    
    // Send the response to the client
    client.print(sHeader);
    client.print(sResponse);
  }
  
  // and stop the client
  client.stop();
}

byte sendEmail()
{
  byte thisByte = 0;
  byte respCode;

  if(client.connect( popyahoo,port) == 1) {
  } else {
    return 0;
  }
  if(!eRcv()) {return 0 ;}
  client.print("EHLO ");
    client.println(WiFi.localIP());
  if(!eRcv()) {return 0 ;}
  client.println("auth login");
  if(!eRcv()) {return 0 ;}
// Change to your base64 encoded user
  client.println(b64Usernm);//
  if(!eRcv()) {return 0 ;}
// change to your base64 encoded password
  client.println(b64Passwd);//
  if(!eRcv()) {return 0;}
// change to your email address (sender)  Send All Mail to Environment Monitor
  client.println(frmaddrs);
  if(!eRcv()) {return 0 ;}
// change to recipient address Send All Mail to Environment Monitor
  client.println(toaddres);
  if(!eRcv()) {return 0 ;}
  client.println("DATA");
  if(!eRcv()) {return 0 ;}
// change to recipient address Send All Mail to Environment Monitor
  client.println("To: The Environment Monitor <environmental.monitor.log@gmail.com>");
String sTable;
// change to your address This is sensor ID
  client.print("From: Living Room Barometer-Monitor <");
  client.print(wifi_station_get_hostname());
  client.println(">");

  client.println("Subject: ESP8266 Daily Environment Report\r\n");

  client.println("This is Sent Directly from An ESP8266 Module every 24 Hours");

  client.print("The Total Number Of Environment Samples Values Being Tranferred Are -  ");

  client.println(ulMeasCount);
  
  client.println("_____________________________________________________________");
File logF = SPIFFS.open("/humidlog.CSV", "r");
    if (!logF) {
      while(1);
    } 
  DsTable = logF.readStringUntil('[');
    while (logF.available())
    {
      sTable = logF.readStringUntil(']');
      sTable += (","); 
      sTable += logF.readStringUntil(',');   
      client.println(sTable);
     DsTable = logF.readStringUntil('[');
      sTable="";
    }

  client.println(sTable);

  client.println("_________________End of Data_______________");

  client.println(".");  
  if(!eRcv()) {return 0 ;}
  client.println("QUIT");
  if(!eRcv()) {return 0 ;}
  client.stop();
  return 1;
}

byte eRcv()
{
  byte respCode;
  byte thisByte;
  int loopCount = 0;

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      return 0;
    }
  }

  respCode = client.peek();

  while(client.available())
  {
    thisByte = client.read();
  }

  if(respCode >= '4')
  {
    efail();
    return 0;
  }

  return 1;
}


void efail()
{
  byte thisByte = 0;
  int loopCount = 0;

  client.println("QUIT");

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      return;
    }
  }

  while(client.available())
  {
    thisByte = client.read();
  }

  client.stop();
}
