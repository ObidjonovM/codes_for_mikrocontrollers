#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP_EEPROM.h>

#define NOTE_A7 3520


/*****************************************  CONSTANTS    *******************************/

///////////////////////////////////// UNIQUE CONSTANTS FROM CLOUD //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const String DEVICE_NAME = "gas_sensor";              // Device name of the product
const String SERIAL_NUMBER = "000322db7e0b";          // Serial number of the product
const bool ACTIONS = false;                           // Actions of the product
const String AP_LOGIN = "FidoElectronics29";          // Access Point (AP) mode default login
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
const String ROUTER_PASS = "DEFAULT";
const String PB_SERVER = "http://10.50.50.157:5000";
const String END_POINT = "/from_device";
const String SETTINGS_END_POINT = "/products/get_device_settings";
////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////// GENERAL CONSTANTS //////////////////////////////////////////////
const int MEMORY_SIZE = 512;                   // EEPROM memory size
const int OFFSET = 0;                          // Offset to start records
const String STAMP = "FIDOELECTRONICS";        // Stamp to check if the user data was recorded
const String SETTINGS_STAMP = "SETTINGS_STAMP";
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
String settingsStamp = "";
String apLogin = "";
String routerLogin = "";
String routerPass = "";
String slaveServer = "";

// HTTP related variables
int statusCode;
String httpResp;
String ssidList;
String settingsHttpResp;
bool loggedIn = false;

String relay = "OFF";
bool mustWriteEEPROM = false;
bool wasConnected = false;
bool gasOn = false;
float gasVal = 0;
int delayCounter = 0;
int measureDelay = 1000;
int buzzDuration = 100;
int buzzInterval = 1000;
int connReistTrails = 10;
float maxGasVal = 0.10;
int R_0 = 1029;


ESP8266WebServer server(80);           // starting the web server

