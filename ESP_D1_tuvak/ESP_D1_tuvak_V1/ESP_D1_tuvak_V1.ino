#include <Arduino.h>
#include "DHTesp.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP_EEPROM.h>


/*****************************************  CONSTANTS    *******************************/

///////////////////////////////////// UNIQUE CONSTANTS FROM CLOUD //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const String SERIAL_NUMBER = "00112102af87";          // Serial number of the product
const String AP_LOGIN = "FidoElectronics27";       // Access Point (AP) mode default login
const String AP_PASS = "7a3cb094";          // AP mode default password
const String STS_LOGIN = "66ad42e1";                 // Login to access settings in AP mode 
const String STS_PASS = "d45cc8a8";                    // Password to access settings in AP mode
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
const String ROUTER_LOGIN = "DEFAULT";
const String ROUTER_PASS = "DEFAULT";
const String HIDDEN_SERVER = "http://157.230.110.88:5000";
const String END_POINT = "/tuvak/from_device";
const String SETTINGS_END_POINT = "/tuvak/device_settings";
////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////// GENERAL CONSTANTS //////////////////////////////////////////////
const int MEMORY_SIZE = 512;                   // EEPROM memory size
const int OFFSET = 0;                          // Offset to start records
const String STAMP = "FIDOELECTRONICS";        // Stamp to check if the user data was recorded
const int CHANNEL = 4;                         // parameter to set Wi-Fi channel, from 1 to 13
///////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////// PIN VALUES //////////////////////////////////////////////////////

const int moistPin = A0;
const int SAPS_BTN_PIN = 5;      // D1
const int dht11 = 4;             // D2          
const int pumpPin = 14;          // D5

/****************************************** VARIABLES *************************************/
// Authorization variables
String stamp = "";
String apLogin = "";
String apPass = "";
String stsLogin = "";
String stsPass = "";
String routerLogin = "";
String routerPass = "";

// HTTP related variables
int statusCode;
String httpResp;
String settingsHttpResp;
bool loggedIn = false;

float currTemp;
float currMoist;
float currHumid;
String waterPoured = "NO";

String slaveServer = HIDDEN_SERVER;
int moistSamples = 50;
int measureDelay = 1000;
int pumpOnDelay = 10000;
int connReistTrails = 10;     // trails to reestablish the connection with the server
int delayCounter = 0;

float sumTemp = 0;
float sumMoist = 0;

ESP8266WebServer server(80);           // starting the web server
DHTesp dht;


void setup() {
  pinMode(SAPS_BTN_PIN, INPUT);
  pinMode(moistPin, INPUT);
  pinMode(pumpPin, OUTPUT);
  dht.setup(dht11, DHTesp::DHT11);     // D2
  fidoDelay(500);

  WiFi.disconnect();
  fidoDelay(3000);
  WiFi.softAPdisconnect(true);
  fidoDelay(2000);
  loadCredentials();
  fidoDelay(2000);
  WiFi.begin(routerLogin.c_str(), routerPass.c_str());
  fidoDelay(1000);
  if (WiFi.status() != WL_CONNECTED) {
    establishConnection(connReistTrails);
  }
  if (WiFi.status() == WL_CONNECTED) {
    settingsHttpResp = sendRequest(HIDDEN_SERVER + SETTINGS_END_POINT, "{\"serial_num\":\"" + SERIAL_NUMBER + "\"}");
    deviceSettings(settingsHttpResp);
  }
}

