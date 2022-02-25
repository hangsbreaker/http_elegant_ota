#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <EEPROM.h>
// install library DoubleResetDetector 1.0.3
#include <DoubleResetDetector.h>
// install library ArduinoJson versi 5
#include <ArduinoJson.h>
#include "FS.h"
 
#include <time.h>

#include <CertStoreBearSSL.h>
BearSSL::CertStore certStore;
 
AsyncWebServer server(80);
 
 
#define FIRMWARE_VERSION "0.1"

#define EEPROM_SIZE 1
#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

String updateServer = "";
int updateServerPort = 443;

const char* wifiSSID = "ThingkerBell";
const char* wifiPASS = "temanbaik";

const char* APSSID = "";
const char* APPSK  = "";

int getupdate = 0, sysreboot = 0;

// start function ================================================================================================================
// Set time via NTP, as required for x.509 validation
time_t setClock() {
  configTime(2*3600, 0, "pool.ntp.org", "time.nist.gov");
 
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
  return now;
}
 
void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}
 
void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}
 
void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}
 
void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void updateota();
bool loadConfig();
bool saveConfig(char* apssid="", char* appsk="", char* ssid="", char* password="", String updateServer="", int updateServerPort=0);

// function webpage
void ICACHE_FLASH_ATTR handleRoot(AsyncWebServerRequest *request);
void ICACHE_FLASH_ATTR handleConWifi(AsyncWebServerRequest *request);
void ICACHE_FLASH_ATTR handleDisConWifi(AsyncWebServerRequest *request);
void ICACHE_FLASH_ATTR handleConfig(AsyncWebServerRequest *request);
void ICACHE_FLASH_ATTR handleReboot(AsyncWebServerRequest *request);
void ICACHE_FLASH_ATTR handleServer(AsyncWebServerRequest *request);
void ICACHE_FLASH_ATTR handleSpot(AsyncWebServerRequest *request);
void ICACHE_FLASH_ATTR handleGetUpdate(AsyncWebServerRequest *request);

// end function ================================================================================================================

// START SETUP ================================================================================================================
void setup(void) {
  Serial.begin(115200);
  
  Serial.println();
  Serial.println();
  
  EEPROM.begin(EEPROM_SIZE);
  if(!SPIFFS.begin()){
    Serial.println("Failed to mount file system");
    return;
  }
 
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  
  WiFi.mode(WIFI_AP_STA);
  //WiFi.begin(APSSID, APPSK);
  Serial.println("");

  // if connect to wifi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(APSSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // setClock();
  }

  
  Serial.println("Configuring access point...");
  if(!loadConfig()){
    Serial.println("Failed to load config");
    WiFi.softAP(wifiSSID, wifiPASS); // Provide the (SSID, password);
  }else{
    Serial.println("Config loaded");
    WiFi.softAP(wifiSSID, wifiPASS); // Provide the (SSID, password);
    if(WiFi.status() != WL_CONNECTED && (String)APSSID!="" && (String)APPSK!=""){
      WiFi.begin(APSSID, APPSK); // connect to wifi
    }
  }
  IPAddress Ip(192, 168, 99, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);


  // initial server ==========  
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA, "admin", "admin"
  
  server.onNotFound([](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", "Not found");
    request->send(response);
  });
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/x-icon");
  });
  server.on("/wifi", HTTP_GET, handleConWifi);
  server.on("/wifi_disconnect", HTTP_GET, handleDisConWifi);
  
  server.on("/server", HTTP_GET, handleServer);
  server.on("/getupdate", HTTP_GET, handleGetUpdate);

  server.on("/spot", HTTP_GET, handleSpot);
  
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/reboot", HTTP_GET, handleReboot);
  server.begin();
  
  Serial.println("HTTP server started");
  
  

  
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    digitalWrite(LED_BUILTIN, LOW);delay(200);
    digitalWrite(LED_BUILTIN, HIGH);delay(500);
    digitalWrite(LED_BUILTIN, LOW);delay(500);
    digitalWrite(LED_BUILTIN, HIGH);delay(500);
    digitalWrite(LED_BUILTIN, LOW);delay(200);
    digitalWrite(LED_BUILTIN, HIGH);delay(100);
    digitalWrite(LED_BUILTIN, LOW);delay(100);
    digitalWrite(LED_BUILTIN, HIGH);delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
// END SETUP ================================================================================================================