//////////////////////// HTML content functions /////////////////////////////////////
String css() {
  String content = "<style>";
  content += ".fido-electronics {";
  content += "text-align : center;";
  content += "}";

  content += ".parent-eye-icon {";
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

  content += ".eye-icon {";
  content += "position : absolute;";
  content += "right : 0px;";
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
  content += "padding: 0.3em 0.3em;";
  content += "}";

  content += ".div-ssid > :first-child {";
  content += "text-decoration: underline;";
  content += "color: blue;";
  content += "}";

  content += ".must-hover:hover {";
  content += "background-color : rgba(0,0,0, 0.05);";
  content += "cursor : pointer;";
  content += "}";

  content += ".selected-ssid {";
  content += "background-color : rgba(0,0,0, 0.2);";
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
  content += "padding: 0.5em;";
  content += "cursor: pointer;";
  content += "}";

  content += ".big-button {";
  content += "padding: 0.5em;";
  content += "cursor: pointer;";
  content += "}";

  content += ".unique-form {";
  content += "background-color: rgba(0,0,0, 0.2);";
  content += "padding: 1em;";
  content += "border-radius: 3px;";
  content += "}";

  content += ".scan-button {";
  content += "width: 100%;";
  content += "padding: 0.5em;";
  content += "margin-top: 1em;";
  content += "cursor: pointer;";
  content += "border-radius: 3px;";
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
  content += "var ssid = document.getElementById('ssid');\n";
  content += "var must_show = document.querySelectorAll('.must-show');\n";
  content += "var div_ssid = document.querySelectorAll('.div-ssid');\n";
  if (ssidList == "") {
    content += "ssid.focus();\n";
  }
  content += "function showIconFunction(e){\n";
  content += "e.parentNode.querySelector('.password').type = 'text';\n";
  content += "e.style.display = 'none';\n";
  content += "e.parentNode.querySelector('.hide-icon').style.display = 'inline-block';\n";
  content += "event.stopPropagation();\n";
  content += "};\n";
  content += "function hideIconFunction(e){\n";
  content += "e.parentNode.querySelector('.password').type = 'password';\n";
  content += "e.style.display = 'none';\n";
  content += "e.parentNode.querySelector('.show-icon').style.display = 'inline-block';\n";
  content += "event.stopPropagation();\n";
  content += "};\n";
  content += "function showInfo(e){\n";
  content += "for (let i =0; i < div_ssid.length; i++) {\n";
  content += "if(div_ssid[i] == e){\n";
  content += "div_ssid[i].classList.add('selected-ssid');\n";
  content += "div_ssid[i].classList.remove('must-hover');\n";
  content += "must_show[i].classList.remove('hide');\n";
  content += "} else {\n";
  content += "div_ssid[i].classList.remove('selected-ssid');\n";
  content += "div_ssid[i].classList.add('must-hover');\n";
  content += "must_show[i].classList.add('hide');\n";
  content += "div_ssid[i].querySelector('form').reset();\n";
  content += "};\n";
  content += "};\n";
  content += "let sel_must_show = e.querySelector('.must-show');\n";
  content += "if(sel_must_show.querySelector('.password') != null){\n";
  content += "sel_must_show.querySelector('.password').focus();\n";
  content += "} else {\n";
  content += "sel_must_show.querySelector('.submit').focus();\n";
  content += "};\n";
  content += "};\n";
  content += "function reloadPage(){\n";
  content += "window.location.reload();\n";
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
    content += "<p class = 'comment'>Введите имя(SSID) для сети и ключ безопасности сети для подключения вашего устройства к интернету:</p>";
    content += "<form method=\"POST\" class=\"unique-form\">";
    content += "<label for=\"login\">Имя сети(SSID):</label>";
    content += "<input id = \"ssid\" type=\"text\" name=\"ssid\" placeholder = \"Введите имя(SSID) для сети...\" required>";
    content += "<label for=\"password\">Ключ безопасности сети:</label>";
    content += "<div class = \"parent-eye-icon\">";
    content += "<input class = \"password\" type=\"password\" name=\"routerPass\" placeholder = \"Введите ключ безопасности сети...\" required>";
    content += "<img class = \"eye-icon show-icon hide\" onclick = \"showIconFunction(this)\" src = \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAnCAYAAABNJBuZAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAABmJLR0QA/wD/AP+gvaeTAAAAB3RJTUUH5gICBSIWia34TgAABBVJREFUWMPt18tvlFUch/Hne973nZnO9DLQDtKGAhaIFjEtRjeALsBLiInu1EjiQvFCMIatmniJbnRj4t7gxihiWBCiCCFqDYSFQbxRKNFyaatcSmdKp3N73/Nz4X/Q1qaL+ezPe86Tk5NzXmhqampqampqWnL0/3xV5Ff0kjTqpDI5zDwzxRtEmSzlqetLP6S7b5Dl7TuYqZxF5JCyQCdYFVTCqACzSCluTZ6jdP3q0grJ5rvoKgxgFhMEXXgr5Z1L3y8Fg6C8wBuMm8Wn0fQf5jviOB7HuVauXvxxaYQUejfS1rqZOJ7E++lMEOSflAueEBSBCcOqAiF1mLEGKJkl+6Ow70y9cR4pYGz0e5J6bV7rCOYz+I41A6zv/ZjpmSG81XuDoO1dyfWh+ACmFlA/qANUANogOYHZOLjd3hc7ofKriJL2Zesoz07g48bi70hhdT/dhZe4Xf4B76ubpOgtsCFv5YNOuTeRprD6YW+kBQ05l8PC58FOeKt9J6VfF7rurfqBFM4K49LIt2C2eDsSZlooFLZSq/+JWa1fit4D/1Wgvv1G+TlJWe+rh1BqnwgGUfAQKIfVPpGi3S7QKZ/MfAmp7VK4DWqnQHG+cw2lydE5hbi5DEqls3g/g/fVbil6Q7KDYdB1oJ78npXcFuf84cBl9jinY3E89rJR2SPUJRfdh+ykmXvYueWz3qbfF2RFy2tm08Fcd2POIS25PGG4Ail6VmKqXt75Reyv4IKoA8x5n9SR6wgj/3W6pavhaCkiOyqFA5CMg3oCtxLn0mWj+hHSTil/L8osboiZx/tpwA+Z0RNmj2zGspglFf47d8LMxw3bgC0jk1qHeTaBv4ZpGXDb2y3aWx8BS+8ERlAyipI5h8zpjGRyHZguk4runjCrzqLwVUiGU27VFW/lAVC7WXwagr1mjbVxMvW45DaY1T4V0TNyHAnCjtFK5dwuyT2K4ndEeE04ijcvLl5ItVwi37kJbyUyqXtGkuRWAwX7vC+NQTwE4Ytgw8h/IwV5iTGjfghLP+Wcyokvfu6T5AUp2CH5t0U06tTKVPEM1ZnSnELmdSH2rt8G8qTCjcR+Yju4vVhyHPywlNqFMMxGgAxSP9h58/XjkHpaUiCXfIiF444Cs/Wf+PuvX+a8lnnf7L0btuKtShSsxqjcCcEr4HogOSuIkWsDYswXQd2gfsQxVPtMlq5IWSq1C/OKWJAQgJ51D9Da8iD1+DzoH4et3AzBYxirgTTII5sR/IaSo+bzY7hJRMjkzZ+ZvjEx7zUs2Os3asmxau0WvNURreCXo2AibRZl5eTlSjPW6E7kKlTjn4lcH5cvnFyo6Rf+f0QupKdvkCSpkEl3gVJI4H2VS8NDFHrv4ubYyEJP29TU1NTU1NS0FP0LqhDKDpwtSjsAAAAldEVYdGRhdGU6Y3JlYXRlADIwMjItMDItMDJUMDU6MzQ6MTkrMDA6MDAFzLzFAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDIyLTAyLTAyVDA1OjM0OjE5KzAwOjAwdJEEeQAAAABJRU5ErkJggg==\">";
    content += "<img class = \"eye-icon hide-icon hide\" onclick = \"hideIconFunction(this)\" src = \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAnCAYAAABNJBuZAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAABmJLR0QA/wD/AP+gvaeTAAAAB3RJTUUH5gICBSERPOQ+LgAABAlJREFUWMPtlstvVVUUh7/fPuf0Puhtb0hLsbRQimgwQRxUESEmxPgm0agMNGFgdKAxER8hJiYGNNUhMXHC1IExmCg4IT4GgolRFA0SIigvI6WALdCW297b07P3coD+A7dXdHC+6d7Z6/ftlay9IScnJycnJyfnf4dafWDXkpV0d2+gVjtBFFWAGCFMHphjunYKs4QkKWLm8SFl7Oyv864btUqgs7uXG/o3EEVFQnB4Xys4V1gJ3AbcjFgINNrLQ/U4KTAzfcxFrtN8NkO9NjHv+i3pSO/yISYmDtG9aBPeT7bHUWUTuHslFQ0mwDKhqhkFsJ8MfxC408LMLgiTF0YPk05fnVeGeXekb3AdRqBavZvgp9e4qPSWU9Qnhf1mpAZVQRFsHPx3klstFd6U02QIE3udq2QdHcupp2P4tPHfiPTduB4j0NGxgTQdeci5ZJsIe8yyryDZIqdJs+xL8IclNyeXPA7RchTeN7OK04JbITsk3Fylo5+ZxkV8mjaVxTUr0Ts4hFmdUmE19Znjj0rxs8gPB2t8LrW9iMLHIUu/weJHIHkSc33AHLIY8z96P/EyUBaFN4x0ARi9fXc1falNiwz07aQtGWQuO3ePXLxFzu8QhR+cK90vx6iFbFyu7Tm5sBfLvpCLnpeUmqXDKNqcFGlk2eUdmCFKr5nV28wyFi1ddX1FRs4P4/3kgFz0kuR3QtuROK6CaZVkR6TkPhfxSZY1TkrxE+B3QoikcBGzSsjKXVFcrvsw9TawFNqfitxizEJTeeJmRSQwA8DMDDDs7zUDJGFmK+Ko/KCkTy1M78e1r7u2F/0zMM0Q4CQkNT9Em+5IpX0Tkav8bsG/i0WvWmiszrIrIDtugTUonIFkm5ymgqW/QfkVzH42U5egFie1cZ/Vi1Hc+Towgq5+4MN5mpVpemol5TGMSZJo5ZnA1JwUbzWbPQWNH5yKT6NoI/jdZlZ0ijci+yWE+mdOhRfk7MPZRmM0jqvbhYtRfVgq1Z0TIycPXl+Rq1dGqXYN4m2MYtu6Y1kYrUnxVkiqctFNoHazcMJCtg/Ct6CS1PYMsgPBp0fjqLJd0rhc4x3RVkOOkbNfE9K5pvLM+2XvX7keY44kXkYItc1S4T3JzlpId4FbDFoGcsgmMH/ETD2SW4vY46Kp3RYWZOKaRFpr/nVvyRelc1EfPT0PMDs78phzccEgxfQwWGRmk5ICRgFRQToK2UeRFp72XEBE/HHqAD6dnVeGlv1+V9xyO2k66wqFtcH7SwS7VBTlAXD9QAI2hrIzkQbGvY1wbnwfPdU7GDn5fUvqt0QkLhZZsmwI71OEKJZ6SdPLSCWw5FoZebBZ0uxPoqjMudOHCFnWqnvMycnJycnJycn5F/kLywjQLv5HliQAAAAldEVYdGRhdGU6Y3JlYXRlADIwMjItMDItMDJUMDU6MzM6MTMrMDA6MDBDYPjyAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDIyLTAyLTAyVDA1OjMzOjEzKzAwOjAwMj1ATgAAAABJRU5ErkJggg==\">";
    content += "</div>";
    content += "<input class=\"submit\" type=\"submit\" value=\"Подключиться\">";
    content += "</form>";
  }
  content += "<input type=\"button\" class=\"scan-button\" value=\"Обновить\" onclick=\"reloadPage()\">";
  content += "</div>";
  content += javaScript();
  content += "</body>";
  content += "</html>";
  return content;
}

