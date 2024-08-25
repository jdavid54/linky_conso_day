#include <Arduino.h>
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0,/*clock*/12,/*data*/14,U8X8_PIN_NONE);

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>

// **********************************************************
// module d'affichage du cumul de conso linky par jour 
// **********************************************************

#include "credentials.h"

int test;
int local;
IPAddress ip;

#define LED 2

// mqtt
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
// time for callbacks
unsigned long then;
unsigned long now;
unsigned long interval;

// time to wait before clear screen
unsigned long previousMillis = 0;
const long wait_time = 5000;
bool cleared = false;

// time for LED blinking
unsigned long currentMillis;
int ledState = LOW;  // ledState used to set the LED
unsigned long previousblink = millis();

void connect_wifi(String ssid, String password) {
  WiFi.begin(ssid, password);
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_logisoso18_tf);
    u8g2.setCursor(0, 40);
    u8g2.drawStr(0, 40, "LINKY CONSO");
    u8g2.setFont(u8g2_font_logisoso16_tf);
    u8g2.drawStr(0, 60, "Raffin Gagny");
    Serial.print("Connecting ");
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.setCursor(0, 10);
    //u8g2.print("Wait ");
    u8g2.print(ssid.c_str());
    // affiche des points pendant la connexion
    u8g2.setCursor(0, 15);
    test = 10;
    while (WiFi.status()!= WL_CONNECTED && test>0) {
      delay(1000);
      u8g2.print(".");
      u8g2.sendBuffer();
      Serial.print(".");     
      test--;
      if (WiFi.status()== WL_CONNECTED) {
        return;
      }
    } 
  } while ( u8g2.nextPage() ); 
  
}

void callback(char* topic, byte* payload, unsigned int length) {
  now = millis();     // time at callback
  //then = time at previous callback
  // interval between 2 callbacks
  interval = now - then;
  then = millis();         // set myTime ref for next callback
  Serial.println(interval);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  digitalWrite(LED, LOW);  // LED on
  /*
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]); 
  }*/

  // first page
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("Cumul(kWh)");
    u8g2.setCursor(90, 10);
    u8g2.print(String((interval/1000.0),1));
    u8g2.print("s");
    u8g2.setFont(u8g2_font_logisoso42_tf);
    u8g2.setCursor(0, 64);
    //++value;
    //snprintf (msg, MSG_BUFFER_SIZE, "#%ld", value);

    // Copier le payload dans une chaîne de caractères
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  // Extraire la valeur numérique (exemple avec une clé "valeur")
  float valeurNumerique = atoi(message)/1000.0;
  Serial.print("Valeur numérique extraite: ");
  Serial.println(valeurNumerique,3);

  String value = (char*)payload;
  value[length]= '\0';
  //u8g2.print(value); 
  u8g2.print(String(valeurNumerique,3)); 
  } while (u8g2.nextPage());

  previousMillis = millis();
  cleared = false;
  
}

void blink() {
  // blink every second
  unsigned long currentblink = millis();
  if (currentblink - previousblink >= 500) {
    // save the last time you blinked the LED
    previousblink = currentblink;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(LED, ledState);
  }
}

void show_state() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setCursor(0, 10);
  switch (client.state()) 
  {
    case -4:
      u8g2.print("MQTT_CONNECTION_TIMEOUT");
      break;
    case -3:
      u8g2.print("MQTT_CONNECTION_LOST");
      break;
    case -2:
      u8g2.print("MQTT_CONNECT_FAILED");
      break;
    case -1:
      u8g2.print("MQTT_DISCONNECTED");
      break;
    case 1:
      u8g2.print("MQTT_CONNECT_BAD_PROTOCOL");
      break;
    case 2:
      u8g2.print("MQTT_CONNECT_BAD_CLIENT_ID");
      break;
    case 3:
      u8g2.print("MQTT_CONNECT_UNAVAILABLE");
      break;
    case 4:
      u8g2.print("MQTT_CONNECT_BAD_CREDENTIALS");
      break;
    case 5:
      u8g2.print("MQTT_CONNECT_UNAUTHORIZED");
      break;
    default:
      //u8g2.print("MQTT_CONNECTED");
      break;
  }
}

void wait_and_clear() {
  // clear screen if interval between 2 callbacks > 5s    
    currentMillis = millis();
    //Serial.print(interval);
    if ((currentMillis - previousMillis >= wait_time) && !cleared) {
      //Serial.print("clear");
      //cleared = true;
      //previousMillis = currentMillis; 
      //u8g2.clearDisplay();       // clear display
      // show interval 
      u8g2.firstPage();
      u8g2.setFont(u8g2_font_7x14_tf);
      u8g2.setCursor(0, 10);
      u8g2.print("Interval");
      u8g2.setCursor(90, 10);
      u8g2.print(String((interval/1000.0),1));
      u8g2.print("s");
      do {
        float elapsed_time = (currentMillis - previousMillis)/1000.0;
        //u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.setFont(u8g2_font_logisoso24_tf);
        u8g2.setCursor(20, 55);
        if (elapsed_time<10) {
          u8g2.print(" ");
        }
        u8g2.print(String(elapsed_time,3));
        if (!client.connected()) {
          Serial.print(client.state());
          show_state();
        }
      } while (u8g2.nextPage());  
    } 
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  // https://github.com/olikraus/u8g2/wiki/setup_tutorial
  u8g2.begin();  
  u8g2.enableUTF8Print();
  connect_wifi(ssid[selected],password[selected]);
  // show connected
  u8g2.firstPage();
  do {  
      u8g2.setCursor(0, 10);
      u8g2.print("Wifi connected!  ");  
      ip = WiFi.localIP();
      Serial.println(ip);
      u8g2.setFont(u8g2_font_logisoso16_tf);
      u8g2.setCursor(0, 55); 
      u8g2.print(ip);

  } while (u8g2.nextPage()); 
  // mqtt setup
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.connect("ESP8266Client");
  // wait
  while (!client.connected()) {} 
  // subscribe
  client.subscribe("conso_day");
  Serial.println("Subscribing MQTT ...");
  // set time ref for callback
  then = millis();
  //Serial.print(then); 
  //Serial.print(client.state());
  //show_state();
}

void loop() {
  // put your main code here, to run repeatedly:
  blink();
  // interval is computed in callback 
  if (interval>5000){        
    wait_and_clear();
  }
  
  if (!client.connected()) {
    Serial.print(client.state());
    /*
    client.connect("ESP8266Client");
    while (!client.connected()) {} 
    client.subscribe("conso_day");
    */
  }
  
  client.loop(); 
  
}