void loop() {
  startAPAndServer();
  currMoist = getMoistureValue(moistPin, moistSamples, 10);
  fidoDelay(dht.getMinimumSamplingPeriod());
  currHumid = dht.getHumidity();
  currTemp = dht.getTemperature();
  startAPAndServer();
  
  if (WiFi.status() == WL_CONNECTED) {
    startAPAndServer();
    httpResp = sendRequest(slaveServer + END_POINT, "{\"serial_num\":\"" + SERIAL_NUMBER + "\", \"soil_moist\":" + currMoist + ", \"air_humid\":" + currHumid + ", \"temp\":" + currTemp + ", \"water_poured\":\"" + waterPoured + "\"}");

    String actionRequested = extractFromJSON(httpResp, "action_requested");
    String actionTaken = extractFromJSON(httpResp, "action_taken");

    if (actionRequested == "ON" && actionTaken == "NO") {
      digitalWrite(pumpPin, HIGH);
      fidoDelay(pumpOnDelay);
      digitalWrite(pumpPin, LOW);
      waterPoured = "YES";
    } else {
      waterPoured = "NO";
    }
  } else {
    establishConnection(connReistTrails);
  }
  fidoDelay(measureDelay);
}

float getMoistureValue(int pinNumber, int numSamples, int measDelay) {
  float sumVal = 0;
  for (int i = 0; i <= numSamples; i++) {
    sumVal += analogRead(pinNumber);
    fidoDelay(measDelay);
  }
  return sumVal / (float) numSamples;
}

void deviceSettings(String httpResp) {
  String slave_server = extractFromJSON(httpResp, "slave_server");
  String measurement_delay = extractFromJSON(httpResp, "measurement_delay");
  String conn_reist_trails = extractFromJSON(httpResp, "conn_reist_trails");
  String pump_on_delay = extractFromJSON(httpResp, "pump_on_delay");
  String moist_samples = extractFromJSON(httpResp, "moist_samples");

  if (slave_server != "" && slave_server != "-"){
    slaveServer = slave_server;
  }
  if (measurement_delay != "" && measurement_delay != "-"){
    measureDelay = measurement_delay.toInt();
  }
  if (conn_reist_trails != "" && conn_reist_trails != "-") {
    connReistTrails = conn_reist_trails.toInt();
  }
  if (pump_on_delay != "" && pump_on_delay != "-") {
    pumpOnDelay = pump_on_delay.toInt();
  }
  if (moist_samples != "" && moist_samples != "-") {
    moistSamples = moist_samples.toInt();
  }
}

void startAPAndServer() {
  int sapsCounter = 0;
  
  while(digitalRead(SAPS_BTN_PIN) == HIGH) {
    fidoDelay(1);
    sapsCounter++;
    if (sapsCounter == 4000) {
      break;
    }
  }
  if (sapsCounter == 4000) {
    setupAccessPoint();
    fidoDelay(100);
    startHTTPServer();
    fidoDelay(100);
    while (1) {
      server.handleClient();
    }
  }
}

String sendRequest(String url, String json) {
  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(json);

    if (httpCode == HTTP_CODE_OK) {
      return http.getString();
    } else {
      return String(httpCode);
    }
  }
  return "-1";
}

String extractCommand(String respContent, String cmd) {
  int st_index = respContent.indexOf(cmd);
  int end_index = respContent.indexOf("\"", st_index + 10);

  if (st_index > -1) {
    return respContent.substring(st_index + 10, end_index);
  }
  return "";
}

String extractFromJSON(String respContent, String cmd) {
  int st_index = respContent.indexOf(cmd);
  if (st_index == -1) {
    return "";
  }
  
  st_index += cmd.length() + 1;
  char lastQuoteCmd = respContent[st_index - 1];
  st_index = respContent.indexOf("\"", st_index);
  if (st_index == -1 || String(lastQuoteCmd) != "\"") {
    return "";
  }

  int end_index = respContent.indexOf("\"", st_index + 1);
  if (end_index == -1) {
    return "";
  }

  return respContent.substring(st_index + 1, end_index);
}

void fidoDelay(unsigned long duration) {
  for (unsigned long i=0; i < duration; i++) {
    delay(1);
  }
}

void establishConnection(int numTrails) {
    int trailCounter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (trailCounter < numTrails) {
        startAPAndServer();
        fidoDelay(500);
        trailCounter++;
      } else {
        break;
      }
   }
}


