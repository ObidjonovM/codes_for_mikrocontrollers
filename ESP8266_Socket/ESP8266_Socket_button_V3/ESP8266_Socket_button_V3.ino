#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP_EEPROM.h>


/*****************************************  CONSTANTS    *******************************/

///////////////////////////////////// UNIQUE CONSTANTS FROM CLOUD //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const String SERIAL_NUMBER = "000121f9df3e";          // Serial number of the product
const String AP_LOGIN = "FidoElectronics4";       // Access Point (AP) mode default login
const String AP_PASS = "737d7f7d";                // AP mode default password
const String STS_LOGIN = "db453e5f";                 // Login to access settings in AP mode 
const String STS_PASS = "63a71354";                    // Password to access settings in AP mode
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
const String ROUTER_LOGIN = "DEFAULT";
const String ROUTER_PASS = "DEFAULT";
const String HIDDEN_SERVER = "http://157.230.110.88:5000";
const String END_POINT = "/socket_button/from_device";
const String SETTINGS_END_POINT = "/socket_button/device_settings";
////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////// GENERAL CONSTANTS //////////////////////////////////////////////
const int MEMORY_SIZE = 512;                   // EEPROM memory size
const int OFFSET = 0;                          // Offset to start records
const String STAMP = "FIDOELECTRONICS";        // Stamp to check if the user data was recorded
const String MEASURE_STAMP = "MEASUREMENTS";        // Stamp to check if the user data was recorded
const int CHANNEL = 4;                         // parameter to set Wi-Fi channel, from 1 to 13
///////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////// PIN VALUES //////////////////////////////////////////////////////
const int LED_PIN = 0;            // GPIO0
const int REL_PIN = 2;            // GPIO2
const int BTN_PIN = 3;            // GPIO3

/****************************************** VARIABLES *************************************/
// Authorization variables
String stamp = "";
String measureStamp = "";
String apLogin = "";
String apPass = "";
String stsLogin = "";
String stsPass = "";
String routerLogin = "";
String routerPass = "";
String slaveServer = "";
int measureDelay;
int connReistTrails;
int minDelay;
int numDelays;

// HTTP related variables
int statusCode;
String httpResp;
String settingsHttpResp;
bool loggedIn = false;
bool mustWriteEEPROM = false;
bool wasConnected = false;

String relay = "OFF";
int btnState = false;
int prevBtnState = false;
int delayCounter = 0;


ESP8266WebServer server(80);           // starting the web server

void setup() {
  pinMode(REL_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(REL_PIN, LOW);
  fidoDelay(500);

  WiFi.disconnect();
  fidoDelay(3000);
  WiFi.softAPdisconnect(true);
  fidoDelay(2000);
  loadCredentials();
  fidoDelay(2000);
  WiFi.begin(routerLogin.c_str(), routerPass.c_str());
  fidoDelay(2000);
}

void loop() {
  resetbtn();
  if (measureStamp == MEASURE_STAMP) {
    if (delayCounter == numDelays) {
      delayCounter = 0;
      if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_PIN, HIGH);
        resetbtn();
        getFromDevice();
        if (!wasConnected) {
          getDeviceSettings();
          if (mustWriteEEPROM) {
            saveCredentials();
            mustWriteEEPROM = false;
            ESP.reset();
          }
          wasConnected = true;
        }
      }else {
        wasConnected = false;
        digitalWrite(LED_PIN, LOW);
        establishConnection(connReistTrails);
      }
      fidoDelay(measureDelay);
    }
    resetbtn();
    
    fidoDelay(minDelay);
    delayCounter += 1;
  }else {
    if (WiFi.status() == WL_CONNECTED) {
      getDeviceSettings();
      measureStamp = MEASURE_STAMP;
      saveCredentials();
      ESP.reset();
    }
  }
}

void getFromDevice() {
  httpResp = sendRequest(slaveServer + END_POINT, "{\"serial_num\":\"" + SERIAL_NUMBER + "\", \"state\":\"" + relay + "\","+
  +" \"slave_server\":\"" + slaveServer + "\", \"measurement_delay\":\"" + measureDelay + "\", "+
  +" \"conn_reist_trails\":\"" + connReistTrails + "\", \"min_delay\":\"" + minDelay + "\", \"num_delays\":\"" + numDelays + "\"}");
  resetbtn();
  String actionRequested = extractFromJSON(httpResp, "action_requested");
  String actionTaken = extractFromJSON(httpResp, "action_taken");
  String resetRequested = extractFromJSON(httpResp, "reset_requested");
  String resetTaken = extractFromJSON(httpResp, "reset_taken");

  if (actionRequested == "ON" && actionTaken == "NO") {
    digitalWrite(REL_PIN, HIGH);
    relay = "ON";
  }
  if (actionRequested == "OFF" && actionTaken == "NO") {
    digitalWrite(REL_PIN, LOW);
    relay = "OFF";
  }
  if (resetRequested == "ON" && resetTaken == "NO") {
    ESP.reset();
  }
}

void getDeviceSettings() {
  settingsHttpResp = sendRequest(HIDDEN_SERVER + SETTINGS_END_POINT, "{\"serial_num\":\"" + SERIAL_NUMBER + "\"}");
  deviceSettings(settingsHttpResp);
}

