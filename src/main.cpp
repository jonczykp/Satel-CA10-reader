#include <LittleFS.h>    

#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#else
#include <WiFi.h>
#include <AsyncTCP.h>
#endif

#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>

#include "wificonfig.h"

AsyncWebServer server(80);
DNSServer dnsServer;
static String indexHTMLPage;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

#define INITWIFINAME "SATELCA_WIFI"

String mqttBroker, mqttUser, mqttPassword;

const String mqttTopic = "satel/ca10/";
const String mqttConfTopic = "homeassistant/binary_sensor/satelca10/";

// Define the pins we're using on your ESP board
#define DATA_PIN D5
#define CLK_PIN D1

#define DATA_FRAME_SIZE 32                          //define data frame size for Satel CA10, extend if you want to ecperiment with other devices and different frame lenght

// Declare global variables to hold the data and clock states, interrupt times in microseconds, bit count
volatile bool data_result[DATA_FRAME_SIZE];         // Current message
volatile bool clk_state = false;                    // Current clock state HIGH or LOW
volatile bool prev_clk_state = false;               // Previous current clock state HIGH or LOW
volatile long long interrupt_time = 0;              // Current interrupt's time in microseconds
volatile long long last_interrupt_time = 0;         // Previous interrupt's time in microseconds
volatile bool data_ready = false;                   // Flag for marking when receiving the message ended, complete and the data is ready for use
volatile uint32_t bit_count = DATA_FRAME_SIZE + 10; // Counter for the number of bits received from the message

bool prev_message[DATA_FRAME_SIZE];             // previous message
bool cur_message[DATA_FRAME_SIZE];              // current message for processing, copy of data_result but not impacted when reading new data

class SatelLed{
  public:
    int FrameBit;
    String Name;
    String DeviceClass;
    SatelLed(){
     FrameBit=-1;
    };
    SatelLed( int framebit, String name, String deviceclass ){
      FrameBit=framebit;
      Name = name;
      DeviceClass = deviceclass;
    };
};

/* 
      31, 29, 27, 25, 15, 13, 11, 9,       //sensors - frame bits which reprent led 1 to 8 on manipulator
      19                                   //malfunction led 
      21                                   //zone1 activate bit in frame
      17                                   //zone2 activate bit in frame
      23                                   //alram LED bit in frame
*/

SatelLed *SatelCA10[] = {
  new SatelLed(31,"LED01", "moving"), new SatelLed(29,"LED02", "moving"), new SatelLed(27,"LED03", "moving"),
  new SatelLed(25,"LED04", "moving"), new SatelLed(15,"LED05", "moving"), new SatelLed(13,"LED06", "moving"),
  new SatelLed(11,"LED07", "moving"), new SatelLed( 9,"LED08", "moving"), 
  new SatelLed(), new SatelLed(), new SatelLed(), new SatelLed(),        // add more sesors/LED if needed but you need to find proper bit in frame
  new SatelLed(), new SatelLed(), new SatelLed(), new SatelLed(),        // empty space left here to keep LED numbering on mqtt (sensors at the begining)
  new SatelLed(19,"Warrning", "problem"),
  new SatelLed(21,"Zone1", "running"),
  new SatelLed(17,"Zone2", "running"),
  new SatelLed(23,"Alarm", "sound"),
  new SatelLed()
};

unsigned long state_refresh_time = 1000000;

// Define the interrupt service routine for the clock pin
void IRAM_ATTR clk_isr() {
  
  interrupt_time = micros();

  // Consider new message start only if it has been longer than 2000 ms since the last interrupt but not for the first read
  if ((interrupt_time - last_interrupt_time > 2000) && (last_interrupt_time != 0)) {
    bit_count = 0;
    data_ready = false;
  }

  last_interrupt_time = interrupt_time;

  // Read the state of the CLK pin but store the previous state first
  prev_clk_state = clk_state;
  clk_state = digitalRead(CLK_PIN);

  // If the CLK pin is on the same level just wait for new with time 2000 Ms and restart transmission
  if ((prev_clk_state == clk_state) && (bit_count > 0)) {
    bit_count = DATA_FRAME_SIZE + 10;
    data_ready = false;
  }
  else
  {
    // Store the new bit from the DATA pin
    if (bit_count < DATA_FRAME_SIZE) {
      // Wait for 100 microseconds to give the DATA pin time to stabilize and avoid the result of the pullup resistor at clock change
      delayMicroseconds(100);
      data_result[bit_count] = digitalRead(DATA_PIN);
    }

    bit_count = bit_count + 1;

    // When bit count reaches FRAME_SIZE mark the data ready
    if (bit_count == DATA_FRAME_SIZE)
      data_ready = true;
    }
}


