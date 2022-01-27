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

    content += ".parent-eye-icon {";
    content += "position : relative;";
    content += "}";

    content += ".hide {";
    content += "display : none;";
    content += "}";

    content += ".eye-icon {";
    content += "position : absolute;";
    content += "right : 0px;";
    content += "height : 38%;";
    content += "padding : 18px;";
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
    content += "";
    content += "var iconBlack = document.getElementById('iconBlack');\n";
    content += "var iconBlue = document.getElementById('iconBlue');\n";
    content += "var password = document.getElementById('password');\n";
    content += "function iconBlackFunction(){\n";
    content += "password.type = 'text';";
    content += "password.tabIndex = 1;";
    content += "iconBlack.style.display = 'none';";
    content += "iconBlue.style.display = 'inline-block';";
    content += "};\n";
    content += "function iconBlueFunction(){\n";
    content += "password.type = 'password';";
    content += "password.tabIndex = 1;";
    content += "iconBlue.style.display = 'none';";
    content += "iconBlack.style.display = 'inline-block';";
    content += "};";
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
    content += "<div class = \"parent-eye-icon\">";
    content += "<input id = \"password\" type=\"password\" name=\"password\">";
    content += "<img id = \"iconBlack\" class = \"eye-icon\" onclick = \"iconBlackFunction()\" src = \"data:image/png;base64,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\">";
    content += "<img id = \"iconBlue\" class = \"eye-icon hide\" onclick = \"iconBlueFunction()\" src=\"data:image/png;base64,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\">";
    content += "</div>";
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