void setupAccessPoint() {
  bool apActivated = false;
  while (!apActivated) {
    apActivated = WiFi.softAP(apLogin, apPass, CHANNEL);
    fidoDelay(10);
  }
}

void startHTTPServer() {
  server.on("/", handleIndex);
  fidoDelay(10);
  server.on("/login", handleLogin); 
  fidoDelay(10);
  server.on("/router", handleRouter);
  fidoDelay(10);
  server.on("/restore_factory", handleFactorySettings);
  fidoDelay(10);
  server.on("/logout", handleLogout);
  server.begin();
}

void sendToLogin() {
  server.sendHeader("Location", "/login");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(301);
}

void handleIndex() {
  if (!loggedIn) {
    sendToLogin();
  }
  else {
    String postBody = server.arg("plain");
    if (postBody.length() > 0) {
      routerLogin = getFieldValue(postBody, "login");
      routerPass = getFieldValue(postBody, "password");
      if (routerLogin != "" && routerPass != "") {
        saveCredentials();
        ESP.reset();
      }
    }
    server.send(200, "text/html", indexHTML());
  }
}

void handleLogin() {
  String postBody = server.arg("plain");
  if (postBody.length() > 0) {
    String login = getFieldValue(postBody, "login");
    String password = getFieldValue(postBody, "password"); 
    if (login == stsLogin.c_str() && password == stsPass.c_str()) {
      loggedIn = true;
      server.sendHeader("Location", "/");
      server.sendHeader("Cache-Control", "no-cache");
      server.send(301);
    } else {
      server.send(200, "text/html", loginHTML());  
    }
  }
 else {
    if (loggedIn) {
      server.sendHeader("Location","/");
      server.sendHeader("Cache-Control", "no-cache");
      server.send(301);
    } else {
      server.send(200, "text/html", loginHTML());
    }
  }
}

void handleRouter() {
  if (!loggedIn) {
    sendToLogin();
  } else {
      String postBody = server.arg("plain");
      if (postBody.length() > 0) {
        routerLogin = getFieldValue(postBody, "login");
        routerPass = getFieldValue(postBody, "password");
        if (routerLogin != "" && routerPass != "") {
          saveCredentials();
          ESP.reset();
        }
      } else {
        server.send(200, "text/html", routerHTML());
      }
    }  
}

void handleFactorySettings() {
  if (!loggedIn) {
    sendToLogin();
  } else {
    stamp = STAMP;
    apLogin = AP_LOGIN;
    apPass = AP_PASS;
    stsLogin = STS_LOGIN;
    stsPass = STS_PASS;
    routerLogin = ROUTER_LOGIN;
    routerPass = ROUTER_PASS;
    saveCredentials();
    ESP.reset();
  }
}

void handleLogout() {
  if (loggedIn) {
    loggedIn = false;
  }
  sendToLogin();
}

String getFieldValue(String txt, String fieldName) {
  if (txt == "") {
    return "";
  }
  int sp = txt.indexOf(fieldName);
  if (sp == -1) {
    return "";
  }
  int ep = txt.indexOf("&", sp);
  if (ep == -1) {
    return txt.substring(sp + fieldName.length() + 1);
  }
  return txt.substring(sp + fieldName.length() + 1, ep);
}