void writeScanNetworks() {
  int n = WiFi.scanNetworks();
  ssidList = "";
  for (int i = 0; i < n; ++i) {
    // Print SSID and RSSI for each network found
    ssidList += "<div class='div-ssid must-hover' onclick='showInfo(this)'>";
    ssidList += "<span>";
    ssidList += WiFi.SSID(i);
    ssidList += "</span>";
    ssidList += "<span style='position: absolute; right:0.3em;'>";
    if (WiFi.encryptionType(i) != ENC_TYPE_NONE) {
      ssidList += "<img width='16px' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABoAAAAaCAYAAACpSkzOAAAABmJLR0QA/wD/AP+gvaeTAAABHUlEQVRIic2VQWrCQBSGP0VSEKHg2lD1HAq9Rem2duei10i9hmeQUnAtuHZT6NboQrrSgN0YFxp4iHnjzETxwYPAfPN/mUkyAft6Aj6BGbAGNsfrAdB0yDtbPSAB0pxOgDdfySuwUyRZ74AXV0kd+BNhU+AZqB67C0zE+Ap4dBF9iJBfoHaGqQI/guu7iL5FwLvC9QQ3chEtRUBL4dqCm7uI/kVAoHAPgtvKgdKFotRijg172wo4nAALzN9OXsdAhL7dRB6C0440kc9KTnupPaxUGbOucpFh1xKNgAYQAl8+N2Ha94ZgQxPvs6Ig59q6TCsaC3Zs4n3fumy+kb3ZW1fxnN/hwoPzLj7YRYGeWBMNCxSpWQGHUzemgN/EHtOMriKlrZeFAAAAAElFTkSuQmCC'>";
    }
    ssidList += " (";
    ssidList += WiFi.RSSI(i);
    ssidList += ")";
    ssidList += "</span>";
    ssidList += "<form method=\"POST\">";
    ssidList += "<input style=\"display: none;\" value = \""+WiFi.SSID(i)+"\" type=\"text\" name=\"ssid\">";
    ssidList += "<div class = \"hide must-show\">";
    if (WiFi.encryptionType(i) != ENC_TYPE_NONE) {
      ssidList += "<label for=\"password\">Ключ безопасности сети:</label>";
      ssidList += "<div class = \"parent-eye-icon\">";
      ssidList += "<input class = \"password\" type=\"password\" name=\"routerPass\" placeholder = \"Введите ключ безопасности сети...\" required  onclick=\"event.stopPropagation();\">";
      ssidList += "<img class = \"eye-icon show-icon\" onclick = \"showIconFunction(this)\" src = \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAnCAYAAABNJBuZAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAABmJLR0QA/wD/AP+gvaeTAAAAB3RJTUUH5gICBSIWia34TgAABBVJREFUWMPt18tvlFUch/Hne973nZnO9DLQDtKGAhaIFjEtRjeALsBLiInu1EjiQvFCMIatmniJbnRj4t7gxihiWBCiCCFqDYSFQbxRKNFyaatcSmdKp3N73/Nz4X/Q1qaL+ezPe86Tk5NzXmhqampqampqWnL0/3xV5Ff0kjTqpDI5zDwzxRtEmSzlqetLP6S7b5Dl7TuYqZxF5JCyQCdYFVTCqACzSCluTZ6jdP3q0grJ5rvoKgxgFhMEXXgr5Z1L3y8Fg6C8wBuMm8Wn0fQf5jviOB7HuVauXvxxaYQUejfS1rqZOJ7E++lMEOSflAueEBSBCcOqAiF1mLEGKJkl+6Ow70y9cR4pYGz0e5J6bV7rCOYz+I41A6zv/ZjpmSG81XuDoO1dyfWh+ACmFlA/qANUANogOYHZOLjd3hc7ofKriJL2Zesoz07g48bi70hhdT/dhZe4Xf4B76ubpOgtsCFv5YNOuTeRprD6YW+kBQ05l8PC58FOeKt9J6VfF7rurfqBFM4K49LIt2C2eDsSZlooFLZSq/+JWa1fit4D/1Wgvv1G+TlJWe+rh1BqnwgGUfAQKIfVPpGi3S7QKZ/MfAmp7VK4DWqnQHG+cw2lydE5hbi5DEqls3g/g/fVbil6Q7KDYdB1oJ78npXcFuf84cBl9jinY3E89rJR2SPUJRfdh+ykmXvYueWz3qbfF2RFy2tm08Fcd2POIS25PGG4Ail6VmKqXt75Reyv4IKoA8x5n9SR6wgj/3W6pavhaCkiOyqFA5CMg3oCtxLn0mWj+hHSTil/L8osboiZx/tpwA+Z0RNmj2zGspglFf47d8LMxw3bgC0jk1qHeTaBv4ZpGXDb2y3aWx8BS+8ERlAyipI5h8zpjGRyHZguk4runjCrzqLwVUiGU27VFW/lAVC7WXwagr1mjbVxMvW45DaY1T4V0TNyHAnCjtFK5dwuyT2K4ndEeE04ijcvLl5ItVwi37kJbyUyqXtGkuRWAwX7vC+NQTwE4Ytgw8h/IwV5iTGjfghLP+Wcyokvfu6T5AUp2CH5t0U06tTKVPEM1ZnSnELmdSH2rt8G8qTCjcR+Yju4vVhyHPywlNqFMMxGgAxSP9h58/XjkHpaUiCXfIiF444Cs/Wf+PuvX+a8lnnf7L0btuKtShSsxqjcCcEr4HogOSuIkWsDYswXQd2gfsQxVPtMlq5IWSq1C/OKWJAQgJ51D9Da8iD1+DzoH4et3AzBYxirgTTII5sR/IaSo+bzY7hJRMjkzZ+ZvjEx7zUs2Os3asmxau0WvNURreCXo2AibRZl5eTlSjPW6E7kKlTjn4lcH5cvnFyo6Rf+f0QupKdvkCSpkEl3gVJI4H2VS8NDFHrv4ubYyEJP29TU1NTU1NS0FP0LqhDKDpwtSjsAAAAldEVYdGRhdGU6Y3JlYXRlADIwMjItMDItMDJUMDU6MzQ6MTkrMDA6MDAFzLzFAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDIyLTAyLTAyVDA1OjM0OjE5KzAwOjAwdJEEeQAAAABJRU5ErkJggg==\">";
      ssidList += "<img class = \"eye-icon hide-icon hide\" onclick = \"hideIconFunction(this)\" src = \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAnCAYAAABNJBuZAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAABmJLR0QA/wD/AP+gvaeTAAAAB3RJTUUH5gICBSERPOQ+LgAABAlJREFUWMPtlstvVVUUh7/fPuf0Puhtb0hLsbRQimgwQRxUESEmxPgm0agMNGFgdKAxER8hJiYGNNUhMXHC1IExmCg4IT4GgolRFA0SIigvI6WALdCW297b07P3coD+A7dXdHC+6d7Z6/ftlay9IScnJycnJyfnf4dafWDXkpV0d2+gVjtBFFWAGCFMHphjunYKs4QkKWLm8SFl7Oyv864btUqgs7uXG/o3EEVFQnB4Xys4V1gJ3AbcjFgINNrLQ/U4KTAzfcxFrtN8NkO9NjHv+i3pSO/yISYmDtG9aBPeT7bHUWUTuHslFQ0mwDKhqhkFsJ8MfxC408LMLgiTF0YPk05fnVeGeXekb3AdRqBavZvgp9e4qPSWU9Qnhf1mpAZVQRFsHPx3klstFd6U02QIE3udq2QdHcupp2P4tPHfiPTduB4j0NGxgTQdeci5ZJsIe8yyryDZIqdJs+xL8IclNyeXPA7RchTeN7OK04JbITsk3Fylo5+ZxkV8mjaVxTUr0Ts4hFmdUmE19Znjj0rxs8gPB2t8LrW9iMLHIUu/weJHIHkSc33AHLIY8z96P/EyUBaFN4x0ARi9fXc1falNiwz07aQtGWQuO3ePXLxFzu8QhR+cK90vx6iFbFyu7Tm5sBfLvpCLnpeUmqXDKNqcFGlk2eUdmCFKr5nV28wyFi1ddX1FRs4P4/3kgFz0kuR3QtuROK6CaZVkR6TkPhfxSZY1TkrxE+B3QoikcBGzSsjKXVFcrvsw9TawFNqfitxizEJTeeJmRSQwA8DMDDDs7zUDJGFmK+Ko/KCkTy1M78e1r7u2F/0zMM0Q4CQkNT9Em+5IpX0Tkav8bsG/i0WvWmiszrIrIDtugTUonIFkm5ymgqW/QfkVzH42U5egFie1cZ/Vi1Hc+Towgq5+4MN5mpVpemol5TGMSZJo5ZnA1JwUbzWbPQWNH5yKT6NoI/jdZlZ0ijci+yWE+mdOhRfk7MPZRmM0jqvbhYtRfVgq1Z0TIycPXl+Rq1dGqXYN4m2MYtu6Y1kYrUnxVkiqctFNoHazcMJCtg/Ct6CS1PYMsgPBp0fjqLJd0rhc4x3RVkOOkbNfE9K5pvLM+2XvX7keY44kXkYItc1S4T3JzlpId4FbDFoGcsgmMH/ETD2SW4vY46Kp3RYWZOKaRFpr/nVvyRelc1EfPT0PMDs78phzccEgxfQwWGRmk5ICRgFRQToK2UeRFp72XEBE/HHqAD6dnVeGlv1+V9xyO2k66wqFtcH7SwS7VBTlAXD9QAI2hrIzkQbGvY1wbnwfPdU7GDn5fUvqt0QkLhZZsmwI71OEKJZ6SdPLSCWw5FoZebBZ0uxPoqjMudOHCFnWqnvMycnJycnJycn5F/kLywjQLv5HliQAAAAldEVYdGRhdGU6Y3JlYXRlADIwMjItMDItMDJUMDU6MzM6MTMrMDA6MDBDYPjyAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDIyLTAyLTAyVDA1OjMzOjEzKzAwOjAwMj1ATgAAAABJRU5ErkJggg==\">";
      ssidList += "</div>";
    }
    ssidList += "<input class=\"submit\" type=\"submit\" value=\"Подключиться\" onclick=\"event.stopPropagation();\">";
    ssidList += "</div>";
    ssidList += "</form>";
    ssidList += "</div>";
  }
}