// LOOP ================================================================================================================
void loop(void) {
  
  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {

    if(getupdate == 1){
      getupdate = 0;
      updateota();
    }
    if(sysreboot == 1){
      sysreboot = 0;
      delay(1500);
      ESP.reset();
    }
  }
  
  drd.loop();
}
// END LOOP ================================================================================================================

// WEBSITE PAGE =====================================================================================================

void ICACHE_FLASH_ATTR handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "Hi.");
}


void ICACHE_FLASH_ATTR handleConWifi(AsyncWebServerRequest *request){
  String htmlpage="";
  if (WiFi.status() == WL_CONNECTED) {
    htmlpage="Connected to "+WiFi.SSID()+"<br>IP Address: "+WiFi.localIP().toString()+"<br><a href='/wifi_disconnect'>Disconnect</a>";
    request->send(200, "text/html", htmlpage);
    return;
  }
  // If the POST request doesn't have username and password data
  if(request->hasParam("apssid") && request->hasParam("appsk")){
    
    if((request->getParam("apssid")->value() == NULL || request->getParam("appsk")->value() == NULL) && (WiFi.status() != WL_CONNECTED)) {
      request->send(200, "text/html", "Invalid Request");
      return;
    }
  
    WiFi.disconnect();
    
    delay(500);
    char* wfssid=new char[request->getParam("apssid")->value().length()+1];
    char* wfpass=new char[request->getParam("appsk")->value().length()+1];
    
    request->getParam("apssid")->value().toCharArray(wfssid, request->getParam("apssid")->value().length()+1);
    request->getParam("appsk")->value().toCharArray(wfpass, request->getParam("appsk")->value().length()+1);
    
    APSSID=wfssid;
    APPSK=wfpass;
  
    // save to config.json
    saveConfig(wfssid, wfpass);
    
    // Koneksi ke akses point
    WiFi.begin(APSSID, APPSK); // connect to wifi
    Serial.println("Connecting to Access Point "+(String)APSSID+"...");
    
    int ntrycon=0;
    for(ntrycon=0;ntrycon<=18;ntrycon++){
      // Tunggu sampai terkoneksi
      if(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }else{
        break;
      }
    }
  
    Serial.print("\nFailed, try again.");
    if(WiFi.status() != WL_CONNECTED) {
      //server.send(200, "text/html", "Connection Failed. Try again.");
      htmlpage="Connection Failed. Try again.";
      request->send(200, "text/html", htmlpage);
      return;
    }
  
    IPAddress myIPWifi = WiFi.localIP();
    Serial.println("\n");
    Serial.print("WiFi Connected\nIP Address: ");
    Serial.println(myIPWifi);  //Print the local IP
  
    htmlpage="Connected to "+WiFi.SSID()+"<br>IP Address: "+myIPWifi.toString();
    request->send(200, "text/html", htmlpage);
    return;
  }else{
      request->send(200, "text/html", "Wifi Not Connected");
      return;
  }
}

void ICACHE_FLASH_ATTR handleDisConWifi(AsyncWebServerRequest *request){
  // save to config.json
  saveConfig("-", "-");
  WiFi.disconnect();
  delay(700);
  request->send(200, "text/html", "Wifi Disconnected<br><a href='/wifi'>Back</a>");
}

void ICACHE_FLASH_ATTR handleConfig(AsyncWebServerRequest *request){
  loadConfig();
  request->send(SPIFFS, "/config.json", "application/json");
}

void ICACHE_FLASH_ATTR handleReboot(AsyncWebServerRequest *request){
  sysreboot = 1;
  request->send(200, "text/html", "System reboot");
}

void ICACHE_FLASH_ATTR handleServer(AsyncWebServerRequest *request){

  // If the POST request doesn't have username and password data
  if(request->hasParam("url") || request->hasParam("port")){    

    if(request->hasParam("url")){
      if(request->getParam("url")->value() != NULL){
        char* urlsrv=new char[request->getParam("url")->value().length()+1];
        request->getParam("url")->value().toCharArray(urlsrv, request->getParam("url")->value().length()+1);
        
        updateServer = (String)urlsrv;
        saveConfig("", "", "", "", updateServer, 0);
      }
    }
    if(request->hasParam("port")){
      if(request->getParam("port")->value() != NULL){
        char* portsrv=new char[request->getParam("port")->value().length()+1];
        request->getParam("port")->value().toCharArray(portsrv, request->getParam("port")->value().length()+1);
      
        updateServerPort = atoi(portsrv);
        saveConfig("", "", "", "", "", updateServerPort);
      }
    }

  } else if(request->hasParam("del")){
    updateServer = "";
    updateServerPort = 443;
    saveConfig("", "", "", "", "-", 443);
  }
  
  String htmlpage = "<table>";
         
         htmlpage += "<tr><td>Update URL Server</td><td>&nbsp;:&nbsp;&nbsp;</td><td>"+(String)updateServer+"</td></tr>";
         htmlpage += "<tr><td>Update Port Server</td><td>&nbsp;:&nbsp;&nbsp;</td><td>"+(String)updateServerPort+"</td></tr>";
         
         htmlpage += "</table>";
         
  request->send(200, "text/html", htmlpage);
}


