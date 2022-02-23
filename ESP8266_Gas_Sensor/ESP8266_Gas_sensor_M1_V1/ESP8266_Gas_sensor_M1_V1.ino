#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP_EEPROM.h>


/*****************************************  CONSTANTS    *******************************/

///////////////////////////////////// UNIQUE CONSTANTS FROM CLOUD //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const String SERIAL_NUMBER = "0003213e4080";          // Serial number of the product
const String AP_LOGIN = "FidoElectronics14";       // Access Point (AP) mode default login
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
const String ROUTER_PASS = "DEFAULT";
const String HIDDEN_SERVER = "http://157.230.110.88:5000";
const String END_POINT = "/gas_sensor/from_device";
const String SETTINGS_END_POINT = "/gas_sensor/device_settings";
////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////// GENERAL CONSTANTS //////////////////////////////////////////////
const int MEMORY_SIZE = 512;                   // EEPROM memory size
const int OFFSET = 0;                          // Offset to start records
const String STAMP = "FIDOELECTRONICS";        // Stamp to check if the user data was recorded
const String MEASURE_STAMP = "MEASUREMENTS";
const int CHANNEL = 4;                         // parameter to set Wi-Fi channel, from 1 to 13
///////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////// PIN VALUES //////////////////////////////////////////////////////
const int GAS_PIN = A0;
const int LED_PIN = 5;          // D1
const int BTN_PIN = 4;          // D2
const int BUZZ_PIN = 14;        // D5
const int REL_PIN = 12;         // D6
const int SAPS_BTN_PIN = 13;    // D7

/****************************************** VARIABLES *************************************/
// Authorization variables
String stamp = "";
String measureStamp = "";
String apLogin = "";
String routerLogin = "";
String routerPass = "";
String slaveServer = "";
float maxGasVal;
int measureDelay;
int buzzDuration;
int buzzInterval;
int connReistTrails;
int R_0;

// HTTP related variables
int statusCode;
String httpResp;
String ssidList;
String settingsHttpResp;
bool loggedIn = false;
bool mustWriteEEPROM = false;
bool wasConnected = false;

String relay = "OFF";
bool gasOn = false;
float gasVal = 0;
int delayCounter = 0;


ESP8266WebServer server(80);           // starting the web server

void setup() {
  pinMode(REL_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(SAPS_BTN_PIN, INPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(REL_PIN, LOW);
  digitalWrite(BUZZ_PIN, LOW);
  fidoDelay(1000);

  WiFi.disconnect();
  fidoDelay(3000);
  WiFi.softAPdisconnect(true);
  fidoDelay(3000);
  loadCredentials();
  fidoDelay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(routerLogin.c_str(), routerPass.c_str());
  fidoDelay(290000);                                           //5 minut dan keyin qurilma ishga tushadi
}

void loop() {
  startAPAndServer();
  if (measureStamp == MEASURE_STAMP) {
    gasSensorLogic();
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_PIN, HIGH);
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
    } else {
      wasConnected = false;
      digitalWrite(LED_PIN, LOW);
      establishConnection(connReistTrails);
    }
    fidoDelay(measureDelay);
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      getDeviceSettings();
      if (mustWriteEEPROM == true) {
        measureStamp = MEASURE_STAMP;
        saveCredentials();
        ESP.reset();
      }
    }
  }
}

float getMethanePPM() {
  float a0 = analogRead(GAS_PIN); // get raw reading from sensor
  float v_o = a0 * 5 / 1023; // convert reading to volts
  float R_S = (5 - v_o) * 1000 / v_o; // apply formula for getting RS
  float PPM;
  if (R_S < 0) {
    R_S = 0;
    PPM = 2147483647;
  } else {
    PPM = pow(R_S / R_0, -2.95) * 1000; //apply formula for getting PPM
  }
  return PPM; // return PPM value to calling function
}