void fidoDelay(unsigned long duration) {
  for (unsigned long i = 0; i < duration; i++) {
    delay(1);
  }
}

////////////////////////// EEPROM functions///////////////////////////////////////////////////
byte fieldLen;
char ch;
String readFromEEPROM(int stPos) {
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

void loadCredentials() {
  EEPROM.begin(MEMORY_SIZE);

  // reading stamp value
  int startPos = OFFSET;
  stamp = readFromEEPROM(startPos);
  if (stamp == STAMP) {
    // reading apLogin value
    startPos += stamp.length() + 1;
    apLogin = readFromEEPROM(startPos);

    // reading settingsStamp value
    startPos += apLogin.length() + 1;
    settingsStamp = readFromEEPROM(startPos);

    if (settingsStamp == SETTINGS_STAMP) {
      // reading slaveServer value
      startPos += settingsStamp.length() + 1;
      slaveServer = readFromEEPROM(startPos);

      // reading routerLogin value
      startPos += slaveServer.length() + 1;
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

  if (settingsStamp == SETTINGS_STAMP) {
    // writting settingsStamp
    startPos += apLogin.length() + 1;
    writeEEPROM(settingsStamp, startPos);

    // writting slaveServer
    startPos += settingsStamp.length() + 1;
    writeEEPROM(slaveServer, startPos);

    // writing routerLogin
    startPos += slaveServer.length() + 1;
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

void setup() {
  pinMode(REL_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(SAPS_BTN_PIN, INPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(REL_PIN, LOW);
  fidoDelay(1000);

  WiFi.disconnect();
  fidoDelay(2000);
  WiFi.softAPdisconnect(true);
  fidoDelay(3000);
  loadCredentials();
  fidoDelay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(routerLogin.c_str(), routerPass.c_str());
//  fidoDelay(290000);                                           //5 minut dan keyin qurilma ishga tushadi
}

String replaceASCIICode(String txt) {
  String result;
  unsigned int idx = 0;

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

void startHTTPServer() {
  server.on("/", handleRouter);
  fidoDelay(10);
  server.begin();
}

void setupAccessPoint() {
  bool apActivated = false;
  while (!apActivated) {
    apActivated = WiFi.softAP(apLogin, "", CHANNEL);
    fidoDelay(10);
  }
}

void startAPAndServer() {
  int sapsCounter = 0;

  while (digitalRead(SAPS_BTN_PIN) == HIGH) {
    fidoDelay(1);
    sapsCounter++;
    if (sapsCounter == 4000) {
      digitalWrite(LED_PIN, LOW);
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

float getMethanePPM() {
  float a0 = analogRead(GAS_PIN); // get raw reading from sensor
  float v_o = a0 * 5 / 1023; // convert reading to volts
  float R_S = (5 - v_o) * 1000 / v_o; // apply formula for getting RS
  float PPM = pow(R_S / R_0, -2.95) * 1000; //apply formula for getting PPM

  return PPM; // return PPM value to calling function
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
      tone(BUZZ_PIN, NOTE_A7);
      fidoDelay(buzzDuration);
      noTone(BUZZ_PIN);
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

void getFromDevice() {
  httpResp = sendRequest(slaveServer + END_POINT, 
                        "{\"device_name\":\"" + DEVICE_NAME + "\", " +
                        +"\"serial_num\":\"" + SERIAL_NUMBER + "\", " +
                        +"\"actions\":" + ACTIONS + ", " +
                        +"\"states\":{" +
                          +"\"action_states\":[" +
                            +"\"" + relay + "\"" +
                          +"]" +
                        +"}, " +
                        +"\"measurements\":{" +
                          +"\"gas_value\":[" +
                            +"\"" + gasVal + "\"" +
                          +"]" +
                        +"}, " +
                         +"\"slave_server\":\"" + slaveServer + "\"}");
}

void deviceSettings(String httpResp) {
  String slave_server = extractFromJSON(httpResp, "slave_server");

  if (slave_server != "" && slave_server != slaveServer) {
    slaveServer = slave_server;
    mustWriteEEPROM = true;
  }
}

void getDeviceSettings() {
  settingsHttpResp = sendRequest(PB_SERVER + SETTINGS_END_POINT, 
                                "{\"serial_num\":\"" + SERIAL_NUMBER + "\"}");
  deviceSettings(settingsHttpResp);
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

void loop() {
  startAPAndServer();
  if (settingsStamp == SETTINGS_STAMP) {
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
        settingsStamp = SETTINGS_STAMP;
        saveCredentials();
        ESP.reset();
      }
    }
  }
}