void ICACHE_FLASH_ATTR handleSpot(AsyncWebServerRequest *request){

  String inf = "";
  int stsid=0, stspwd=0;
  // If the POST request doesn't have username and password data
  if(request->hasParam("ssid") || request->hasParam("password")){

    if(request->hasParam("ssid")){
      if(request->getParam("ssid")->value() != NULL){
        char* ssid=new char[request->getParam("ssid")->value().length()+1];
        request->getParam("ssid")->value().toCharArray(ssid, request->getParam("ssid")->value().length()+1);
        
        wifiSSID = ssid;
        saveConfig("", "", strdup(wifiSSID), "", "", 0);
        stsid=1;
      }
    }else{
      stsid=1;
    }
    
    if(request->hasParam("password")){
      if(request->getParam("password")->value() != NULL){
        if(request->getParam("password")->value().length()>=8){
          char* password=new char[request->getParam("password")->value().length()+1];
          request->getParam("password")->value().toCharArray(password, request->getParam("password")->value().length()+1);
        
          wifiPASS = password;
          saveConfig("", "", "", strdup(wifiPASS), "", 0);
          stspwd=1;
        }else{
          inf = "Password minimum 8 digits<br>";
        }
      }
    }else{
      stspwd=1;
    }

  } else if(request->hasParam("del")){
    wifiSSID = "ThingkerBell";
    wifiPASS = "temanbaik";
    saveConfig("", "", strdup(wifiSSID), strdup(wifiPASS), "", 0);
    
    stsid=1;
    stspwd=1;
  }

  if(stsid==1 && stspwd==1){
    inf = "Changed <a href='/reboot'>reboot now</a><br>";
  }
  
  String htmlpage = inf + "<table>";
         
         htmlpage += "<tr><td>SSID</td><td>&nbsp;:&nbsp;&nbsp;</td><td>"+String(wifiSSID)+"</td></tr>";
         htmlpage += "<tr><td>Password</td><td>&nbsp;:&nbsp;&nbsp;</td><td>"+String(wifiPASS)+"</td></tr>";
         
         htmlpage += "</table>";
         
  request->send(200, "text/html", htmlpage);
}

void ICACHE_FLASH_ATTR handleGetUpdate(AsyncWebServerRequest *request){
  if((String)updateServer != "" && (String)updateServer != ""){
    getupdate = 1;
    request->send(200, "text/html", "Get update "+String(updateServer)+":"+String(updateServerPort)+"/update/file.bin");
  }else{
    request->send(200, "text/html", "URL / Port Server not exists");
  }
}
// END WEBSITE PAGE =====================================================================================================