void getFromDevice() {
  httpResp = sendRequest(slaveServer + END_POINT, "{\"serial_num\":\"" + SERIAL_NUMBER + "\", \"state\":\"" + relay + "\"," +
                         +" \"gas_val\":\"" + gasVal + "\", \"max_gas_value\":\"" + maxGasVal + "\", \"slave_server\":\"" + slaveServer + "\", \"measurement_delay\":\"" + measureDelay + "\"," +
                         +" \"conn_reist_trails\":\"" + connReistTrails + "\", \"buzz_duration\":\"" + buzzDuration + "\", \"buzz_interval\":\"" + buzzInterval + "\", \"R_0\":\"" + R_0 + "\"}");

  String resetRequested = extractFromJSON(httpResp, "reset_requested");
  String resetTaken = extractFromJSON(httpResp, "reset_taken");

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
  String max_gas_value = extractFromJSON(httpResp, "max_gas_value");
  String measurement_delay = extractFromJSON(httpResp, "measurement_delay");
  String buzz_duration = extractFromJSON(httpResp, "buzz_duration");
  String buzz_interval = extractFromJSON(httpResp, "buzz_interval");
  String conn_reist_trails = extractFromJSON(httpResp, "conn_reist_trails");
  String r_0 = extractFromJSON(httpResp, "r_0");
  if (slave_server != "" && slave_server != slaveServer) {
    slaveServer = slave_server;
  }
  if (max_gas_value != "" && max_gas_value.toFloat() != maxGasVal) {
    maxGasVal = max_gas_value.toFloat();
    mustWriteEEPROM = true;
  }
  if (measurement_delay != "" && measurement_delay.toInt() != measureDelay) {
    measureDelay = measurement_delay.toInt();
    mustWriteEEPROM = true;
  }
  if (buzz_duration != "" && buzz_duration.toInt() != buzzDuration) {
    buzzDuration = buzz_duration.toInt();
    mustWriteEEPROM = true;
  }
  if (buzz_interval != "" && buzz_interval.toInt() != buzzInterval) {
    buzzInterval = buzz_interval.toInt();
    mustWriteEEPROM = true;
  }
  if (conn_reist_trails != "" && conn_reist_trails.toInt() != connReistTrails) {
    connReistTrails = conn_reist_trails.toInt();
    mustWriteEEPROM = true;
  }
  if (r_0 != "" && r_0.toInt() != R_0) {
    R_0 = r_0.toInt();
    mustWriteEEPROM = true;
  }
}

