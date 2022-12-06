#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Provide login information for WiFi
const char* ssid = "NETGEAR49";
const char* password = "kindoboe238";

// Assign ports
const int dns_port = 53;
const int http_port = 80;
const int ws_port = 1337;

// Used for http communication w/ website
AsyncWebServer server(http_port);   
WebSocketsServer webSocket = WebSocketsServer(ws_port);

// Pins on relay board for each sensor/actuator
const int led_pin = 18;  // K3 Relay
const int fan_pin = 19;    // K2 Relay
const int air_compressor_pin = 21;    // K1 Relay
const int release_valve_pin = 5;    // K4 Relay
const int temp_pin = 23;    // One pin for bus data from temp. sensors
const int pir_pin = 26;
const int rain_sensor_pin = 36;
const int ceiling_slit_pin = 33;

// Initialize states for each sensor/actuator
char msg_buf[100];    // Save any incoming websocket messages in this buffer
int led_state = 0;    // State of LED to send as a message to client
int fan_state = 0;    // State of fan to send as a message to client
int fan_state_pause = 0;    // State of whether the fan is on pause or not
int fan_state_cool = 0;    // State of whether the fan should be cooling shed or not
int air_compressor_state = 0;    // State of air compressor to send as a message to client
int closing_ceiling_procedure = 0;    // State of ceiling slit closing procedure
int opening_ceiling_procedure = 0;    // State of ceiling slit opening procedure
int release_valve_state = 0;    // State of release valve to send as a message to client
int ceiling_slit_state = 0;    // Open == 0, Closed == 1
float indoor_temp_state = 0;
float outdoor_temp_state = 0;
int pir_state = 0;
int battery_sensor_state = 0;
int rain_sensor_state = 0;    // No rain == 1, Rain == 0

// The difference between the indoor/outdoor temperature in order to turn on/off the fan
const float temp_differential = 5.0;    // degrees F

// Variables for timer delay used in retrieving data from sensors & opening/closing ceiling slits
unsigned long lastTime = 0;
unsigned long timerDelay = (1000)*0.25;    // Timer delay for sending all sensor & actuator data 
unsigned long lastTimeOpenCeilingSlits = 0;
unsigned long timerDelayOpenCeilingSlits = (1000)*25;    // Time to open ceiling slits
unsigned long lastTimeCloseCeilingSlits = 0;
unsigned long timerDelayCloseCeilingSlits = (1000)*10;    // Time to close ceiling slits
unsigned long lastTimeFan = 0;
unsigned long timerDelayFan = (1000)*5;    // Keep fan on for this amount of time
unsigned long lastTimeFanPause = 0;
unsigned long timerDelayFanPause = (1000)*15;    // Pause the fan for this amount of time

// Setup for temp. sensors
OneWire oneWire(temp_pin);    // Setup OneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);    // Pass our oneWire reference to Dallas Temperature
DeviceAddress insideThermometer, outsideThermometer;    // arrays to hold device addresses
const int temp_precision = 9;    // Bit resolution



/*
 * WebSocket Functions
 * - onWebSocketEvent()
 * - onIndexRequest()
 * - onCSSRequest()
 * - onJSRequest()
 * - onPageNotFound()
 */
