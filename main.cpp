#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DHTesp.h>


#define             EEPROM_LENGTH 1024
#define             RESET_PIN 0

HTTPClient http;
WiFiClient Client;

DHTesp        dht;
int           interval = 2000; 
unsigned long lastDHTReadMillis = 0;
float         humidity = 0;
float         temperature = 0;

ESP8266WebServer    webServer(80);
char                eRead[30];
char                ssid[30] = "iptime";
char                password[30] = "jhs2031%";  
char                influxdb[90] = "http://52.7.186.144:8086/write?db=mydb"; //influxdb 

String responseHTML = ""
    "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body><center>"
    "<p>Captive Sample Server App</p>"
    "<form action='/save'>"
    "<p><input type='text' name='ssid' placeholder='SSID' onblur='this.value=removeSpaces(this.value);'></p>"
    "<p><input type='text' name='password' placeholder='WLAN Password'></p>"
    "<p><input type='submit' value='Submit'></p></form>"
    "<p>This is a captive portal example</p></center></body>"
    "<script>function removeSpaces(string) {"
    "   return string.split(' ').join('');"
    "}</script></html>";

// Saves string to EEPROM
void SaveString(int startAt, const char* id) { 
    for (byte i = 0; i <= strlen(id); i++) {
        EEPROM.write(i + startAt, (uint8_t) id[i]);
    }
    EEPROM.commit();
}

// Reads string from EEPROM
void ReadString(byte startAt, byte bufor) {
    for (byte i = 0; i <= bufor; i++) {
        eRead[i] = (char)EEPROM.read(i + startAt);
    }
}

void save(){
    Serial.println("button pressed");
    Serial.println(webServer.arg("ssid"));
    SaveString( 0, (webServer.arg("ssid")).c_str());
    SaveString(30, (webServer.arg("password")).c_str());
    webServer.send(200, "text/plain", "OK");
    ESP.restart();
}

void configWiFi() {
    const byte DNS_PORT = 53;
    IPAddress apIP(192, 168, 1, 1);
    DNSServer dnsServer;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("CaptivePortal");     // change this to your portal SSID
    
    dnsServer.start(DNS_PORT, "*", apIP);

    webServer.on("/save", save);

    webServer.onNotFound([]() {
        webServer.send(200, "text/html", responseHTML);
    });
    webServer.begin();
    while(true) {
        dnsServer.processNextRequest();
        webServer.handleClient();
        yield();
    }
}

void load_config_wifi() {
    ReadString(0, 30);
    if (!strcmp(eRead, "")) {
        Serial.println("Config Captive Portal started");
        configWiFi();
    } else {
        Serial.println("IOT Device started");
        strcpy(ssid, eRead);
        ReadString(30, 30);
        strcpy(password, eRead);
    }
}

IRAM_ATTR void GPIO0() {
    SaveString(0, ""); // blank out the SSID field in EEPROM
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    EEPROM.begin(EEPROM_LENGTH);
    pinMode(RESET_PIN, INPUT_PULLUP);
    attachInterrupt(RESET_PIN, GPIO0, FALLING);
    while(!Serial);
    Serial.println();

    //load_config_wifi(); // load or config wifi if not configured
   
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if(i++ > 15) {
            configWiFi();
        }
    }
    Serial.print("Connected to "); Serial.println(ssid);
    Serial.print("IP address: "); Serial.println(WiFi.localIP());

    if (MDNS.begin("YourNameHere")) {
       Serial.println("MDNS responder started");
    }

    http.begin(Client, influxdb);

    dht.setup(14, DHTesp::DHT22);
    delay(1000);
    Serial.println(); Serial.println("Humidity%\tTemperature'C");
    Serial.println("Runtime Starting");  
}

void loop() {
    MDNS.update();

    unsigned long currentMillis = millis();
    
    if(currentMillis - lastDHTReadMillis >= interval) {
        lastDHTReadMillis = currentMillis;

        humidity = dht.getHumidity();
        temperature = dht.getTemperature();
        
        
    }
    //Serial.printf("%.1f\t %.1f\n", humidity, temperature);

    http.addHeader("Content-Type", "text/plain");
    char post_str[80];
    sprintf(post_str, "temparture,host=server01,region=us-west temperature=%f,humidity=%f", temperature, humidity);
    int httpCode = http.POST(post_str);
    String payload = http.getString();
    Serial.println(httpCode);
    Serial.println(payload);
    http.end();

    delay(1000);
}