void SatelCA10DiscoveryConfig()
{
    if (!mqttClient.connected())
          mqttClient.connect(mqttTopic.c_str() , mqttUser.c_str(), mqttPassword.c_str());
    
    int n = sizeof(SatelCA10) / sizeof(SatelCA10[0]);
    for (int i = 0; i < n; i++) 
      if (SatelCA10[i]->FrameBit != -1)
      {
        String message = "{\"name\":\""+ SatelCA10[i]->Name + "\",\"device_class\":\"" + SatelCA10[i]->DeviceClass + "\",\"state_topic\":\"" + mqttTopic + "LED" + String(i+1) + "\",\
\"object_id\":\"Satel_"+ SatelCA10[i]->Name + "\",\"unique_id\":\"Satel_"+ SatelCA10[i]->Name + "\",\
\"device\":{\"identifiers\":[\"satel_ca10\"],\"name\":\"Satel CA10\"}}";
        
        mqttClient.publish ( (mqttConfTopic + SatelCA10[i]->Name +"/config").c_str(), message.c_str(), true);
      }
}   



void setup() {

  Serial.begin(115200);

  if (LittleFS.begin()) {
    Serial.println("File system mounted");
    ListAllFilesInDir("/");
    
    if ( !LittleFS.exists(CFGFILENAME) )      //no config file
      SaveWiFiConfigFile(INITWIFINAME);
    
    File cfgFile = LittleFS.open(CFGFILENAME, "r");
    String str = cfgFile.readString();
    cfgFile.close();

    JsonDocument jsonBuffer;
    DeserializationError error = deserializeJson(jsonBuffer, str);

    if (!error) {
      indexHTMLPage = IndexHTMLPage(String(jsonBuffer["wifiAP"]), String(jsonBuffer["wifiPASS"]), String(jsonBuffer["wifiHOSTNAME"]), 
                                    String(jsonBuffer["mqttSERVER"]), jsonBuffer["mqttPORT"], 
                                    String(jsonBuffer["mqttUSER"]), String(jsonBuffer["mqttPASS"]));

      Serial.println("Trying to connect to WIFI network: " + String(jsonBuffer["wifiAP"]));
      //try to connect
      WiFi.begin(String(jsonBuffer["wifiAP"]), String(jsonBuffer["wifiPASS"]));
      WiFi.setHostname(String(jsonBuffer["wifiHOSTNAME"]).c_str());
      
      int r = 0, timeout = ( String(jsonBuffer["wifiAP"]) == INITWIFINAME? 10 : 150 ); //150 for power cut t restart WiFI router
      
      while ((WiFi.status() != WL_CONNECTED) && (r++ < timeout )) {
        Serial.print(".");     // failed, retry
        delay(1000);
      }
      if (WiFi.status() != WL_CONNECTED){
        Serial.println("\nYou cannot connect to WiFi network: " + String(jsonBuffer["wifiAP"]) + ", establishing WIFI AP (" + INITWIFINAME  + ") to configure the network");
        
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(INITWIFINAME);

        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", WiFi.softAPIP());
        
        Serial.print ("AP IP address: ");
        Serial.println(WiFi.softAPIP());
        Serial.println ("AP SSID: " + String(WiFi.softAPSSID()));
      }
      else {
        Serial.print(String("\nYou are connected to the WiFI network: ") + String(WiFi.SSID()) + String(", IP: ") );
        Serial.print(WiFi.localIP().toString());
        Serial.println(String(", hostname: ") + String(WiFi.hostname()) );

        Serial.println("\nCLK pin: " + String(CLK_PIN) + ", DATA pin: " + String(DATA_PIN));
        
        mqttBroker = String(jsonBuffer["mqttSERVER"]);
        mqttUser = String(jsonBuffer["mqttUSER"]);
        mqttPassword =String(jsonBuffer["mqttPASS"]);

        mqttClient.setServer(mqttBroker.c_str(), jsonBuffer["mqttPORT"]);
        //mqttClient.setServer(mqttBroker, mqttPort);

        mqttClient.connect(mqttTopic.c_str(), mqttUser.c_str(), mqttPassword.c_str() );
        if (!mqttClient.connected()){
          Serial.println("MQTT connection failed!");
          Serial.println(mqttClient.state());
        }
        else {
          Serial.println("You're connected to the MQTT broker!");
          indexHTMLPage = IndexHTMLPage(String(jsonBuffer["wifiAP"]), String(jsonBuffer["wifiPASS"]), String(jsonBuffer["wifiHOSTNAME"]), 
                                    String(jsonBuffer["mqttSERVER"]), jsonBuffer["mqttPORT"], String(jsonBuffer["mqttUSER"]), String(jsonBuffer["mqttPASS"]), "connected, authenticated" );

          // Set the DATA and CLK pins as inputs
          pinMode(DATA_PIN, INPUT);
          pinMode(CLK_PIN, INPUT);

          SatelCA10DiscoveryConfig();

          // Enable interrupts on the CLK pin
          attachInterrupt(digitalPinToInterrupt(CLK_PIN), clk_isr, CHANGE);  
          // end of successful setup      
    
        }
      }

      // Web Server Root URL
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        //request->send(LittleFS, SETUPHTLMPAGE , "text/html");
        request->send_P(200 , "text/html", indexHTMLPage.c_str());
        Serial.println("Web Client connected!");
      });
 
      server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
        int params = request->params();
        static String ssid, pass, hostname, mqttserver, mqttuser, mqttpass;
        static int mqttport;
        for(int i=0;i<params;i++){
          AsyncWebParameter* p = request->getParam(i);
          if(p->isPost()){
            // reading POST form values
            if (p->name() == "ssid") ssid = p->value().c_str();
            if (p->name() == "pass") pass = p->value().c_str();
            if (p->name() == "hostname") hostname = p->value().c_str();
            if (p->name() == "mqttserver") mqttserver = p->value().c_str();
            if (p->name() == "mqttport") mqttport = p->value().toInt();
            if (p->name() == "mqttuser") mqttuser = p->value().c_str();
            if (p->name() == "mqttpass") mqttpass = p->value().c_str();
            //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
          }
        }
        SaveWiFiConfigFile(ssid, pass,hostname, mqttserver, mqttport, mqttuser, mqttpass);
        
        delay(1000);
        Serial.println("Restarting ESP");
        ESP.restart();
      });
      server.begin();
    }   
    //end of no error
  } //end of LittleFS.begin()
}   //end of setup