////////////////////////// EEPROM functions///////////////////////////////////////////////////
void loadCredentials() {
  EEPROM.begin(MEMORY_SIZE);
  
  // reading stamp value
  int startPos = OFFSET;                                                        
  stamp = readFromEEPROM(startPos);
  if (stamp == STAMP) {
    // reading apLogin value
    startPos += stamp.length() + 1;
    apLogin = readFromEEPROM(startPos);          

    // reading apPass value
    startPos += apLogin.length() + 1;
    apPass = readFromEEPROM(startPos);

    // reading stsLogin value
    startPos += apPass.length() + 1;
    stsLogin = readFromEEPROM(startPos);

    // reading stsPass value
    startPos += stsLogin.length() + 1;
    stsPass = readFromEEPROM(startPos);

    // reading routerLogin value
    startPos += stsPass.length() + 1;
    routerLogin = readFromEEPROM(startPos);

    // reading routerPass value
    startPos += routerLogin.length() + 1;
    routerPass = readFromEEPROM(startPos);
    
  } else {
    // assigning default values to authorization variables
    apLogin = AP_LOGIN;
    apPass = AP_PASS;
    stsLogin = STS_LOGIN;
    stsPass = STS_PASS;

    // writing stamp
    startPos = OFFSET;
    stamp = STAMP;
    writeEEPROM(stamp, startPos);

    // writting apLogin
    startPos += stamp.length() + 1;  
    writeEEPROM(apLogin, startPos);
    
    // writting apPass
    startPos += apLogin.length() + 1;
    writeEEPROM(apPass, startPos);

    // writting stsLogin
    startPos += apPass.length() + 1;
    writeEEPROM(stsLogin, startPos);

    // writting stsPass
    startPos += stsLogin.length() + 1;
    writeEEPROM(stsPass, startPos);

    // writing routerLogin
    startPos += stsPass.length() + 1;
    writeEEPROM(routerLogin, startPos);

    // writing routerPass
    startPos += routerLogin.length() + 1;
    writeEEPROM(routerPass, startPos);
  }
  
  EEPROM.end();
}

void saveCredentials() {
    EEPROM.begin(MEMORY_SIZE);

    // writing stamp
    int startPos = OFFSET;
    stamp = STAMP;
    writeEEPROM(stamp, startPos);

    // writting apLogin
    startPos += stamp.length() + 1;  
    writeEEPROM(apLogin, startPos);
    
    // writting apPass
    startPos += apLogin.length() + 1;
    writeEEPROM(apPass, startPos);

    // writting stsLogin
    startPos += apPass.length() + 1;
    writeEEPROM(stsLogin, startPos);

    // writting stsPass
    startPos += stsLogin.length() + 1;
    writeEEPROM(stsPass, startPos);

    // writing routerLogin
    startPos += stsPass.length() + 1;
    writeEEPROM(routerLogin, startPos);

    // writing routerPass
    startPos += routerLogin.length() + 1;
    writeEEPROM(routerPass, startPos);

    EEPROM.end();
}

bool writeEEPROM(String txt, int stPos) {
  // save the text length to memory
  byte txtLen = byte(txt.length());
  EEPROM.put(stPos, txtLen);

  // save the text to memeory
  for (int i = 1; i <= txtLen; i++) {
    EEPROM.put(stPos + i, char(txt[i-1]));
    fidoDelay(10);
  }

  return EEPROM.commitReset();
}

String readFromEEPROM(int stPos) {
  byte fieldLen;
  char ch;
  String result;

  // reading the field length from the memory
  EEPROM.get(stPos, fieldLen);
  // reading the field from the memory
  for (int i = stPos + 1; i <= stPos + fieldLen; i++) {
    EEPROM.get(i, ch);
    if (String(ch) == "+") {
      result += " ";
    }else {
      result += ch;
    }
  }
  
  return replaceASCIICode(result);
}

String replaceASCIICode(String txt) {
  String result;
  int idx = 0;

  while (idx < txt.length()) {
    if (txt[idx] == '%') {
      String numString = "0x" + String(txt[idx+1]) + String(txt[idx+2]);
      char ch = (char)(int)strtol(numString.c_str(), NULL, 16);
      result += ch;
      idx += 3;
    } 
    else {
      result += txt[idx];
      idx += 1;
    }
  }
  
  return result;
}