// WebSocket Server Callback
// Callback when receiving any Websocket message (Library can handle up to 5 clients)
// num is the assigned number of the client that has been connected
// type is the type of message; determines how to handle each message
// payload is the data as raw bites
// length is how many bits are in the data
void onWebSocketEvent(uint8_t client_num, WStype_t type, uint8_t * payload, size_t length){
    // Figure out the type of WebSocket event and react accordinly
    switch(type){
      // Client has disconnected
      case WStype_DISCONNECTED:
        Serial.printf("[%u] Disconnected!\n", client_num);
        break;

      // New client has connected
      case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(client_num);
        Serial.printf("[%u] Connection from ", client_num);
        Serial.println(ip.toString());
      }
      break;

      // Handle text messages from client
      case WStype_TEXT:
        // Toggle LED
        if (strcmp((char *)payload, "toggleLedButton")==0){
          led_state = digitalRead(led_pin);
          if (led_state == 0){
            led_state = 1;
          }
          else{
            led_state = 0;
          }
          Serial.printf("Toggling LED to %u\n", led_state);
          digitalWrite(led_pin, led_state);
        }

        // Toggle fan
        else if (strcmp((char *)payload, "toggleFanButton")==0) {
          fan_state = digitalRead(fan_pin);
          if (fan_state == 0){
            fan_state = 1;
          }
          else{
            fan_state = 0;
          }
          Serial.printf("Toggling Fan to %u\n", fan_state);
          digitalWrite(fan_pin, fan_state);
        }
        
        // Toggle air compressor
        else if (strcmp((char *)payload, "toggleAirCompressorButton")==0) {
          air_compressor_state = digitalRead(air_compressor_pin);
          if (air_compressor_state == 0){
            air_compressor_state = 1;
          }
          else{
            air_compressor_state = 0;
          }
          Serial.printf("Toggling Air Compressor to %u\n", air_compressor_state);
          digitalWrite(air_compressor_pin, air_compressor_state);
        }

        // Toggle release valve
        else if (strcmp((char *)payload, "toggleReleaseValveButton")==0) {
          release_valve_state = digitalRead(release_valve_pin);
          if (release_valve_state == 0){
            release_valve_state = 1;
          }
          else{
            release_valve_state = 0;
          }
          Serial.printf("Toggling Release Valve to %u\n", release_valve_state);
          digitalWrite(release_valve_pin, release_valve_state);
        }

        // Message not recognized
        else{
          Serial.println("[%u] Message not recognized");
        }
        break;
      
      // For everything else: do nothing
      case WStype_BIN:
      case WStype_ERROR:
      case WStype_FRAGMENT_TEXT_START:
      case WStype_FRAGMENT_BIN_START:
      case WStype_FRAGMENT:
      case WStype_FRAGMENT_FIN:
      default:
        break;
    }
}

// Web Server Callbacks
// Show HTML page
void onIndexRequest(AsyncWebServerRequest *request){
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/index.html", "text/html");    // Respond by sending html file in SPIFFS directory
}

// Show CSS page
void onCSSRequest(AsyncWebServerRequest *request){
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/style.css", "text/css");    // Respond by sending CSS file in SPIFFS directory
}

// Connect Javascript file
void onJSRequest(AsyncWebServerRequest *request){
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/script.js", "text/js");    // Respond by sending JS file in SPIFFS directory
}

// Show error if requested file does not exist
void onPageNotFound(AsyncWebServerRequest *request){
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(404, "text/plain", "Not found");
}



/*
 * WiFi Information 
 */
