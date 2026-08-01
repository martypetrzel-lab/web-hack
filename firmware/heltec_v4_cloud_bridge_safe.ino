/*
  Heltec WiFi LoRa 32 V4 -> Railway cloud bridge
  Safe laboratory build: WiFi scan, LoRa RX, STOP and telemetry.

  Before upload set WIFI_SSID, WIFI_PASS, WEB_SERVER and API_KEY.
  Configure TFT_eSPI User_Setup.h for ILI9341:
  SCLK=36, MOSI=35, CS=4, DC=5, RST=6, 240x320.
*/
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <RadioLib.h>

const char *WIFI_SSID = "DOPLN_WIFI";
const char *WIFI_PASS = "DOPLN_HESLO";
const char *WEB_SERVER = "https://web-hack-production.up.railway.app";
const char *API_KEY = "DOPLN_STEJNY_DEVICE_API_KEY_JAKO_NA_RAILWAY";

TFT_eSPI tft;
SX1262 radio = new Module(8, 14, 12, 13); // NSS, DIO1, RST, BUSY

struct DeviceState {
  String status = "Startuji";
  String mode = "idle";
  bool running = false;
  int wifiCount = 0;
  String wifiList = "";
  String deauthTarget = "—";
  uint32_t deauthPackets = 0;
  uint32_t beaconCount = 0;
  uint32_t probeCount = 0;
  String probeList = "";
  String bleType = "—";
  uint32_t bleCount = 0;
  uint32_t loraPackets = 0;
  float loraRssi = 0;
  float loraSnr = 0;
  String loraData = "—";
} state;

bool loraReady = false;
uint32_t lastPost = 0;

String jsonEscape(const String &src) {
  String out; out.reserve(src.length() + 16);
  for (size_t i = 0; i < src.length(); ++i) {
    char c = src[i];
    if (c == '\\' || c == '"') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if ((uint8_t)c >= 0x20) out += c;
  }
  return out;
}

String stateJson() {
  String j; j.reserve(1500 + state.wifiList.length());
  j = "{\"status\":\"" + jsonEscape(state.status) + "\",\"running\":" + (state.running ? "true" : "false");
  j += ",\"mode\":\"" + state.mode + "\",\"wifi_count\":" + String(state.wifiCount);
  j += ",\"wifi_list\":\"" + jsonEscape(state.wifiList) + "\",\"deauth_target\":\"" + jsonEscape(state.deauthTarget) + "\"";
  j += ",\"deauth_packets\":" + String(state.deauthPackets) + ",\"beacon_count\":" + String(state.beaconCount);
  j += ",\"probe_count\":" + String(state.probeCount) + ",\"probe_list\":\"" + jsonEscape(state.probeList) + "\"";
  j += ",\"ble_type\":\"" + state.bleType + "\",\"ble_count\":" + String(state.bleCount);
  j += ",\"lora_packets\":" + String(state.loraPackets) + ",\"lora_rssi\":" + String(state.loraRssi, 1);
  j += ",\"lora_snr\":" + String(state.loraSnr, 1) + ",\"lora_data\":\"" + jsonEscape(state.loraData) + "\"}";
  return j;
}

String htmlEscape(String value) {
  value.replace("&", "&amp;"); value.replace("<", "&lt;"); value.replace(">", "&gt;");
  value.replace("\"", "&quot;"); return value;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  state.status = "Připojuji WiFi";
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; ++i) delay(500);
  state.status = WiFi.status() == WL_CONNECTED ? "Připojeno ke cloudu" : "WiFi nedostupná";
}

void runWifiScan() {
  state.running = true; state.mode = "scan"; state.status = "WiFi scan běží…";
  int n = WiFi.scanNetworks(false, true);
  state.wifiCount = max(0, n); state.wifiList = "";
  for (int i = 0; i < n && i < 30; ++i) {
    String ssid = WiFi.SSID(i).length() ? WiFi.SSID(i) : "[skrytá]";
    state.wifiList += String(i + 1) + " · " + String(WiFi.RSSI(i)) + " dBm · " + htmlEscape(ssid) + "<br>";
  }
  WiFi.scanDelete(); state.running = false; state.mode = "idle";
  state.status = "WiFi scan dokončen: " + String(state.wifiCount) + " sítí";
}

