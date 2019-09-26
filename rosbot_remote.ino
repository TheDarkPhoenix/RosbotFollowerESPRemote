#include <ros.h>
#include <std_msgs/Bool.h>
#include <WiFi.h>
#include <Husarnet.h>
#include <WiFiMulti.h>
#define PUB_FREQ 4 // frequency of publishing data

uint16_t port = 11411; //this must be set the same as tcp_port in launch file (esp_connect.launch)

// Timer: Auxiliary variables
unsigned long now = millis();
unsigned long lastTrigger = 0;

// Husarnet credentials
const char* hostNameESP = "******"; //this will be the name of the 1st ESP32 device at https://app.husarnet.com
const char* hostNameComputer = "******"; //this will be the name of the host/rosmaster device at https://app.husarnet.com
/* to get your join code go to https://app.husarnet.com
  -> select network
  -> click "Add element"
  -> select "join code" tab
  Keep it secret!
*/

const char* husarnetJoinCode = "fc94:b01d:1803:8dd8:b293:5c7d:7639:932a/******";
// WiFi credentials
#define NUM_NETWORKS 2 //number of Wi-Fi network credentials saved
const char* ssidTab[NUM_NETWORKS] = {
  "******",
  "******",
};
const char* passwordTab[NUM_NETWORKS] = {
  "******",
  "******",
};

WiFiMulti wifiMulti;
HusarnetClient client;

class WiFiHardware {
  public:
    WiFiHardware() {};
    void init() {
      // do your initialization here. this probably includes TCP server/client setup
      Serial.printf("WiFiHardware: init, hostname = %s, port = %d\r\n", hostNameComputer, port);
      while (! client.connect(hostNameComputer, port)) {
        Serial.printf("Waiting for connection\r\n");
        delay(500);
      }
    }
    // read a byte from the serial port. -1 = failure
    int read() {
      // implement this method so that it reads a byte from the TCP connection and returns it
      // you may return -1 is there is an error; for example if the TCP connection is not open
      return client.read(); //will return -1 when it will works
    }
    // write data to the connection to ROS
    void write(uint8_t* data, int length) {
      // implement this so that it takes the arguments and writes or prints them to the TCP connection
      for (int i = 0; i < length; i++) {
        client.write(data[i]);
      }
    }
    // returns milliseconds since start of program
    unsigned long time() {
      return millis(); // easy; did this one for you
    }
};

ros::NodeHandle_<WiFiHardware> nh;
std_msgs::Bool buttonState;
ros::Publisher esp_publisher("/esp_remote/start", &buttonState);
std_msgs::Bool configButtonState;
ros::Publisher config_publisher("/esp_remote/calibrate", &configButtonState);


void taskWifi( void * parameter );

const int ledPin = 2; 
const int startButton = 15;
const int stopButton = 4;

void messageCb( const std_msgs::Bool& led_msg){
  if (led_msg.data == true)
    digitalWrite(ledPin, HIGH);
  else
    digitalWrite(ledPin, LOW);
}
ros::Subscriber<std_msgs::Bool> led_sub("/esp_remote/led_signal", &messageCb );

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(startButton, INPUT);
  pinMode(stopButton, INPUT);
  
  for (int i = 0; i < NUM_NETWORKS; i++) {
    String ssid = ssidTab[i];
    String pass = passwordTab[i];
    wifiMulti.addAP(ssid.c_str(), pass.c_str());
    Serial.printf("WiFi %d: SSID: \"%s\" ; PASS: \"%s\"\r\n", i, ssid.c_str(), pass.c_str());
  }
  xTaskCreate(
    taskWifi, /* Task function. */
    "taskWifi", /* String with name of task. */
    10000, /* Stack size in bytes. */
    NULL, /* Parameter passed as input of the task */
    1, /* Priority of the task. */
    NULL); /* Task handle. */
  Serial.begin(115200);
  nh.initNode();
  nh.advertise(esp_publisher);
  nh.advertise(config_publisher);
  nh.subscribe(led_sub);
}

void loop() {
  int startStatus = digitalRead(startButton);
  int stopStatus = digitalRead(stopButton);
  
  now = millis();
  if ((now - lastTrigger) > (1000 / PUB_FREQ)) {
    if (client.connected()) {
      Serial.println("Publishing Data");
      
      buttonState.data = (startStatus==HIGH);
      esp_publisher.publish( &buttonState );
      
      configButtonState.data = (stopStatus==HIGH);
      config_publisher.publish( &configButtonState );
      
      lastTrigger = millis();
      nh.spinOnce();
    }
    else {
      while (! client.connect(hostNameComputer, port)) {
        Serial.printf("Waiting for connection\r\n");
        digitalWrite(ledPin, LOW);
        delay(250);
        digitalWrite(ledPin, HIGH);
        delay(250);
      }
    }
  }
}

void taskWifi( void * parameter ) {
  while (1) {
    uint8_t stat = wifiMulti.run();
    if (stat == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      Husarnet.join(husarnetJoinCode, hostNameESP);
      Husarnet.start();
      while (WiFi.status() == WL_CONNECTED) {
        delay(500);
      }
    } else {
      Serial.printf("WiFi error: %d\r\n", (int)stat);
      delay(500);
    }
  }
}
