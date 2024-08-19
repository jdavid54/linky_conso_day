#include "ESP8266WiFi.h"
#include "PubSubClient.h"
#include "credentials.h"

#include <TZ.h>

#define MYTZ  TZ_Europe_Paris
#include <coredecls.h>                  // settimeofday_cb()
#include <PolledTimeout.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval


// **********************************************************
// module de détection de flash led sur compteur linky
// **********************************************************

// this line is necessary, not sure what it does
extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);

// An object which can store a time
static time_t now;


// this uses the PolledTimeout library to allow an action to be performed every 5 seconds
// static esp8266::polledTimeout::periodicMs showTimeNow(5000);

// This is a shortcut to print a bunch of stuff to the serial port.  
// It's very confusing, but shows what values are available
#define PTM(w) \
  Serial.print(" " #w "="); \
  Serial.print(tm->tm_##w);

void printTm(const char* what, const tm* tm) {
  Serial.print(what);
  PTM(isdst); PTM(yday); PTM(wday);
  PTM(year);  PTM(mon);  PTM(mday);
  PTM(hour);  PTM(min);  PTM(sec);
  Serial.println();
}

/*
// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 1;     // Central European Time

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
*/

//static const uint8_t LED_BUILTIN = 8;
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE	(50)
#define LED 2

// Define the interrupt pin
const int interruptPin = 14;  // GPIO 14 sur pin D5 relié au  photo transistor côté émetteur à la masse et à la résistance 47kohm au 3V 
unsigned long lastInterruptTime = 0;
bool flag = false;
unsigned long cumul = 0;
bool first = true;  // first pulse at midnight

// Define the interrupt service routine
void ICACHE_RAM_ATTR handleInterrupt() {
//void IRAM_ATT handleInterrupt() {
  if (!flag) {flag=true;}
}

void showTime() {   // This function is used to print stuff to the serial port, it's not mandatory
  now = time(nullptr);      // Updates the 'now' variable to the current time value

  // human readable
  Serial.println();
  Serial.print("ctime:     ");
  Serial.print(ctime(&now));
  // Here is one example showing how you can get the current month
  Serial.print("current month: ");
  Serial.println(localtime(&now)->tm_mon + 1);
  // Here is another example showing how you can get the current year
  Serial.print("current year: ");
  Serial.println(localtime(&now)->tm_year + 1900);
  // Look in the printTM method to see other data that is available
  Serial.println();
  printTm("Time", localtime(&now));
  Serial.println();
}

void time_is_set_scheduled() {    // This function is set as the callback when time data is retrieved
  // In this case we will print the new time to serial port, so the user can see it change (from 1970)
  showTime();
}


/*
La LED orange correspond à un indicateur de consommation.
Chaque clignotement représente la consommation d’1 Wh d’électricité.
De fait, plus vous consommez d’électricité, plus le voyant clignote rapidement ; 
*/

// Define the interrupt service routine
void do_it() {
  digitalWrite(LED, LOW); // LED ON

  now = time(nullptr);      // Updates the 'now' variable to the current time value
  printTm("Time", localtime(&now));
  
  if ((localtime(&now)->tm_hour == 0) && (localtime(&now)->tm_min == 0) && (first))
  {
    cumul = 1;       // reset counter
    first = false;   // inhibit reset in the first minute for others pulses
  }
  else cumul += 1;

  if (localtime(&now)->tm_hour != 0) first= true; // reset for next midnight

  Serial.println();
  Serial.print("Cumul:");
  Serial.println(cumul);
  //localtime(&now)->tm_sec

  // Calculate the time elapsed since the last interrupt
  unsigned long currentInterruptTime = millis();
  // Serial.println(currentInterruptTime);
  unsigned long elapsedTime = currentInterruptTime - lastInterruptTime;
  

  float power = 3600000/elapsedTime;
  // Store the current interrupt time as the last interrupt time
  lastInterruptTime = currentInterruptTime;

  // This code will be executed when the interrupt pin changes from low to high (rising edge)
  // Add your desired actions or code here
  // For example, you can print the elapsed time
  Serial.print("Interrupt triggered! Elapsed time: ");
  Serial.print(elapsedTime);
  Serial.println(" ms");
  Serial.print(power/1000);
  Serial.println(" kWh");
  Serial.print(24*power/1000);
  Serial.println(" kWh by day");
  Serial.print((float) cumul/1000);
  Serial.println(" kWh");
  showTime();
  //Serial.println(digitalRead(interruptPin));
  // Once connected, publish an announcement...
  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  if (client.connect(clientId.c_str())) {
      //Serial.println("connected");
      char msg[MSG_BUFFER_SIZE];
      //snprintf (msg, MSG_BUFFER_SIZE, "Elapsed time:%d ms, Power:%.2f kWh",elapsedTime,power/1000);
      snprintf (msg, MSG_BUFFER_SIZE, "%d", elapsedTime);
      Serial.println(msg);
      client.publish("elapsedTime", msg);
      snprintf (msg, MSG_BUFFER_SIZE, "%.2f", power/1000);
      Serial.println(msg);
      client.publish("power", msg);
      snprintf (msg, MSG_BUFFER_SIZE, "%.3f", cumul/1000.0);
      Serial.println(msg);
      client.publish("cumul", msg);
      // beep if interval <= 1sec
      snprintf (msg, MSG_BUFFER_SIZE, "%s", "E");
      if (elapsedTime <= 1000) client.publish("beep", msg);
  }
  delay(300); //anti rebond
  flag = false;
  digitalWrite(LED, HIGH);  // LED OFF
  //delay(500);
  
}


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid[selected]);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid[selected],password[selected]);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED, LOW);
    delay(100);
    Serial.print(".");
    digitalWrite(LED, HIGH);
    delay(100);
  }
}

void setup_mqtt() {
  digitalWrite(LED, LOW);
  // Loop until we're reconnected
  client.setServer(mqtt_server, 1883);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println(" connected");
    }
  }
  digitalWrite(LED, HIGH);
}

void setup() {
  // Initialize the serial communication for debugging
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);   // LED OFF
  setup_wifi();
  // MQTT connection
  setup_mqtt();


  // Configure the interrupt pin as an input
  pinMode(interruptPin, INPUT);

  // Attach the interrupt handler function to the interrupt pin
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
  
  /*
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  */

  // install callback - called when settimeofday is called (by SNTP or us)
  // once enabled (by DHCP), SNTP is updated every hour
  // settimeofday_cb(time_is_set_scheduled);

  // This is where your time zone is set
  configTime(MYTZ, "pool.ntp.org");
  now = time(nullptr);      // Updates the 'now' variable to the current time value
  printTm("Time", localtime(&now));


}

// time_t prevDisplay = 0; // when the digital clock was displayed

void loop() {
  // Your main code goes here
  // This code will be executed continuously while the interrupt is enabled
  //Serial.println(flag);
  //Serial.println(digitalRead(interruptPin));
  if (flag) {do_it();}
  //delay(1000);
  
  //if (showTimeNow) {
  //  showTime();
  //}
  
  
  /*
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }
  */

  //printTm("Time", localtime(&now));
}

/*
void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

//-------- NTP code ----------

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
*/