void deviceSettings(String httpResp) {
  String slave_server = extractFromJSON(httpResp, "slave_server");
  String measurement_delay = extractFromJSON(httpResp, "measurement_delay");
  String conn_reist_trails = extractFromJSON(httpResp, "conn_reist_trails");
  String min_delay = extractFromJSON(httpResp, "min_delay");
  String num_delays = extractFromJSON(httpResp, "num_delays");
  if (slave_server != "" && slave_server != slaveServer){
    slaveServer = slave_server;
    mustWriteEEPROM = true;
  }
  if (measurement_delay != "" && measurement_delay.toInt() != measureDelay){
    measureDelay = measurement_delay.toInt();
    mustWriteEEPROM = true;
  }
  if (conn_reist_trails != "" && conn_reist_trails.toInt() != connReistTrails) {
    connReistTrails = conn_reist_trails.toInt();
    mustWriteEEPROM = true;
  }
  if (min_delay != "" && min_delay.toInt() != minDelay) {
    minDelay = min_delay.toInt();
    mustWriteEEPROM = true;
  }
  if (num_delays != "" && num_delays.toInt() != numDelays) {
    numDelays = num_delays.toInt();
    mustWriteEEPROM = true;
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
  WiFi.begin(routerLogin.c_str(), routerPass.c_str());
  int trailCounter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    resetbtn();
    if (trailCounter < numTrails) {
      fidoDelay(50);
      trailCounter++;
    } else {
      break;
    }
  }
}

void resetbtn(){
  int sapsCounter = 0;
  btnState = digitalRead(BTN_PIN);   // HIGH if button is pressed
  if (btnState == HIGH) {
    while(btnState == HIGH) {
      fidoDelay(1);
      btnState = digitalRead(BTN_PIN);
      sapsCounter++;
      if (sapsCounter == 7000) {
        break;
      }
    }
    if (sapsCounter == 7000) {
      startAPAndServer();
    }else {
      if (relay == "ON") {
        digitalWrite(REL_PIN, LOW);
        relay = "OFF";
      } else {
        digitalWrite(REL_PIN, HIGH);
        relay = "ON";
      }
    }
  }
}

void startAPAndServer() {
  setupAccessPoint();
  fidoDelay(100);
  startHTTPServer();
  fidoDelay(100);
  while (1) {
    server.handleClient();
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
    return replaceASCIICode(txt.substring(sp + fieldName.length() + 1));
  }
  return replaceASCIICode(txt.substring(sp + fieldName.length() + 1, ep));
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
    }else if (txt[idx] == '+'){
      result += " ";
      idx += 1;
    }
    else {
      result += txt[idx];
      idx += 1;
    }
  }
  
  return result;
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

    // reading measureStamp value
    startPos += stsPass.length() + 1;
    measureStamp = readFromEEPROM(startPos);
    
    if (measureStamp == MEASURE_STAMP) {
      // reading slaveServer value
      startPos += measureStamp.length() + 1;
      slaveServer = readFromEEPROM(startPos);
  
      // reading measureDelay value
      startPos += slaveServer.length() + 1;
      String measure_delay = readFromEEPROM(startPos);
      measureDelay = measure_delay.toInt();
  
      // reading connReistTrails value
      startPos += measure_delay.length() + 1;
      String conn_reist_trails = readFromEEPROM(startPos);
      connReistTrails = conn_reist_trails.toInt();
  
      // reading minDelay value
      startPos += conn_reist_trails.length() + 1;
      String min_delay = readFromEEPROM(startPos);
      minDelay = min_delay.toInt();
  
      // reading numDelays value
      startPos += min_delay.length() + 1;
      String num_delays = readFromEEPROM(startPos);
      numDelays = num_delays.toInt();

      // reading routerLogin value
      startPos += num_delays.length() + 1;
      routerLogin = readFromEEPROM(startPos);
    }else {
      // reading routerLogin value
      routerLogin = readFromEEPROM(startPos);
    }
    // reading routerPass value
    startPos += routerLogin.length() + 1;
    routerPass = readFromEEPROM(startPos);
    
    EEPROM.end();
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

    EEPROM.end();
    ESP.reset();
  }

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

    if (measureStamp == MEASURE_STAMP) {
      // writting measureStamp
      startPos += stsPass.length() + 1;
      writeEEPROM(measureStamp, startPos);
      
      // writting slaveServer
      startPos += measureStamp.length() + 1;
      writeEEPROM(slaveServer, startPos);
  
      // writting measureDelay
      startPos += slaveServer.length() + 1;
      String measure_delay = String(measureDelay);
      writeEEPROM(measure_delay, startPos);
  
      // writting connReistTrails
      startPos += measure_delay.length() + 1;
      String conn_reist_trails = String(connReistTrails);
      writeEEPROM(conn_reist_trails, startPos);
  
      // writting minDelay
      startPos += conn_reist_trails.length() + 1;
      String min_delay = String(minDelay);
      writeEEPROM(min_delay, startPos);
  
      // writting numDelays
      startPos += min_delay.length() + 1;
      String num_delays = String(numDelays);
      writeEEPROM(num_delays, startPos);

      // writing routerLogin
      startPos += num_delays.length() + 1;
      writeEEPROM(routerLogin, startPos);
    }else {
      // writing routerLogin
      startPos += stsPass.length() + 1;
      writeEEPROM(routerLogin, startPos);
    }

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
    result += ch;
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
    content += "<li><a href=\"/router\">Роутер</a></li>";
    content += "<li><a href=\"/restore_factory\">Восстановить заводские настройки</a></li>";
    content += "<li><a href=\"/logout\">Выйти</a></li>";
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
    content += "<label for=\"login\">Логинь</label>";
    content += "<input type=\"text\" name=\"login\">";
    content += "<label for=\"password\">Пароль</label>";
    content += "<input id = \"password\" type=\"password\" name=\"password\">";
    content += "<input type=\"submit\" value=\"" + submitName + "\">";
    content += "</form>";
    content += "</div>";
    content += javaScript();
    content += "</body>";
    content += "</html>";
    return content;  
}

String loginHTML() {
  return formHTML("ВОЙТИ","Войти");
}

String routerHTML() {
  return formHTML("Настройки роутера", "Сохранить");
}