void startAPAndServer() {
  int sapsCounter = 0;

  while (digitalRead(SAPS_BTN_PIN) == HIGH) {
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

void gasSensorLogic() {
  gasVal = getMethanePPM();
  if (gasOn == true) {
    if (gasVal >= maxGasVal) {
      digitalWrite(REL_PIN, LOW);
      gasOn = false;
      relay = "OFF";
    }
  } else {
    if (digitalRead(BTN_PIN) == HIGH) {
      digitalWrite(REL_PIN, HIGH);
      gasOn = true;
      relay = "ON";
    } else {
      digitalWrite(BUZZ_PIN, HIGH);
      fidoDelay(buzzDuration);
      digitalWrite(BUZZ_PIN, LOW);
      fidoDelay(buzzInterval);
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
  for (unsigned long i = 0; i < duration; i++) {
    delay(1);
  }
}

void establishConnection(int numTrails) {
  int trailCounter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (trailCounter < numTrails) {
      gasSensorLogic();
      startAPAndServer();
      fidoDelay(1000);
      trailCounter++;
    } else {
      break;
    }
  }
}


void setupAccessPoint() {
  bool apActivated = false;
  while (!apActivated) {
    apActivated = WiFi.softAP(apLogin, "", CHANNEL);
    fidoDelay(10);
  }
}

void startHTTPServer() {
  server.on("/", handleRouter);
  fidoDelay(10);
  server.begin();
}

void writeScanNetworks() {
  int n = WiFi.scanNetworks();
  ssidList = "";
  for (int i = 0; i < n; ++i) {
    // Print SSID and RSSI for each network found
    ssidList += "<div class='div-ssid' onclick='fillSsid(this)'>";
    ssidList += "<span>";
    ssidList += WiFi.SSID(i);
    ssidList += "</span>";
    ssidList += "<span style='position: absolute; right:0.3em;'>";
    ssidList += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "<img id='lock' width='16px' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABoAAAAaCAYAAACpSkzOAAAABmJLR0QA/wD/AP+gvaeTAAABHUlEQVRIic2VQWrCQBSGP0VSEKHg2lD1HAq9Rem2duei10i9hmeQUnAtuHZT6NboQrrSgN0YFxp4iHnjzETxwYPAfPN/mUkyAft6Aj6BGbAGNsfrAdB0yDtbPSAB0pxOgDdfySuwUyRZ74AXV0kd+BNhU+AZqB67C0zE+Ap4dBF9iJBfoHaGqQI/guu7iL5FwLvC9QQ3chEtRUBL4dqCm7uI/kVAoHAPgtvKgdKFotRijg172wo4nAALzN9OXsdAhL7dRB6C0440kc9KTnupPaxUGbOucpFh1xKNgAYQAl8+N2Ha94ZgQxPvs6Ig59q6TCsaC3Zs4n3fumy+kb3ZW1fxnN/hwoPzLj7YRYGeWBMNCxSpWQGHUzemgN/EHtOMriKlrZeFAAAAAElFTkSuQmCC'>";
    ssidList += " (";
    ssidList += WiFi.RSSI(i);
    ssidList += ")";
    ssidList += "</span>";
    ssidList += "</div>";
  }
}

void handleRouter() {
  String postBody = server.arg("plain");
  if (postBody.length() > 0) {
    routerLogin = getFieldValue(postBody, "ssid");
    routerPass = getFieldValue(postBody, "routerPass");
    if (routerPass == "") {
      routerPass = ROUTER_PASS;
    }
    if (routerLogin != "" && routerPass != "") {
      saveCredentials();
      ESP.reset();
    }
  } else {
    writeScanNetworks();
    server.send(200, "text/html", routerHTML());
  }
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
      String numString = "0x" + String(txt[idx + 1]) + String(txt[idx + 2]);
      char ch = (char)(int)strtol(numString.c_str(), NULL, 16);
      result += ch;
      idx += 3;
    } else if (txt[idx] == '+') {
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

    // reading measureStamp value
    startPos += apLogin.length() + 1;
    measureStamp = readFromEEPROM(startPos);

    if (measureStamp == MEASURE_STAMP) {
      // reading slaveServer value
      startPos += measureStamp.length() + 1;
      slaveServer = readFromEEPROM(startPos);

      // reading maxGasVal value
      startPos += slaveServer.length() + 1;
      String max_gas_val = readFromEEPROM(startPos);
      maxGasVal = max_gas_val.toFloat();

      // reading measureDelay value
      startPos += max_gas_val.length() + 1;
      String measure_delay = readFromEEPROM(startPos);
      measureDelay = measure_delay.toInt();

      // reading connReistTrails value
      startPos += measure_delay.length() + 1;
      String conn_reist_trails = readFromEEPROM(startPos);
      connReistTrails = conn_reist_trails.toInt();

      // reading buzzDuration value
      startPos += conn_reist_trails.length() + 1;
      String buzz_duration = readFromEEPROM(startPos);
      buzzDuration = buzz_duration.toInt();

      // reading buzzInterval value
      startPos += buzz_duration.length() + 1;
      String buzz_interval = readFromEEPROM(startPos);
      buzzInterval = buzz_interval.toInt();

      // reading R_0 value
      startPos += buzz_interval.length() + 1;
      String r_0 = readFromEEPROM(startPos);
      R_0 = r_0.toInt();

      // reading routerLogin value
      startPos += r_0.length() + 1;
      routerLogin = readFromEEPROM(startPos);
    } else {
      // reading routerLogin value
      routerLogin = readFromEEPROM(startPos);
    }

    // reading routerPass value
    startPos += routerLogin.length() + 1;
    routerPass = readFromEEPROM(startPos);

    if (routerPass == ROUTER_PASS) {
      routerPass = "";
    }

    EEPROM.end();
  } else {
    // assigning default values to authorization variables
    apLogin = AP_LOGIN;

    // writing stamp
    startPos = OFFSET;
    stamp = STAMP;
    writeEEPROM(stamp, startPos);

    // writting apLogin
    startPos += stamp.length() + 1;
    writeEEPROM(apLogin, startPos);

    EEPROM.end();
    ESP.reset();
  }
}

void saveCredentials() {
  EEPROM.begin(MEMORY_SIZE);

  // writting stamp
  int startPos = OFFSET;
  stamp = STAMP;
  writeEEPROM(stamp, startPos);

  // writting apLogin
  startPos += stamp.length() + 1;
  writeEEPROM(apLogin, startPos);

  if (measureStamp == MEASURE_STAMP) {
    // writting measureStamp
    startPos += apLogin.length() + 1;
    writeEEPROM(measureStamp, startPos);

    // writting slaveServer
    startPos += measureStamp.length() + 1;
    writeEEPROM(slaveServer, startPos);

    // writting maxGasVal
    startPos += slaveServer.length() + 1;
    String max_gas_val = String(maxGasVal);
    writeEEPROM(max_gas_val, startPos);

    // writting measureDelay
    startPos += max_gas_val.length() + 1;
    String measure_delay = String(measureDelay);
    writeEEPROM(measure_delay, startPos);

    // writting connReistTrails
    startPos += measure_delay.length() + 1;
    String conn_reist_trails = String(connReistTrails);
    writeEEPROM(conn_reist_trails, startPos);

    // writting buzzDuration
    startPos += conn_reist_trails.length() + 1;
    String buzz_duration = String(buzzDuration);
    writeEEPROM(buzz_duration, startPos);

    // writting buzzInterval
    startPos += buzz_duration.length() + 1;
    String buzz_interval = String(buzzInterval);
    writeEEPROM(buzz_interval, startPos);

    // writting R_0
    startPos += buzz_interval.length() + 1;
    String r_0 = String(R_0);
    writeEEPROM(r_0, startPos);

    // writing routerLogin
    startPos += r_0.length() + 1;
    writeEEPROM(routerLogin, startPos);
  } else {
    // writing routerLogin
    startPos += apLogin.length() + 1;
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
    EEPROM.put(stPos + i, char(txt[i - 1]));
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

  content += ".parent-button-hide-show {";
  content += "position : relative;";
  content += "}";

  content += ".list-ssid {";
  content += "line-height: 2em;";
  content += "padding: 0px 0px 20px 0px;";
  content += "font-size: 1.2em;";
  content += "}";

  content += ".hide {";
  content += "display : none;";
  content += "}";

  content += ".button-hide-show {";
  content += "position : absolute;";
  content += "right : 10px;";
  content += "top : 50%;";
  content += "transform : translate(0, -50%);";
  content += "}";

  content += ".comment {";
  content += "color: red;";
  content += "font-style: italic;";
  content += "margin: 0.5em 0px;";
  content += "font-size: 1.1em;";
  content += "}";

  content += ".div-ssid {";
  content += "position : relative;";
  content += "padding: 0px 0.3em;";
  content += "}";

  content += ".div-ssid > :first-child {";
  content += "text-decoration: underline;";
  content += "color: -webkit-link;";
  content += "}";

  content += ".div-ssid:hover {";
  content += "background-color : rgba(0,0,0, 0.05);";
  content += "cursor : pointer;";
  content += "}";

  content += ".div-ssid:hover :first-child {";
  content += "color: purple;";
  content += "}";

  content += ".div-ssid:active :first-child {";
  content += "color: -webkit-activelink;";
  content += "}";

  content += "input[type=text], input[type=password] {";
  content += "width: 100%;";
  content += "padding: 12px 15px;";
  content += "margin: 8px 0;";
  content += "display: inline-block;";
  content += "border: 1px solid #ccc;";
  content += "box-sizing: border-box;";
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

  content += "</style>";
  return content;
}

String javaScript() {
  String content = "<script>";
  content += "var showButton = document.getElementById('showButton');\n";
  content += "var hideButton = document.getElementById('hideButton');\n";
  content += "var password = document.getElementById('password');\n";
  content += "var ssid = document.getElementById('ssid');\n";
  if (ssidList == "") {
    content += "ssid.focus();\n";
  }
  content += "function showButtonFunction(){\n";
  content += "password.type = 'text';";
  content += "showButton.style.display = 'none';";
  content += "hideButton.style.display = 'inline-block';";
  content += "};\n";
  content += "function hideButtonFunction(){\n";
  content += "password.type = 'password';";
  content += "hideButton.style.display = 'none';";
  content += "showButton.style.display = 'inline-block';";
  content += "};\n";
  content += "function fillSsid(v){\n";
  content += "if(v.childNodes[1].childNodes[0].id != 'lock'){";
  content += "password.setAttribute('disabled','')";
  content += "}else {";
  content += "password.focus();";
  content += "password.removeAttribute('disabled')";
  content += "}";
  content += "ssid.value = v.childNodes[0].innerText;";
  content += "};\n";
  content += "</script>";
  return content;
}

String routerHTML() {
  String content = "<!DOCTYPE HTML>";
  content += "<html>";
  content += "<head>";
  content += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  content += "<meta charset=\"UTF-8\">";
  content += "<title>Настройки роутера</title>";
  content += "</head>";
  content += css();
  content += "<body>";
  content += "<h1 class = 'fido-electronics'>Fido Electronics</h1>";
  content += "<div class=\"container\">";
  if (ssidList != "") {
    content += "<p class = 'comment'>Выберите одну из следующих доступных сетей для подключения вашего устройства к интернету:</p>";
    content += "<div class='list-ssid' id='listSsid'>";
    content += ssidList;
    content += "</div>";
  }else {
    content += "<p class = 'comment'>Введите имя сети(SSID роутера) и пароль для подключения вашего устройства к интернету:</p>";
  }
  content += "<form method=\"POST\">";
  content += "<label for=\"login\">Имя сети(SSID):</label>";
  content += "<input id = \"ssid\" type=\"text\" name=\"ssid\" placeholder = \"Введите SSID вашего Wi-Fi роутера...\">";
  content += "<label for=\"password\">Пароль:</label>";
  content += "<div class = \"parent-button-hide-show\">";
  content += "<input id = \"password\" type=\"password\" name=\"routerPass\" placeholder = \"Введите пароль вашего Wi-Fi роутера...\">";
  content += "<input type=\"button\" id = \"showButton\" class = \"button-hide-show\" onclick = \"showButtonFunction()\" value = \"показать\">";
  content += "<input type=\"button\" id = \"hideButton\" class = \"button-hide-show hide\" onclick = \"hideButtonFunction()\" value=\"скрывать\">";
  content += "</div>";
  content += "<input type=\"submit\" value=\"Сохранить\">";
  content += "</form>";
  content += "</div>";
  content += javaScript();
  content += "</body>";
  content += "</html>";
  return content;
}