// Function for Non-Static IP Address
// Use this function first when connecting to a new network
// then update the wifi parameters in the Static IP Address function
// and switch to the Static IP Address
void connectToWifi(){
  // Start access point
  WiFi.begin(ssid, password);

  // Print our IP address
  Serial.println(ssid);
  Serial.println(password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nIP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS 1: ");
  Serial.println(WiFi.dnsIP(0));
  //Serial.print("DNS 2: ");
  //Serial.println(WiFi.dnsIP(1));

  // On HTTP request for root, provide index.html file
  server.on("/", HTTP_GET, onIndexRequest);

  // On HTTP request for style sheet, provide style.css file
  server.on("/style.css", HTTP_GET, onCSSRequest);

  // On HTTP request for javascript file
  server.on("/script.js", HTTP_GET, onJSRequest);

  // Handle requests for pages that do not exist
  server.onNotFound(onPageNotFound);

  // Start web server
  server.begin();

  // Start Websocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

// Function for Static IP Address
// Don't forget to update the url (first line) in script.js
void connectToWifiStatic(){
  IPAddress staticIP(192, 168, 1, 2);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress dns(192, 168, 1, 1);

  // See if parameters are available/correct
  if (WiFi.config(staticIP, gateway, subnet, dns) == false) {
    Serial.println("Configuration failed.\nStaticIP not available.");
  }

  // Start access point
  WiFi.begin(ssid, password);

  // Print our IP address
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nIP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS 1: ");
  Serial.println(WiFi.dnsIP(0));

  // On HTTP request for root, provide index.html file
  server.on("/", HTTP_GET, onIndexRequest);

  // On HTTP request for style sheet, provide style.css file
  server.on("/style.css", HTTP_GET, onCSSRequest);

  // On HTTP request for javascript file
  server.on("/script.js", HTTP_GET, onJSRequest);

  // Handle requests for pages that do not exist
  server.onNotFound(onPageNotFound);

  // Start web server
  server.begin();

  // Start Websocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}



/*
 * Main
 */
void setup(){
  pinMode(led_pin, OUTPUT);
  pinMode(fan_pin, OUTPUT);
  pinMode(air_compressor_pin, OUTPUT);
  pinMode(release_valve_pin, OUTPUT);
  pinMode(pir_pin, INPUT);
  pinMode(rain_sensor_pin, INPUT);    // Needs an external pull-up resistor
  pinMode(ceiling_slit_pin, INPUT);    // Needs an external pull-up resistor

  // FYI - HIGH and LOW are switched only for onboard LED in ESP32
  digitalWrite(led_pin, LOW);    
  digitalWrite(fan_pin, LOW);
  digitalWrite(air_compressor_pin, LOW);
  digitalWrite(release_valve_pin, LOW);

  // Start Serial port
  Serial.begin(115200);

  // Make sure we can read the file system
  if(!SPIFFS.begin()){
    // If SPIFFS doesn't load, then print message and enter forever loop
    Serial.println("Error mounting SPIFFS");
    while(1);
  }

  // Start up the library for temp sensors
  sensors.begin();

  // Setup temp. sensors
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  if (!sensors.getAddress(outsideThermometer, 1)) Serial.println("Unable to find address for Device 1");

  // set the temp. sensor resolutions to 9 bit per device
  sensors.setResolution(insideThermometer, temp_precision);
  sensors.setResolution(outsideThermometer, temp_precision);


  connectToWifi();
  //connectToWifiStatic();
}


void loop(){
  // Look for and handle WebSocket data
  webSocket.loop();

  // Get sensor data & update actuator statuses
  if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED){
      String data = "SENSORS,";

      // Get limit swtich status
      ceiling_slit_state = digitalRead(ceiling_slit_pin);
      if (ceiling_slit_state == 0){
        data += "OPEN,";
      }
      else{
        data += "CLOSED,";
      }


      // Get rain sensor data
      rain_sensor_state = digitalRead(rain_sensor_pin);
      // Close ceiling slits if raining
      if (rain_sensor_state == 1){    // Sensor does NOT detect rain (Opposite because of pull-up resistor)
        data += "NO RAIN,";
      }
      if (rain_sensor_state == 0 && closing_ceiling_procedure == 0 && ceiling_slit_state == 0){
        // Currently raining, close ceiling slits
        data += "RAIN,";
        Serial.println("Begin closing ceiling slits");
        digitalWrite(release_valve_pin, 1);
        lastTimeCloseCeilingSlits = millis();
        closing_ceiling_procedure = 1;    // Signal beginning to close ceiling slits
      }
      // Timer for closing ceiling slits
      else if (release_valve_state == 1 && closing_ceiling_procedure == 1){
        data += "RAIN,";
        if ((millis() - lastTimeCloseCeilingSlits) > timerDelayCloseCeilingSlits) {
          digitalWrite(release_valve_pin, 0);
          closing_ceiling_procedure = 0;    // Signal finished closing ceiling
          Serial.println("Finished closing ceiling slits");
        }
      }
      // It's raining, but the ceiling slits are already closed
      else if (rain_sensor_state == 0 && ceiling_slit_state == 1){
        data += "RAIN,";
      }
  

      // Get indoor/outside temperature sensor data
      sensors.requestTemperatures();    // Issue global temp. request to all devices on bus
      indoor_temp_state = sensors.getTempC(insideThermometer);    // Get indoor temp. sensor data
      indoor_temp_state = DallasTemperature::toFahrenheit(indoor_temp_state);    // Convert from C to F
      data += String(indoor_temp_state) + ",";  
      //data += "0,"; 

      outdoor_temp_state = sensors.getTempC(outsideThermometer);    // Get outdoor temp. sensor data
      outdoor_temp_state = DallasTemperature::toFahrenheit(outdoor_temp_state);    // Convert from C to F
      data += String(outdoor_temp_state) + ",";
      //data += "0,";

      // Bring in cooler air from outside w/ fan & ceiling slits
      if ((indoor_temp_state - outdoor_temp_state) >= (temp_differential) && fan_state == 0 && fan_state_pause == 0){
        // Turn on fan for specified amount of time then pause for specified amount of time
        Serial.println("Fan on");
        digitalWrite(fan_pin, 1);
        fan_state_cool = 1;    // Signal to begin cooling phase using fan
        lastTimeFan = millis();    // Begin timer for fan shut off then pause
        // If ceiling slits are closed & if it's not raining, then open ceiling slits
        if (ceiling_slit_state == 1 && rain_sensor_state == 1){
          Serial.println("Begin opening ceiling slits");
          digitalWrite(air_compressor_pin, 1);
          lastTimeOpenCeilingSlits = millis();
          opening_ceiling_procedure = 1;    // Signal beginning to close ceiling slits
        }
      }
      // Timer for fan shut off & pause
      if (fan_state == 1 && fan_state_pause == 0 && fan_state_cool == 1){
        if ((millis() - lastTimeFan) > timerDelayFan) {
            digitalWrite(fan_pin, 0);
            Serial.println("Fan turned off");
            Serial.println("Begin fan pause");
            fan_state_pause = 1;    // Signal to begin pause fan section
            fan_state_cool = 0;    // Signal to allow fan to be used to cool again
            lastTimeFanPause = millis();    // Begin timer for fan pause
        }
      }
      else if (fan_state == 0 && fan_state_pause == 1){
        if ((millis() - lastTimeFanPause) > timerDelayFanPause) {
            Serial.println("Fan pause over; Fan is now available for use");
            fan_state_pause = 0;    // Signal to allow fan to be used again
        }
      }
      // Timer for opening ceiling slits
      if (air_compressor_state == 1 && opening_ceiling_procedure == 1){
        if ((millis() - lastTimeOpenCeilingSlits) > timerDelayOpenCeilingSlits) {
          digitalWrite(air_compressor_pin, 0);
          opening_ceiling_procedure = 0;    // Signal finished opening ceiling
          Serial.println("Finished opening ceiling slits");
        }
      }

      

      // Get PIR sensor data
      // Will override toggle button commands
      // TODO: Test logic w/ sensor
      pir_state = digitalRead(pir_pin);
      /*
      if (pir_state == 0){
        data += "OFF,";
      }
      else{
        data += "ON,";
      }
      */
      data += "OFF,";
      // Temporarily commented out to keep testing relay
      //digitalWrite(led_pin, pir_state);


      // Get battery level data
      // TODO: Assemble/Solder sensor then write logic 
      // TODO: Test w/ sensor
      battery_sensor_state = 0;
      data += String(battery_sensor_state) + ",";

      // Send all sensor data in a single string
      webSocket.sendTXT(0, data);


      // Send status of each actuator
      int client_num = 0;    // Default to first client

      // Report state of LED
      led_state = digitalRead(led_pin);
      if (led_state == 0){
        sprintf(msg_buf, "%s", "LED_OFF");
      }
      else{
        sprintf(msg_buf, "%s", "LED_ON");
      }
      webSocket.sendTXT(client_num, msg_buf);

      // Report state of fan  
      fan_state = digitalRead(fan_pin);
      if (fan_state == 0){
        sprintf(msg_buf, "%s", "FAN_OFF");
      }
      else{
        sprintf(msg_buf, "%s", "FAN_ON");
      }
      webSocket.sendTXT(client_num, msg_buf);

      // Report state of air compressor
      air_compressor_state = digitalRead(air_compressor_pin);
      if (air_compressor_state == 0){
        sprintf(msg_buf, "%s", "AIR_COMPRESSOR_OFF");
      }
      else{
        sprintf(msg_buf, "%s", "AIR_COMPRESSOR_ON");
      }
      webSocket.sendTXT(client_num, msg_buf);

      // Report state of release valve
      release_valve_state = digitalRead(release_valve_pin);
      if (release_valve_state == 0){
        sprintf(msg_buf, "%s", "RELEASE_VALVE_OFF");
      }
      else{
        sprintf(msg_buf, "%s", "RELEASE_VALVE_ON");
      }
      webSocket.sendTXT(client_num, msg_buf);
    }

    else {
      Serial.println("WiFi Disconnected");
      // Turn everything off if WiFi is disconnected.
      digitalWrite(led_pin, LOW);    
      digitalWrite(fan_pin, LOW);
      digitalWrite(air_compressor_pin, LOW);
      digitalWrite(release_valve_pin, LOW);


      // Try to connect back to WiFi
      connectToWifiStatic();
    }

    lastTime = millis();
  }
}