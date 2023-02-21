2/*
 This code allows to connect to the WiFi LTE router and to operate the profiling buoy.

 created August 15th 2022
 by Bruno Courtemanche
 */
#include <SPI.h>
#include <WiFiNINA.h>
#include "TimeLib.h"
#include <SoftwareSerial.h>
#include <OneWire.h> 
#include <Wire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>                           //we have to include the SoftwareSerial library, or else we can't use it

#include "arduino_secrets.h" 


#define DEPTH 25
#define buoyIdentifier 6660001
#define rx 0                                         //define what pin rx is going to be
#define tx 1                                         //define what pin tx is going to be
#define STEPPER_STEP_PUSH 3                          //Port to push the stepper to rotate
#define STEPPER_DIRECTION 2                          //Direction of the steppper rotation
#define RELAY_STEPPER 7                              // Control if stepper has power or not
#define TEMP_PROBE 4                                 // Temperature probe
#define LIMIT_SWITCH 5                               //Limit switch reading port
#define LIMIT_SWITCHPOWER 6
#define PROFILE_DELAY 1800000                           //Delay betweeen vertical profiles


///////Sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status

bool directionUP = false;

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "www.extreme-way.com";    // name address for Google (using DNS)

// Setup a oneWire instance to communicate with any OneWire devices  
// (not just Maxim/Dallas temperature ICs) 
OneWire oneWire(TEMP_PROBE); 
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
/********************************************************************/ 

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;

SoftwareSerial myserial(rx, tx);                      //define how the soft serial port is going to work

String inputstring = "";                              //a string to hold incoming data from the PC
String sensorstring = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_complete = false;                //have we received all the data from the PC
boolean sensor_string_complete = false;               //have we received all the data from the Atlas Scientific product
float DO;                                             //used to hold a floating point number that is the DO



int count;
int count2;
int prof;
bool motorON = false;

struct profile {
  char timeStamp[23];
  int prof[DEPTH/5];
  float doUP[DEPTH/5];
  float doDOWN[DEPTH/5];
  float tempUP[DEPTH/5];
  float tempDOWN[DEPTH/5];
};

int profCount=0;
int profMax=1;
profile profilingBuoy;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial2.begin(9600);                                //set baud rate for software serial port_3 to 9600
  inputstring.reserve(10);                            //set aside some bytes for receiving data from the PC
  sensorstring.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product
  
  // Start up the library
  sensors.begin();
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LIMIT_SWITCH, INPUT);                    // Limit switch
  pinMode(LIMIT_SWITCHPOWER, OUTPUT);              // Signal to measure
  pinMode(RELAY_STEPPER, OUTPUT);                  // Activate the relay for the stepper motor
  pinMode(STEPPER_DIRECTION, OUTPUT);              //Control the direction
  pinMode(STEPPER_STEP_PUSH, OUTPUT);              //Do one step
  digitalWrite(RELAY_STEPPER, LOW);
  digitalWrite(STEPPER_DIRECTION, HIGH);
  digitalWrite(LIMIT_SWITCHPOWER, HIGH);

  count= 0;
  count2= 0;
  prof = 0;
  

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi");

}

void serialEvent() {                                  //if the hardware serial port_0 receives a char
  inputstring = Serial.readStringUntil(13);           //read the string until we see a <CR>
  input_string_complete = true;                       //set the flag used to tell if we have received a completed string from the PC
}


void serialEvent2() {                                 //if the hardware serial port_3 receives a char
  sensorstring = Serial2.readStringUntil(13);         //read the string until we see a <CR>
  sensor_string_complete = true;                      //set the flag used to tell if we have received a completed string from the PC
}

