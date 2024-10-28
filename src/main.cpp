#include <Arduino.h>
#include "HX711.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"
#include "SPIFFS.h"
#include <Preferences.h>
#include <ESPmDNS.h>

#define PROCESSING 0
#define READY 1

void connect_app(bool useStaticIP);
void handle_add_item_form(AsyncWebServerRequest *request);
void handle_config_form(AsyncWebServerRequest *request);
void initialize_data(void);
void initialize_loadcell(void);
String readRecords(void);
void reset_preferences(void);
void start_server(void);
void writeRecord(const String &itemName, const String &avgValue);

void state_machine(void);

// preferences variable
Preferences preferences;

HX711 loadcell;

const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 4;

// Crear un servidor AsyncWebServer en el puerto 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

// Variables to save values from HTML form
String ssid;
String password;
String ip;
String gateway;

String valor_en_gramos = "0.00";

// IP Address, Gateway and subnet
IPAddress localIP(192, 168, 1, 200);;
IPAddress localGateway(192, 168, 1, 1);;
IPAddress subnet(255, 255, 0, 0);

// Pendiente y su incertidumbre (Obtenido por la calibracion)
static float scale_factor = 0.00238210329;
static double incert_factor_escala = 0.00000051581;

void setup() {
  // Iniciar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Serial.begin(9600);
  
  reset_preferences();
  initialize_data();
  initialize_loadcell();
  connect_app(false);
  start_server();
}

void state_machine () {
  static uint32_t state = PROCESSING;
  static uint32_t count = 0;
  static float cell_count;
  static float weight;
  static float weightSum = 0;
  static float weightAvg = 0;

  switch (state) {
    case PROCESSING:
      cell_count = loadcell.get_value();
      weight = cell_count * scale_factor;
      weightSum += weight;
      count++;
      delay(100);
      if (count == 10) state = READY;
      break;

    case READY:
      weightAvg = weightSum / 10.0;
      valor_en_gramos = String(weightAvg);
      Serial.println("weightAvg");
      Serial.println(weightAvg, 2);
      count = 0;
      weightSum = 0;
      state = PROCESSING;
      break;

    default:
      count = 0;
      weightSum = 0;
      state = PROCESSING;
      break;
  }
}

void loop() {
  state_machine();
}

void connect_app(bool useStaticIP = false) {
  WiFi.mode(WIFI_AP);

  while(!WiFi.softAP(ssid.c_str(), password.c_str())) {
    Serial.println(".");
    delay(100);
  }

  if(useStaticIP) WiFi.softAPConfig(localIP, localGateway, subnet);

  Serial.println("");
  Serial.print("Iniciado AP:\t");
  Serial.println(ssid.c_str());
  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());

  if(MDNS.begin("esp32")) Serial.println("Habilito: http://esp32.local/");
}

void handle_add_item_form(AsyncWebServerRequest *request) {
  String itemNameForm = request->arg("item-name");
  String itemAvgValueForm = request->arg("item-avg-value");
  writeRecord(itemNameForm, itemAvgValueForm);

  request->redirect("/medicion.html");
}

/*
void handle_config_form(AsyncWebServerRequest *request) {
  String ssidForm = request->arg("SSID");
  String passwordForm = request->arg("PASSWORD");
  String ipForm = request->arg("IP");
  String gatewayForm = request->arg("GATEWAY");

  preferences.begin("Mi-APP", false);
  if (ssidForm != "")
    preferences.putString("SSID", ssidForm);
  if (passwordForm != "")
    preferences.putString("PASSWORD", passwordForm);
  if (ipForm != "")
    preferences.putString("IP", ipForm);
  if (gatewayForm != "")
    preferences.putString("GATEWAY", gatewayForm);
  preferences.end();

  request->redirect("/index.html");
}
*/

void initialize_data() {
  preferences.begin("Mi-APP", false);
  ssid = preferences.getString("ssid","");
  password = preferences.getString("password", "");
  ip = preferences.getString("ip", "");
  gateway = preferences.getString("gateway", "");
  preferences.end();
}

void initialize_loadcell() {
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.tare();
  loadcell.set_raw_mode();
};

String readRecords() {
  if (!SPIFFS.begin(true)) {
    return "Failed to mount file system";
  }

  File file = SPIFFS.open("/db.txt", FILE_READ);
  if (!file) return "Failed to open file";

  String records = "";

  while (file.available()) {
    records += file.readStringUntil('\n');
    records += "\n";
  }

  file.close();
  return records;
}

void reset_preferences() {
  preferences.begin("Mi-APP", false);
  preferences.putString("ssid","Balanza-Contabilizadora");
  preferences.putString("password","");
  preferences.putString("ip", "");
  preferences.putString("gateway", "");
  preferences.putString("item-name", "");
  preferences.putString("item-quantity", "");
  preferences.end(); 
}

void start_server() {
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  // server.on("/config", HTTP_POST, handle_config_form);
  server.on("/add-item", HTTP_POST, handle_add_item_form);
  server.on("/records", HTTP_GET, [](AsyncWebServerRequest *request) {
    String records = readRecords(); // Get records from the file
    request->send(200, "text/plain", records); // Send records as plain text
  });
  server.on("/peso_en_gramos", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", valor_en_gramos);
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not Found");
  });
  server.begin();
  Serial.println("HTTP server started");
}

void writeRecord(const String &itemName, const String &avgValue) {
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount file system");
    return;
  }

  File file = SPIFFS.open("/db.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }

  // If the file is empty, write the column names first
  if (file.size() == 0) {
    file.println("item-name;avg-value");
  }

  // Write the record
  file.println(itemName + ";" + avgValue);
  file.close();
}