// config.json =====================================================================================================
bool loadConfig(){
  // read config ======================================================
  File configFile = SPIFFS.open("/config.json", "r");
  if(!configFile){
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if(size > 1024){
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if(!json.success()){
    Serial.println("Failed to parse config file");
    return false;
  }
  // end read config ======================================================
  
  const char* ussid= json["ssid"];
  const char* upass= json["password"];
  
  const char* userv= json["updateServer"];
  const int usport= json["updateServerPort"];
  
  const char* apssid= json["apssid"];
  const char* appsk= json["appsk"];

  
  if((String)ussid!=""){
    char* wfassid=new char[String(ussid).length()+1];
    String(ussid).toCharArray(wfassid, String(ussid).length()+1);
    
    wifiSSID = wfassid;
  }
  if((String)upass!=""){
    char* wfapass=new char[String(upass).length()+1];
    String(upass).toCharArray(wfapass, String(upass).length()+1);
    
    wifiPASS = wfapass;
  }
  
  if((String)userv!=""){
    updateServer= (String)userv;
  }
  if(usport!=0){
    updateServerPort= usport;
  }
  
  if((String)apssid!=""){
    
    char* wfssid=new char[String(apssid).length()+1];
    String(apssid).toCharArray(wfssid, String(apssid).length()+1);
    
    APSSID = wfssid;
  }
  if((String)appsk!=""){
    
    char* wfpass=new char[String(appsk).length()+1];
    String(appsk).toCharArray(wfpass, String(appsk).length()+1);
    
    APPSK = wfpass;
  }
  // Real world application would store these values in some variables for later use.

  Serial.print("Loaded wifiSSID: ");
  Serial.println(wifiSSID);
  Serial.print("Loaded wifiPASS: ");
  Serial.println(wifiPASS);
  
  Serial.print("Update urlserver: ");
  Serial.println(updateServer);
  Serial.print("Update urlport: ");
  Serial.println(updateServerPort);
  
  Serial.print("AP SSID: ");
  Serial.println(APSSID);
  Serial.print("AP PSK: ");
  Serial.println(APPSK);
  return true;
}


bool saveConfig(char* apssid, char* appsk, char* ssid, char* password, String updateServer, int updateServerPort){
  // read config ======================================================
  File configFile = SPIFFS.open("/config.json", "r");
  if(!configFile){
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if(size > 1024){
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());
  
  if(!json.success()){
    Serial.println("Failed to parse config file");
    return false;
  }
  // end read config ======================================================

  
  const char* jussid= json["ssid"];
  const char* jupass= json["password"];
  
  const char* juserv= json["updateServer"];
  const int jusport= json["updateServerPort"];
  
  const char* japssid= json["apssid"];
  const char* jappsk= json["appsk"];

  Serial.println("JSON Config");
  
  Serial.print("wifiSSID: ");
  Serial.println((String)jussid);
  Serial.print("wifiPASS: ");
  Serial.println((String)jupass);
  
  Serial.print("Update urlserver: ");
  Serial.println((String)juserv);
  Serial.print("Update urlport: ");
  Serial.println((String)jusport);
  
  Serial.print("AP SSID: ");
  Serial.println((String)japssid);
  Serial.print("AP PSK: ");
  Serial.println((String)jappsk);
  
  if((String)apssid != ""){
    if((String)apssid == "-"){
      json["apssid"] = "";
    }else{
      json["apssid"] = apssid;
    }
  }
  if((String)appsk != ""){
    if((String)appsk == "-"){
      json["appsk"] = "";
    }else{
      json["appsk"] = appsk;
    }
  }
  
  if((String)ssid != ""){
    json["ssid"] = ssid;
  }
  if((String)password != ""){
    json["password"] = password;
  }
  
  if((String)updateServer != ""){
    if((String)updateServer == "-"){
      json["updateServer"] = "";
    }else{
      json["updateServer"] = updateServer;
    }
  }
  if(updateServerPort > 0){
    json["updateServerPort"] = updateServerPort;
  }

  File configFiles = SPIFFS.open("/config.json", "w");
  if (!configFiles) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFiles);
  return true;
}

void updateota(){
  
    if(WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      
      setClock();
       
      client.setInsecure();
      client.connect(updateServer, updateServerPort);
   
   
      // The line below is optional. It can be used to blink the LED on the board during flashing
      // The LED will be on during download of one buffer of data from the network. The LED will
      // be off during writing that buffer to flash
      // On a good connection the LED should flash regularly. On a bad connection the LED will be
      // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
      // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
      ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
   
      // Add optional callback notifiers
      ESPhttpUpdate.onStart(update_started);
      ESPhttpUpdate.onEnd(update_finished);
      ESPhttpUpdate.onProgress(update_progress);
      ESPhttpUpdate.onError(update_error);
   
      ESPhttpUpdate.rebootOnUpdate(false); // remove automatic update
   
      Serial.println("Update start now!");
   
       t_httpUpdate_return ret = ESPhttpUpdate.update(client, String(updateServer)+":"+String(updateServerPort)+"/update/file.bin", FIRMWARE_VERSION);
   
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          Serial.println(F("Retry in 10secs!"));
          delay(10000); // Wait 10secs
          break;
   
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          Serial.println("Your code is up to date!");
            delay(10000); // Wait 10secs
          break;
   
        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          delay(1000); // Wait a second and restart
          ESP.restart();
          break;
      }
    }
}
