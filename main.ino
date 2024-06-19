/*
  Repeating Wifi Web Client, JSON Parser and RGB Led Driver

  This sketch connects to a web server and makes a request
  using a WiFi Module. The server response is JSON formatted data, parsed using ArduinoJSON Library.
  The arithmetic mean from the extracted Particles Matter Values is compared with the preset values of the different levels of PM pollution.
  Depending on the result certain analog levels are set on selected output pins. The levels operate the external transistors to generate 
  the color corresponding to the level of PM pollution.

*/

/*  Example URL: https://data.sensor.community/airrohr/v1/sensor/39300/
    "39300" is the unique ID of a particular Air Quality Sensor

    The server response is a data set of name/value pairs in JSON (JavaScript Object Notation) format. 
    The data is the last stored data from the selected sensor. The set contains information about the geo-location of the sensor, 
    timestamp (time and date of the measurement) and both PM10 and PM2.5. Since the PM2.5 is more health-damaging, we extract that value from the server response.
    The Luftdaten sensors take measure every 5 minutes so we check for new data in our selected set of 9 sensors.

    Example response for sensor with ID "39300":
    [{"sensordatavalues":[{"value_type":"P1","id":4936976270,"value":"2.73"},{"value_type":"P2","id":4936976271,"value":"2.00"}],
    "timestamp":"2020-10-18 15:16:17","id":2285397478,"sensor":{"sensor_type":{"name":"SDS011","id":14,"manufacturer":"Nova Fitness"},"id":39300,"pin":"1"},
    "location":{"longitude":"23.38","altitude":"628.8","latitude":"42.626","id":25019,"exact_location":0,"country":"BG","indoor":0},"sampling_rate":null}]
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"
/////// arduino_secrets.h - Selects WiFi Network and sets the Password
char ssid[] = SECRET_SSID;    // network SSID (name)
char pass[] = SECRET_PASS;    // network password
int keyIndex = 0;             // network key Index number (needed only for WEP) - not used

// Return the WiFi connection status.
int status = WL_IDLE_STATUS; //a temporary status assigned when WiFi.begin() is called and remains active until the number of attempts expires (resulting in WL_CONNECT_FAILED) or a connection is established (resulting in WL_CONNECTED)

//  gives names to a constant values (the pins of the RGB Led Driver)
#define REDPIN A2
#define GREENPIN A3
#define BLUEPIN A5

// Defines 6 colors in 6 areas containing 3 elements for Red, Green and Blue Value.
// The values are modified for better representation of the desired colors from PM pollution color set.
const int color1[3] = {0, 128, 15};   //green {0, 255, 30}
const int color2[3] = {128, 90, 0};   //yellow {255, 210, 0}
const int color3[3] = {255, 40, 0};   //orange
const int color4[3] = {150, 0, 0};    //red {255, 0, 0}
const int color5[3] = {128, 0, 80};   //pink {225, 0, 180}
const int color6[3] = {70, 0, 150};   //purple {90, 0, 180}

int r = 0, g = 0, b = 0;  // Initialize r, g and b variable to 0 - vars used to assign the values to the control pins of the LED strip driver scheme

// Define an area of 9 constants - items, representing the unique identifiers of 9 Air pollution sensors in the area
const int station[9] = {28130, 7334, 9920, 6693, 11076, 39300 , 11807 , 28864};

int ledPin = 2;   // Initialize the output pin for the control LED - blinks on every web request

WiFiSSLClient client; // Initialize the Wifi client

const char server[] = "data.sensor.community";  // defines domain address of the server where the data from the air sensors is collected and later served on demand in JSON format:

// defines the var and const of the timer for the delay between two groups of web request (9 requests for MP2.5 data, waits 3 minutes, then again sends the same 9 requests for data)
unsigned long lastConnectionTime = 0;               // last time we connected to the server, in milliseconds
const unsigned long postingInterval = 180L * 1000L; // delay between updates, in milliseconds 180 000 milliseconds = 3 minutes

bool isFirstRun = true;   // Initialize a boolean variable used in the initial loop of the sketch
// - if "true" runs a group of requests right after the Arduino is powered than it is set to "false"


// setup()function is called when a sketch starts - initialize variables, pin modes, start using libraries, etc.
// Runes only run once, after each powerup or reset of the Arduino board.
void setup() {

  // initialize the selected physical pins as outputs
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);

  // sets the channels to 0 - the LED Strip is turned OFF at the beginning
  analogWrite(REDPIN, 0);
  analogWrite(GREENPIN, 0);
  analogWrite(BLUEPIN, 0);

  // initialize the selected physical pin of the control LED as an output
  pinMode(ledPin, OUTPUT);


  //Initialize serial port:
  Serial.begin(9600);


  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  // check for the WiFi firmware version - for debug purposes:
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 5 seconds for connection:
    delay(5000);
  }
  // print out the WiFi status when connected:
  printWifiStatus();
}


// main function that loops consecutively
void loop() {

  // if there's incoming data from the net connection send it out the serial port.  This is for debugging purposes only:
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }

  // check if the var "isFirstRun" is "true" or "false", If "true" run once the function "httpRequest2" and set "isFirstRun" to "false"
  // thus skipping it on the next loop if "false" skip and continue
  if (isFirstRun) {
    httpRequest();
    isFirstRun = false;
  }

  // if 3 minutes have passed since the last connection,
  // then connect again through "httpRequest()" function (send new HTTP requests, get the responses, pars them and calculate the result):
  if (millis() - lastConnectionTime > postingInterval) {
    httpRequest();
  }

}


// this function makes HTTP connections to the server, gets the responses, parses them and calculates the result:
void httpRequest() {

  //Initialize local var "recordValue" - represents the average from all 9 sensors for the present cycle
  float recordValue;

  // Initialize the variables for the arithmetic mean calculation
  int resultCount = 0;
  float resultSum = 0;
  float resultAverage = 0;

  // for loop going through all 9 requests, responses, parsing and calculations for the selected 9 sensors
  for (int i = 0; i < 8; i++) {



    int mid_url = station[i];       // gets the ID of the number "i" station from the station's area and associates it with the local var "mid_url"

    char httpPAQString[64];         // initialize HTTP Path and Query String for the HTTP request (the second half of the whole request)

    //Composes the HTTP Path and Query String. Adds the sensor ID "mid_url" inside the "httpPAQString"
    sprintf(httpPAQString, "GET /airrohr/v1/sensor/%u/ HTTP/1.1", mid_url);

    Serial.println(F(httpPAQString)); //prints the result to the serial port for debug purposes

    // close any connection before sending a new request.
    // This will free the socket on the WiFi module
    client.stop();

    client.setTimeout(5000);         // sets the maximum milliseconds to wait for the response from the server

    // Connects to a specified URL and port. The return value indicates success or failure.
    if (!client.connect(server, 443)) {
      Serial.println(F("Connection failed")); // F is used to save RAM. F puts the string in the flash memory, leaving more RAM to work with
      return;
    }


    Serial.println(F("Connected!"));  // Notifies for successful connection if connected - just for debugging

    // Send HTTP request
    client.println(httpPAQString); //Print data to the server the client is connected to.
    client.println("Host: data.sensor.community");
    client.println(F("Connection: close"));

    // checks if any request was sent
    if (client.println() == 0) {
      Serial.println(F("Failed to send request"));
      return;
    }

    // Check HTTP status (copied)
    char status[32] = {0};                                  // initialize array of char
    client.readBytesUntil('\r', status, sizeof(status));    // reads characters from a stream into a buffer. The function terminates if the terminator character "\r" is detected.
    // Returns the number of bytes placed in the buffer
    // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) {                // compares the string "status" to the string "200 OK".
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      return;
    }

    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";         // initialize array of char for End Of Headers
    if (!client.find(endOfHeaders)) {         // checks if there is End Of Headers
      Serial.println(F("Invalid response"));  //  if not print "Invalid response" - for debugging
      return;
    }


    // Allocate the JSON document (copied from the documentation in arduinojson.org
    // Use arduinojson.org/v6/assistant to compute the capacity.
    const size_t capacity = 3 * JSON_ARRAY_SIZE(2) + 8 * JSON_OBJECT_SIZE(3) + 2 * JSON_OBJECT_SIZE(6) + 2 * JSON_OBJECT_SIZE(7) + 1380;

    DynamicJsonDocument doc(capacity);  // allocates memory pool for the Dynamic Json Document
    ARDUINOJSON_USE_LONG_LONG == 1;     // determines the type used to store integer values (long long, because of the size of the JSON doc)


    deserializeJson(doc, client); //The function deserializeJson() parses a JSON input "client" and puts the result in a Json Document "doc"

    // checks for errors and prints them on the serial port
    DeserializationError error = deserializeJson(doc, client);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }

    // note the time that the connection was made:
    lastConnectionTime = millis();

    // finds the needed value from the sensor. based on the analysis of the JSON response (used arduinojson.org/v6/assistant)
    JsonObject record = doc[0];                                     // object "record" -  a collection of key-value pairs
    JsonObject record_value_1 = record["sensordatavalues"][1];      // object "record_value_1" - an object in the "record" object that is collection of key-value pairs
    recordValue = record_value_1["value"];                          // the value for PM2.5 from the object/key-value pair "record_value_1"


    //Print the result for PM2,5
    Serial.println(F("===PM2.5===:"));
    Serial.println(recordValue);
    Serial.println(F("===========:"));
    // Disconnect
    client.stop();


    // Blink the LED diode once as a status update for a finished attempt of a URL request / JSON analysis
    digitalWrite(ledPin, HIGH);      // sets the LED on
    delay(500);                      // waits for half a second
    digitalWrite(ledPin, LOW);       // sets the LED off


    // Calculating the average PM2.5
    if (recordValue > 0) {                    // check if the current "recordValue" result after JSON parsing is higher than 0, in other word, we have data from a working sensor
      resultCount = resultCount + 1;          // increase the counter of the real results
      resultSum = resultSum + recordValue;    // add the current result "recordValue" to the sum of the previous results "resultSum"
    } else {
      resultCount = resultCount;              // if the recordValue is less or equal 0 keep "resultCount" and "resultSum" as they are (without increase),
      resultSum = resultSum;                  // thus ensuring we will produce correct calculation of the average result
    }
    resultAverage = resultSum / resultCount;  // calculating "resultAverage" - the average result
  }

  // set the color of the LED Strip based on the "resultAverage"
  if (resultAverage <= 25) {       // checking for the "green" color
    r = color1[0];                 // assigns the value for the "r" from the first element of area "color1" - RED channel
    g = color1[1];                 // assigns the value for the "g" from the second element of area "color1" - GREEN channel
    b = color1[2];                 // assigns the value for the "b" from the third element of area "color1" - BLUE channel
    analogWrite(REDPIN, r);        // writes the value of "r" to the analog output - the PIN connected to the transistor regulating the RED color of the LED strip
    analogWrite(GREENPIN, g);      // writes the value of "g" to the analog output - the PIN connected to the transistor regulating the GREEN color of the LED strip
    analogWrite(BLUEPIN, b);       // writes the value of "b" to the analog output - the PIN connected to the transistor regulating the BLUE color of the LED strip

  } else if ((resultAverage > 25) && (resultAverage <= 50)) {     // if not "green" checking for the "yellow" color
    r = color2[0];
    g = color2[1];
    b = color2[2];
    analogWrite(REDPIN, r);
    analogWrite(GREENPIN, g);
    analogWrite(BLUEPIN, b);
  } else if ((resultAverage > 50) && (resultAverage <= 75)) {     // if not "yellow" checking for the "orange" color
    r = color3[0];
    g = color3[1];
    b = color3[2];
    analogWrite(REDPIN, r);
    analogWrite(GREENPIN, g);
    analogWrite(BLUEPIN, b);
  } else if ((resultAverage > 75) && (resultAverage <= 100)) {    // if not "orange" checking for the "red" color
    r = color4[0];
    g = color4[1];
    b = color4[2];
    analogWrite(REDPIN, r);
    analogWrite(GREENPIN, g);
    analogWrite(BLUEPIN, b);
  } else if ((resultAverage > 100) && (resultAverage <= 500)) {   // if not "red" checking for the "pink" color
    r = color5[0];
    g = color5[1];
    b = color5[2];
    analogWrite(REDPIN, r);
    analogWrite(GREENPIN, g);
    analogWrite(BLUEPIN, b);
  } else if (resultAverage > 500) {    // if not "pink" checking for the "purple" color
    r = color6[0];
    g = color6[1];
    b = color6[2];
    analogWrite(REDPIN, r);
    analogWrite(GREENPIN, g);
    analogWrite(BLUEPIN, b);
  }

  //print values in serial port for debug
  Serial.println ("Average:");
  Serial.println(resultCount);
  Serial.println(resultAverage);
}

// function for checking and printing of the WiFi Status
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
