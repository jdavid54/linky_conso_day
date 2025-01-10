#include <WiFi.h>               // For WiFi connectivity
#include <PubSubClient.h>       // MQTT client library
#include "credentials.h"        // WiFi and MQTT credentials

#include <time.h>               // For time functions
#include <sys/time.h>

// **********************************************************
// Module for detecting LED flashes on the Linky meter
// **********************************************************

// Constants and global variables
#define LED 2                   // Onboard LED for visual feedback
#define INTERRUPT_PIN 5         // GPIO 14 (D5 on ESP32 dev boards) connected to phototransistor
unsigned long lastInterruptTime = 0;
bool flag = false;
unsigned long cumul = 0;
bool first = true;              // Indicates the first pulse at midnight

WiFiClient espClient;
PubSubClient client(espClient);
static time_t now;

// Interrupt service routine (ISR)
void IRAM_ATTR handleInterrupt() {
    if (!flag) {
        flag = true;
    }
}

void showTime() {
    now = time(nullptr);  // Update 'now' with the current time
    Serial.println();
    Serial.print("Current time: ");
    Serial.print(ctime(&now));  // Print human-readable time
    Serial.println();
}

void do_it() {
    digitalWrite(LED, HIGH); // Turn LED ON

    now = time(nullptr);  // Update 'now'
    if ((localtime(&now)->tm_hour == 0) && (localtime(&now)->tm_min == 0) && first) {
        cumul = 1;       // Reset counter at midnight
        first = false;   // Prevent further resets within the first minute
    } else {
        cumul += 1;      // Increment cumulative count
    }

    if (localtime(&now)->tm_hour != 0) {
        first = true;    // Allow reset for the next midnight
    }

    unsigned long currentInterruptTime = millis();
    unsigned long elapsedTime = currentInterruptTime - lastInterruptTime;
    float power = 3600000.0 / elapsedTime;  // Power in watts
    lastInterruptTime = currentInterruptTime;

    Serial.print("Elapsed time: ");
    Serial.print(elapsedTime);
    Serial.println(" ms");
    Serial.print("Power: ");
    Serial.print(power / 1000);
    Serial.println(" kWh");

    // Publish data via MQTT
    if (client.connected()) {
        char msg[50];
        snprintf(msg, sizeof(msg), "%d", elapsedTime);
        client.publish("elapsedTime", msg);
        snprintf(msg, sizeof(msg), "%.2f", power / 1000);
        client.publish("power", msg);
        snprintf(msg, sizeof(msg), "%.3f", cumul / 1000.0);
        client.publish("cumul", msg);
    }

    digitalWrite(LED, LOW);  // Turn LED OFF
    delay(100);              // skip led parasite blink
    flag = false;             // Reset the flag
}

void setup_wifi() {
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid[selected]);

    WiFi.begin(ssid[selected], password[selected]);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
}

void setup_mqtt() {
    client.setServer(mqtt_server, 1883);
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            Serial.println(" connected");
        } else {
            Serial.print(" failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup_time() {
    configTime(3600, 0, "pool.ntp.org", "time.nist.gov");  // Set time zone and NTP servers
    now = time(nullptr);  // Get the current time
    while (now < 24 * 3600) {  // Wait until the time is set
        delay(500);
        now = time(nullptr);
    }
    Serial.println("Time synchronized");
    showTime();
}

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);   // Turn LED OFF

    pinMode(INTERRUPT_PIN, INPUT_PULLUP);  // Configure interrupt pin
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), handleInterrupt, FALLING);

    setup_wifi();   // Connect to Wi-Fi
    setup_mqtt();   // Connect to MQTT broker
    setup_time();   // Synchronize time
}

void loop() {
    if (flag) {
        do_it();  // Handle detected interrupt
    }

    if (!client.connected()) {
        setup_mqtt();  // Reconnect to MQTT if disconnected
    }
    client.loop();  // Keep MQTT connection alive
}