void startLoRaScan() {
  if (!loraReady) { state.status = "LoRa modul není připraven"; return; }
  radio.startReceive(); state.running = true; state.mode = "lorascan"; state.status = "LoRa RX 868 MHz běží";
}

void stopAll() {
  if (loraReady) radio.standby();
  state.running = false; state.mode = "idle"; state.status = "Zastaveno";
}

String readJsonString(const String &json, const String &key) {
  String needle = "\"" + key + "\":\""; int start = json.indexOf(needle);
  if (start < 0) return ""; start += needle.length(); int end = json.indexOf('"', start);
  return end < 0 ? "" : json.substring(start, end);
}

void applyCommand(const String &response) {
  String command = readJsonString(response, "command");
  if (command == "scan") runWifiScan();
  else if (command == "lorascan") startLoRaScan();
  else if (command == "stop") stopAll();
}

void postTelemetry() {
  if (WiFi.status() != WL_CONNECTED || millis() - lastPost < 3000) return;
  WiFiClientSecure secure; secure.setInsecure(); // Pro produkci nahraďte kořenovým CA certifikátem.
  HTTPClient http;
  String url = String(WEB_SERVER) + "/api/data";
  if (!http.begin(secure, url)) { state.status = "Nelze otevřít cloud API"; return; }
  http.addHeader("Content-Type", "application/json"); http.addHeader("X-API-Key", API_KEY);
  int code = http.POST(stateJson()); String response = code > 0 ? http.getString() : "";
  Serial.printf("POST /api/data -> %d\n", code);
  http.end(); lastPost = millis();
  if (code == 200) applyCommand(response);
  else if (code == 401) state.status = "Chybný DEVICE_API_KEY";
}

String bytesToHex(const String &data) {
  const char h[] = "0123456789ABCDEF"; String out; out.reserve(data.length() * 2);
  for (size_t i = 0; i < data.length(); ++i) { uint8_t b = data[i]; out += h[b >> 4]; out += h[b & 15]; }
  return out;
}

void updateLoRa() {
  if (state.mode != "lorascan" || !loraReady || digitalRead(14) != HIGH) return;
  String packet; int code = radio.readData(packet);
  if (code == RADIOLIB_ERR_NONE) {
    state.loraPackets++; state.loraRssi = radio.getRSSI(); state.loraSnr = radio.getSNR(); state.loraData = bytesToHex(packet);
    state.status = "LoRa paket přijat";
  }
  radio.startReceive();
}

void drawStatus() {
  static uint32_t last = 0; if (millis() - last < 1000) return; last = millis();
  tft.fillScreen(TFT_BLACK); tft.setTextColor(TFT_RED, TFT_BLACK); tft.setTextSize(2); tft.setCursor(10, 12); tft.println("MARAUDER CLOUD");
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(1); tft.setCursor(10, 45); tft.println(WiFi.localIP());
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(10, 68); tft.println(state.status.substring(0, 38));
  tft.setCursor(10, 92); tft.printf("WiFi: %d  LoRa: %lu", state.wifiCount, (unsigned long)state.loraPackets);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(10, 116); tft.println("SAFE CLOUD COMMANDS ONLY");
}

void setup() {
  Serial.begin(115200); pinMode(14, INPUT);
  tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
  connectWiFi();
  int code = radio.begin(868.0, 125.0, 9, 7, 0x12, 10, 8, 0);
  loraReady = code == RADIOLIB_ERR_NONE;
  if (!loraReady) state.status = "LoRa init chyba: " + String(code);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { WiFi.disconnect(); connectWiFi(); }
  updateLoRa(); postTelemetry(); drawStatus(); delay(5);
}