//////////////////////// HTML content functions /////////////////////////////////////
String css() {
    String content = "<style>";
    content += ".fido-electronics {";
    content += "text-align : center;";
    content += "}";
    
    content += "input[type=text], input[type=password] {";
    content += "width: 100%;";
    content += "padding: 12px 15px;";
    content += "margin: 8px 0;";
    content += "display: inline-block;";
    content += "border: 1px solid #ccc;";
    content += "box-sizing: border-box;";
    content += "outline: none;";
    content += "}";

    content += "input[type=submit] {";
    content += "background-color: #374a62;";
    content += "color: white;";
    content += "padding: 14px 20px;";
    content += "margin: 8px 0;";
    content += "border: 2px solid #374a62;";
    content += "cursor: pointer;";
    content += "width: 100%;";
    content += "}";

    content += "input[type=submit]:hover {";
    content += "background-color: #232F3E;";
    content += "transition: all .3s;";
    content += "}";

    content += ".container {";
    content += "width: 25%;";
    content += "margin: auto;";
    content += "padding: 16px;";
    content += "border: 1px solid #ccc;";
    content += "border-radius : 3px;";
    content += "}";

    content += "@media screen and (max-width: 1400px) {";
    content += ".container {";
    content += "width:30%";
    content += "}";
    content += "}";

    content += "@media screen and (max-width: 1100px) {";
    content += ".container {";
    content += "width:40%";
    content += "}";
    content += "}";
    
    content += "@media screen and (max-width: 600px) {";
    content += ".container {";
    content += "width:85%";
    content += "}";
    content += "}";

    content += ".index-ul {";
    content += "list-style-type: none;";
    content += "text-align: center;";
    content += "border: none;";
    content += "}";

    content += ".index-ul li{";
    content += "margin: 5px;";
    content += "}";

    content += ".index-ul a{";
    content += "display: block;";
    content += "padding: 10px;";
    content += "background-color: #374a62;";
    content += "color: white;";
    content += "text-decoration: none;";
    content += "font-size: 1.2em;";
    content += "}";
    
    content += "</style>";
    return content;
}

String javaScript() {
    String content = "<script>";
    content += "</script>";
    return content;
}

String headerHTML() {
    String content = "<head>";
    content += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    content += "<meta charset=\"UTF-8\">";
    content += "<title>Fido Electronics</title>";
    content += "</head>";
    return content;
}

String indexHTML() {
    String content = "<!DOCTYPE HTML>";
    content += "<html>";
    content += headerHTML();
    content += css();
    content += "<body>";
    content += "<h1 class = 'fido-electronics'>Fido Electronics</h1>";
    content += "<ul class= 'container index-ul'>";
    content += "<li><a href=\"/router\">????????????</a></li>";
    content += "<li><a href=\"/restore_factory\">???????????????????????? ?????????????????? ??????????????????</a></li>";
    content += "<li><a href=\"/logout\">??????????</a></li>";
    content += "</ul>";
    content += javaScript();
    content += "</body>";
    content += "</html>";
    return content;
}

String formHTML(String formName, String submitName) {
    String content = "<!DOCTYPE HTML>";
    content += "<html>";
    content += headerHTML();
    content += css();
    content += "<body>";
    content += "<h1 class = 'fido-electronics'>Fido Electronics</h1>";
    content += "<div class=\"container\">";
    content += "<h2>" + formName + "</h2>";
    content += "<div style='line-height:2em' id='listSsid'>";
    content += "</div>";
    content += "<form method=\"POST\">";
    content += "<label for=\"login\">????????????</label>";
    content += "<input type=\"text\" name=\"login\">";
    content += "<label for=\"password\">????????????</label>";
    content += "<input type=\"password\" name=\"password\">";
    content += "<input type=\"submit\" value=\"" + submitName + "\">";
    content += "</form>";
    content += "</div>";
    content += javaScript();
    content += "</body>";
    content += "</html>";
    return content;  
}

String loginHTML() {
  return formHTML("??????????","??????????");
}

String routerHTML() {
  return formHTML("?????????????????? ??????????????", "??????????????????");
}