void loop() {
  
  if(!motorON){
    Serial.println("Limit switch calibration for depth...");
    digitalWrite(RELAY_STEPPER, LOW);

    //It will go up first...
    digitalWrite(STEPPER_DIRECTION, LOW);     //Control the direction
    directionUP = true; 

    while(digitalRead(LIMIT_SWITCH)==LOW){
      digitalWrite(STEPPER_STEP_PUSH, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(3);                       // wait for a milisecond
      //digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(STEPPER_STEP_PUSH, LOW);    // turn the LED off by making the voltage LOW
      delay(3);                       // wait for a milisecond
    }

    Serial.println("Limit switch located...");

    
    //The it goes down at the right spot
    digitalWrite(STEPPER_DIRECTION, HIGH);     //Control the direction
  directionUP = false; 
    for (int l = 0; l <=1750; l++){
      digitalWrite(STEPPER_STEP_PUSH, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(3);                       // wait for a milisecond
      //digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(STEPPER_STEP_PUSH, LOW);    // turn the LED off by making the voltage LOW
      delay(3);                       // wait for a milisecond
    }
    Serial.println("Height adjust...");

    motorON = true; 
  }

  if(motorON){
    //Serial.println("we are going down"); 
    if(count<=(DEPTH*50)){

      if(directionUP){
        //It will go down first...
        digitalWrite(STEPPER_DIRECTION, HIGH);     //Control the direction
        directionUP=false;
      }

      digitalWrite(STEPPER_STEP_PUSH, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(3);                       // wait for a milisecond
      //digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(STEPPER_STEP_PUSH, LOW);    // turn the LED off by making the voltage LOW
      delay(3);                       // wait for a milisecond
      count = count +1;
      //Serial.print("Counter = ");
      //Serial.println(count); 
    }
    if(count%250==0){
     delay(5000); 
     prof = prof+5;
     profilingBuoy.prof[profCount] = prof;
     Serial.print("Profondeur = ");
     Serial.print(profilingBuoy.prof[profCount]);
     Serial.println(" cm");
    
      // call sensors.requestTemperatures() to issue a global temperature 
      // request to all devices on the bus
      sensors.requestTemperatures(); // Send the command to get temperatures
      // After we got the temperatures, we can print them here.
      // We use the function ByIndex, and as an example get the temperature from the first sensor only.
      Serial.print("Temperature for the device 1 (index 0) is: ");
      profilingBuoy.tempDOWN[profCount]=sensors.getTempCByIndex(0);
      Serial.println(profilingBuoy.tempDOWN[profCount]);  
    
      if (input_string_complete == true) {                //if a string from the PC has been received in its entirety
        Serial2.print(inputstring);                       //send that string to the Atlas Scientific product
        Serial2.print('\r');                              //add a <CR> to the end of the string
        inputstring = "";                                 //clear the string
        input_string_complete = false;                    //reset the flag used to tell if we have received a completed string from the PC
      }
    
    
      if (sensor_string_complete == true) {               //if a string from the Atlas Scientific product has been received in its entirety
        Serial.print("Dissolved oxygen level is: ");
        profilingBuoy.doDOWN[profCount]=sensorstring.toFloat();
        Serial.println(profilingBuoy.doDOWN[profCount]);  //send that string to the PC's serial monitor
       /*                                                 //uncomment this section to see how to convert the D.O. reading from a string to a float 
        if (isdigit(sensorstring[0])) {                   //if the first character in the string is a digit
          DO = sensorstring.toFloat();                    //convert the string to a floating point number so it can be evaluated by the Arduino
          if (DO >= 6.0) {                                //if the DO is greater than or equal to 6.0
            Serial.println("high");                       //print "high" this is demonstrating that the Arduino is evaluating the DO as a number and not as a string
          }
          if (DO <= 5.99) {                               //if the DO is less than or equal to 5.99
            Serial.println("low");                        //print "low" this is demonstrating that the Arduino is evaluating the DO as a number and not as a string
          }
        }
      */
      }
      sensorstring = "";                                  //clear the string:
      sensor_string_complete = false;                     //reset the flag used to tell if we have received a completed string from the Atlas Scientific product

      Serial.print("Token de profondeur: ");
      Serial.println(profCount);
      profCount++;
      profMax=profCount;
     
    }
    if(count>(DEPTH*50)){
      count=((DEPTH*50))+2;

       if(!directionUP){
        //It will go down first...
        digitalWrite(STEPPER_DIRECTION, LOW);     //Control the direction
        directionUP=true;
      }

      if(motorON){
        //digitalWrite(LED_BUILTIN, HIGH);
        digitalWrite(STEPPER_STEP_PUSH, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(3);                       // wait for a milisecond
        //digitalWrite(LED_BUILTIN, LOW);
        digitalWrite(STEPPER_STEP_PUSH, LOW);    // turn the LED off by making the voltage LOW
        delay(3);                       // wait for a milisecond
        count2 = count2 +1;
        //Serial.print("Counter = ");
        //Serial.println(count); 
        if(count2%250==0){
          profCount--;
          Serial.print("Token de profondeur: ");
          Serial.println(profCount);
          delay(5000); 
          prof = prof-5;
          Serial.print("Profondeur = ");
          Serial.print(prof);
          Serial.println(" cm");
          
          // call sensors.requestTemperatures() to issue a global temperature 
          // request to all devices on the bus
          sensors.requestTemperatures(); // Send the command to get temperatures
          // After we got the temperatures, we can print them here.
          // We use the function ByIndex, and as an example get the temperature from the first sensor only.
          Serial.print("Temperature for the device 1 (index 0) is: ");
          profilingBuoy.tempUP[profCount]=sensors.getTempCByIndex(0);
          Serial.println(profilingBuoy.tempUP[profCount]);  
      
          if (input_string_complete == true) {                //if a string from the PC has been received in its entirety
            Serial2.print(inputstring);                       //send that string to the Atlas Scientific product
            Serial2.print('\r');                              //add a <CR> to the end of the string
            inputstring = "";                                 //clear the string
            input_string_complete = false;                    //reset the flag used to tell if we have received a completed string from the PC
          }
          
          if (sensor_string_complete == true) {               //if a string from the Atlas Scientific product has been received in its entirety
            Serial.print("Dissolved oxygen level is: ");
            profilingBuoy.doUP[profCount]=sensorstring.toFloat();
            Serial.println(profilingBuoy.doUP[profCount]);  //send that string to the PC's serial monitor
           /*                                                 //uncomment this section to see how to convert the D.O. reading from a string to a float 
            if (isdigit(sensorstring[0])) {                   //if the first character in the string is a digit
              DO = sensorstring.toFloat();                    //convert the string to a floating point number so it can be evaluated by the Arduino
              if (DO >= 6.0) {                                //if the DO is greater than or equal to 6.0
                Serial.println("high");                       //print "high" this is demonstrating that the Arduino is evaluating the DO as a number and not as a string
              }
              if (DO <= 5.99) {                               //if the DO is less than or equal to 5.99
                Serial.println("low");                        //print "low" this is demonstrating that the Arduino is evaluating the DO as a number and not as a string
              }
            }
          */
          }
          sensorstring = "";                                  //clear the string:
          sensor_string_complete = false;                     //reset the flag used to tell if we have received a completed string from the Atlas Scientific product


         
        }
        if(count2>(DEPTH*50)){
          count2 = 0;
          count = 0;
          //Delay between profiles
          digitalWrite(RELAY_STEPPER, HIGH);
          motorON = false;

            getCurrentTime();
            printCurrentNet();
            //goOnline();

            Serial.print("Time: ");
            Serial.println(profilingBuoy.timeStamp);

            // if the server's disconnected, stop the client:
            if (!client.connected()) {
              Serial.println();
              Serial.println("disconnecting from server.");
              client.stop();

            }
            delay(PROFILE_DELAY); 
        }
      }
    }
  }


}

void goOnline(){
  Serial.println("\nStarting connection to server...");
  if (client.connect(server, 80)) {
  // if you get a connection, report back via serial:
    int num_of_depths = (sizeof(profilingBuoy.prof)/8);
    for(int f = 0; f<profMax; f++){
      Serial.println("trying to connect to server");
      
      Serial.println("connected to server");
      // Make a HTTP request:
      client.println("POST /lakeUpload/php_profileFilling.php HTTP/1.1");
      client.println("Host: www.extreme-way.com");
      client.println("Content-Type: application/x-www-form-urlencoded");

      String postData; 
      postData = "buoyIMEI=";
      postData += buoyIdentifier;

      postData += "&timestamp=";
      postData +=profilingBuoy.timeStamp;

      postData += "&depth=";
      postData += profilingBuoy.prof[f];
              
      postData += "&doUP=";
      postData += profilingBuoy.doUP[f];

      postData += "&doDOWN=";
      postData += profilingBuoy.doDOWN[f];

      postData += "&tempUP=";
      postData += profilingBuoy.tempUP[f];

      postData += "&tempDOWN=";
      postData +=profilingBuoy.tempDOWN[f];

      client.print("Content-Length: ");
      client.println(postData.length());
      client.println();
      client.print(postData);
      client.println();

      Serial.println(postData);
    }
  }
  // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  Serial.println("");

  if (client.connected()) {
    client.stop();
  }
}

void getCurrentTime() {
  time_t secSinceForever = WiFi.getTime();
  String timeBufferTransfer;
  timeBufferTransfer = "'";
  timeBufferTransfer += year(secSinceForever);
  timeBufferTransfer += "-";
  if(month(secSinceForever)>=10){ 
    timeBufferTransfer += month(secSinceForever);
  }
  else{
    timeBufferTransfer += "0";
    timeBufferTransfer += month(secSinceForever);
  }
  timeBufferTransfer += "-";
  if(day(secSinceForever)>=10){ 
    timeBufferTransfer += day(secSinceForever);
  }
  else{
    timeBufferTransfer += "0";
    timeBufferTransfer += day(secSinceForever);
  }
  timeBufferTransfer += " ";
  if(hour(secSinceForever)>=10){ 
    timeBufferTransfer += hour(secSinceForever);
  }
  else{
    timeBufferTransfer += "0";
    timeBufferTransfer += hour(secSinceForever);
  }
  timeBufferTransfer +=":";
  if(minute(secSinceForever)>=10){ 
    timeBufferTransfer += minute(secSinceForever);
  }
  else{
    timeBufferTransfer += "0";
    timeBufferTransfer += minute(secSinceForever);
  }
  timeBufferTransfer +=":";
  if(second(secSinceForever)>=10){ 
    timeBufferTransfer += second(secSinceForever);
  }
  else{
    timeBufferTransfer += "0";
    timeBufferTransfer += second(secSinceForever);
  }
  timeBufferTransfer += "'";
  timeBufferTransfer.toCharArray(profilingBuoy.timeStamp, 23);

}

void printWifiData() {
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