void loop()
{
  while (data_ready != true)
    delay(1);

  bool new_data_flag = false;                       // Flag if the message changed from the previous one
  for (int i = 0; i < DATA_FRAME_SIZE; i++) {
    cur_message[i] = data_result[i];
    if (prev_message[i] != cur_message[i])
      new_data_flag = true;
  }

  if (new_data_flag == true) {
    /*
    // Create a JSON document and add the data result array to it, // Send the JSON string to the MQTT broker -- section commented for deplyment, uncomment if you need to evaluate bits fo new satel device or you want to look for new leds
    String RawDataMessage = "[", ChangesMessage = "";
    for (int i = 0; i < DATA_FRAME_SIZE; i++) {
      RawDataMessage += String(cur_message[i]) + ((i < (DATA_FRAME_SIZE - 1)) ? "," : "]");
      if (cur_message[i] != prev_message[i]) // find changed bits
        ChangesMessage += String(i) + ",";
    }

    unsigned long long NumMessgae = 0, tmp; //conver binary message  to int
    for (int i = 0; i < DATA_FRAME_SIZE; i++) {
        tmp = cur_message[i];
        NumMessgae |= tmp << (DATA_FRAME_SIZE - i - 1);
    }

    char hex_string[30];     //conver int message  to Hex
    sprintf(hex_string, "%llX", NumMessgae);

    Serial.print("Sent to MQTT (");
    Serial.print(mqttClient.connected());
    Serial.print("): ");   
    Serial.println(RawDataMessage + ", " + hex_string + ", bit: " + ChangesMessage);
    */

    if (!mqttClient.connected())
       mqttClient.connect(mqttTopic.c_str() , mqttUser.c_str(), mqttPassword.c_str());

    // Send the JSON string to the MQTT broker -- section commented for deplyment, uncomment if you need to evaluate bits fo new satel device or you want to look for new leds
    /*
    mqttClient.publish( (mqttTopic + "raw_data").c_str() , RawDataMessage.c_str());
    mqttClient.publish( (mqttTopic + "change").c_str(), ChangesMessage.c_str());
    */
    
    // send messages for particular sensors LED if state has been changed
    int n = sizeof(SatelCA10) / sizeof(SatelCA10[0]);
    for (int i = 0; i < n; i++) 
      if (SatelCA10[i]->FrameBit != -1)
        if  (prev_message[SatelCA10[i]->FrameBit] != cur_message[SatelCA10[i]->FrameBit]) 
          mqttClient.publish( (mqttTopic + "LED" + String(i+1)).c_str(),  (cur_message[SatelCA10[i]->FrameBit] == true) ? "ON" : "OFF" );

        
    for (int i = 0; i < DATA_FRAME_SIZE; i++) 
      prev_message[i] = cur_message[i];
  }
  
  
  //refresh all sensors every 3 min, sometimes relay needs it, millis function is set to 0 after 49 days of running thus seconf condition
  unsigned long cur_time = millis();
  if ( (cur_time > state_refresh_time ? cur_time-state_refresh_time : state_refresh_time-cur_time) > 180000  ) {      
    int n = sizeof(SatelCA10) / sizeof(SatelCA10[0]);

    for (int i = 0; i < n; i++) 
      if (SatelCA10[i]->FrameBit != -1)
        mqttClient.publish( (mqttTopic + "LED" + String(i+1)).c_str(),  (cur_message[SatelCA10[i]->FrameBit] == true) ? "ON" : "OFF" );
    
    SatelCA10DiscoveryConfig();
    state_refresh_time = cur_time;
    
  }
  
  delay(1500); // Wait a little bit, at least sensor state change is hold a little bit longer
}
