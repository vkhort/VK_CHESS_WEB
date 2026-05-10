#include "classes.h"
#include "chess_classes.h"

extern TouchManager touch;
// ==================== WorkLittleFS ====================

bool WorkLittleFS::saveFullConfig(const ConfigData &data) {
  return saveConfig(data);
}

WorkLittleFS::WorkLittleFS(const String& configFile)
  : configPath(configFile), tempPath(configFile + ".tmp") {}

bool WorkLittleFS::begin(bool formatOnFail) {
  return LittleFS.begin(formatOnFail);
}

bool WorkLittleFS::exists() {
  return LittleFS.exists(configPath);
}

bool WorkLittleFS::loadConfig(ConfigData &data) {
  if (!exists()) {
    Serial.println("[LittleFS] Config file not found");
    return false;
  }

  File file = LittleFS.open(configPath, "r");
  if (!file) return false;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("[LittleFS] JSON parse error");
    return false;
  }

  if (doc["ssid"].isNull() || doc["password"].isNull()) {
    Serial.println("[LittleFS] Missing required fields");
    return false;
  }

  data.ssid = doc["ssid"].as<String>();
  data.password = doc["password"].as<String>();
  data.email = doc.containsKey("email") ? doc["email"].as<String>() : String(DEFAULT_EMAIL);
  data.apiToken = doc.containsKey("apiToken") ? doc["apiToken"].as<String>() : String(API_LICHESS);  // ИСПОЛЬЗУЕМ DEFAULT
  data.tftRotation = doc.containsKey("tftRotation") ? doc["tftRotation"].as<int>() : 1;
  data.playerColor = doc.containsKey("playerColor") ? doc["playerColor"].as<int>() : 1;
  data.aiLevel = doc.containsKey("aiLevel") ? doc["aiLevel"].as<int>() : AI_LEVEL;

  Serial.println("[LittleFS] Config loaded successfully");
//  Serial.print("  SSID: "); Serial.println(data.ssid);
//  Serial.print("  API Token: "); Serial.println(data.apiToken);
  
  return true;
}

bool WorkLittleFS::saveConfig(const ConfigData &data) {
  File file = LittleFS.open(tempPath, "w");
  if (!file) return false;

  JsonDocument doc;  // ИЗМЕНЕНО: вместо StaticJsonDocument<512>
  doc["ssid"] = data.ssid;
  doc["password"] = data.password;
  doc["email"] = data.email;
  doc["tftRotation"] = data.tftRotation;
  doc["playerColor"] = data.playerColor;
  doc["apiToken"] = data.apiToken;
  doc["aiLevel"] = data.aiLevel;

if (serializeJson(doc, file) == 0) {
    file.close();
    LittleFS.remove(tempPath);
    return false;
  }
  file.close();

  if (LittleFS.exists(configPath) && !LittleFS.remove(configPath)) {
    LittleFS.remove(tempPath);
    return false;
  }
  if (!LittleFS.rename(tempPath, configPath)) {
    LittleFS.remove(tempPath);
    return false;
  }
  return true;
}

bool WorkLittleFS::saveWifiConfig(const String& ssid, const String& password, const String& email) {
  ConfigData data;
  loadConfig(data);
  data.ssid = ssid;
  data.password = password;
  data.email = email;
  // data.apiToken остаётся как был (не меняется при вызове этой функции)
  return saveConfig(data);
}

void WorkLittleFS::createDefaultConfig(ConfigData &data) {
  setDefaults(data);
  saveConfig(data);
}

bool WorkLittleFS::deleteConfig() {
  return LittleFS.remove(configPath);
}

size_t WorkLittleFS::getFileSize() {
  if (!exists()) return 0;
  File file = LittleFS.open(configPath, "r");
  if (!file) return 0;
  size_t size = file.size();
  file.close();
  return size;
}

void WorkLittleFS::setDefaults(ConfigData &data) {
  data.ssid = DEFAULT_SSID;
  data.password = DEFAULT_PASSWORD;
  data.email = DEFAULT_EMAIL;
  data.apiToken = API_LICHESS;
  data.tftRotation = 1;
  data.playerColor = 1;
}

// ==================== WiFiConnect ====================

WiFiConnect::WiFiConnect()
  : server(80), currentMode(Mode::DISCONNECTED), currentStatus(Status::IDLE),
    connectTimeout(15000), apSSID("VK_CHESS"), apPassword("87654321"),
    littleFSref(nullptr), displayRef(nullptr), apiToken(API_LICHESS),
    aiLevel(1) {}   // ← инициализация aiLevel

void WiFiConnect::init(DisplayManager& disp) {
  displayRef = &disp;
}

bool WiFiConnect::setupWiFi(WorkLittleFS& fs, WorkLittleFS::ConfigData& config, unsigned long timeoutMs) {
  littleFSref = &fs;
  targetSSID = config.ssid;
  targetPassword = config.password;
  currentEmail = config.email;
  apiToken = config.apiToken;
  connectTimeout = timeoutMs;
  
  if (connectToWiFi(targetSSID, targetPassword)) {
    currentMode = Mode::STA_MODE;
    currentStatus = Status::CONNECTED;
    
    server.on("/", std::bind(&WiFiConnect::handleRoot, this));
    server.on("/save", std::bind(&WiFiConnect::handleSave, this));
    server.onNotFound(std::bind(&WiFiConnect::handleNotFound, this));
    server.begin();
    Serial.println("Web server started on " + WiFi.localIP().toString());
    
    if (displayRef) {
      displayRef->drawConnectedScreen(WiFi.localIP().toString());
    }

    // КАЛИБРОВКА ТАЧСКРИНА
    // Нужно получить ссылку на TouchManager из основного файла
    // Пока сделаем через extern
//    extern TouchManager touch;
//    touch.calibrate(displayRef->getTft());
    
    return true;
  }
  
  Serial.println("Starting AP mode...");
  startAPMode();
  return false;
}

bool WiFiConnect::connectToWiFi(const String& ssid, const String& password) {
  if (displayRef) {
    displayRef->drawConnectingScreen(ssid);
  }
  
  Serial.print("Connecting to SSID: ");
  Serial.println(ssid);
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < connectTimeout) {
    delay(500);
    Serial.print(".");
    if (displayRef) {
      displayRef->drawConnectingDots();
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("Connection FAILED!");
    if (displayRef) {
      displayRef->drawConnectionFailedScreen();
    }
    return false;
  }
}

void WiFiConnect::startAPMode() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), apPassword.c_str());

  currentMode = Mode::AP_MODE;
  currentStatus = Status::AP_ACTIVE;

  server.on("/", std::bind(&WiFiConnect::handleRoot, this));
  server.on("/save", std::bind(&WiFiConnect::handleSave, this));
  server.onNotFound(std::bind(&WiFiConnect::handleNotFound, this));
  server.begin();
  
  if (displayRef) {
    displayRef->drawAPScreen();
  }
  
  Serial.println("==========================================");
  Serial.println("AP Mode started - WiFi connection FAILED");
  Serial.println("  SSID: " + apSSID);
  Serial.println("  Password: " + apPassword);
  Serial.println("  IP: " + WiFi.softAPIP().toString());
  Serial.println("  Open http://192.168.4.1 in browser");
  Serial.println("==========================================");
}

void WiFiConnect::handle() {
  // currentMode == STA_MODE, isConnected() == false
  if (currentMode == Mode::STA_MODE && !isConnected()) {
    return;  // ← ВЫХОДИМ, сервер НЕ РАБОТАЕТ!
  }
  server.handleClient();
}

void WiFiConnect::updateStatus() {
  if (displayRef) {
    displayRef->drawWifiStatus(isConnected(), getIPAddress(), currentMode == Mode::AP_MODE);
  }
}

void WiFiConnect::handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>VK CHESS Settings</title>
<style>
    body {
        font-family: Arial, sans-serif;
        background: #1a1a2e;
        color: white;
        margin: 0;
        padding: 20px;
    }
    .container {
        max-width: 500px;
        margin: 0 auto;
        background: #16213e;
        padding: 20px;
        border-radius: 10px;
    }
    h1 {
        text-align: center;
        color: #4CAF50;
    }
    .form-row {
        display: flex;
        gap: 15px;
        margin-bottom: 15px;
    }
    .form-group {
        flex: 1;
    }
    .form-group-full {
        margin-bottom: 15px;
    }
    label {
        display: block;
        margin-bottom: 5px;
        color: #4CAF50;
        font-weight: bold;
    }
    input, select {
        width: 100%;
        padding: 8px;
        border-radius: 5px;
        border: 1px solid #ccc;
        background: #0f3460;
        color: white;
        box-sizing: border-box;
    }
    input[type="number"] {
        -moz-appearance: textfield;
    }



/* Убираем стандартные стрелки */
input[type=number]::-webkit-inner-spin-button,
input[type=number]::-webkit-outer-spin-button {
  -webkit-appearance: none;
  margin: 0;
}
input[type=number] { -moz-appearance: textfield; }

/* Контейнер */
.win-input-container {
    display: flex;
    border: 1px solid #858585;
    border-radius: 4px;
    background: #fff;
    height: 32px;
    overflow: hidden;
    width: 100%;                     /* растянуть на всю ширину */
}

/* Подсветка всего контейнера при фокусе на инпуте */
.win-input-container:focus-within {
  border-color: #0067c0; /* Акцентный синий Windows */
  box-shadow: inset 0 0 0 1px #0067c0;
}

.win-input-container input {
    border: none;
    outline: none;
    padding: 0 10px;
    width: 100%;                     /* займёт всё свободное место */
    min-width: 60px;                 /* минимальная ширина */
    font-family: "Segoe UI", Tahoma, sans-serif;
    font-size: 14px;
}

/* Панель кнопок */
.win-controls {
    display: flex;
    flex-direction: column;
    width: 22px;
    border-left: 1px solid #e5e5e5;
    background: #f9f9f9;
    flex-shrink: 0;                 /* кнопки не сжимаются */
}

/* Кнопки */
.win-btn {
  flex: 1;
  border: none;
  background: transparent;
  cursor: pointer;
  position: relative;
  transition: background 0.1s;
}

/* Эффект наведения на кнопку */
.win-btn:hover {
  background: #e9e9e9;
}

/* Эффект клика */
.win-btn:active {
  background: #d1d1d1;
}

/* Рисуем стрелочки (иконки) */
.win-btn::after {
  content: '';
  position: absolute;
  left: 50%;
  top: 50%;
  width: 6px;
  height: 6px;
  border-left: 1px solid #333;
  border-top: 1px solid #333;
}

.up::after {
  transform: translate(-50%, -20%) rotate(45deg); /* Стрелка вверх */
}

.down::after {
  transform: translate(-50%, -80%) rotate(-135deg); /* Стрелка вниз */
}

/* Разделитель между кнопками */
.up {
  border-bottom: 1px solid #e5e5e5;
}







    .password-wrapper {
        position: relative;
        width: 100%;
    }
    .password-wrapper input {
        width: 100%;
        padding: 8px;
        padding-right: 40px;
        box-sizing: border-box;
    }
    .toggle-password {
        position: absolute;
        right: 10px;
        top: 50%;
        transform: translateY(-50%);
        background: none;
        border: none;
        color: #4CAF50;
        cursor: pointer;
        font-size: 18px;
        padding: 0;
        margin: 0;
        width: 30px;
        height: 30px;
        display: flex;
        align-items: center;
        justify-content: center;
    }
    button[type="submit"] {
        background: #4CAF50;
        color: white;
        padding: 10px 20px;
        border: none;
        border-radius: 5px;
        cursor: pointer;
        width: 100%;
        font-size: 16px;
    }
    button[type="submit"]:hover {
        background: #45a049;
    }
    .info {
        background: #0f3460;
        padding: 10px;
        border-radius: 5px;
        margin-top: 20px;
        font-size: 12px;
        text-align: center;
    }
</style>
</head>
<body>
<div class="container">
    <h1>♜ VK CHESS Settings</h1>
    <form action="/save" method="POST">
        <div class="form-group-full">
            <label>WiFi SSID</label>
            <input type="text" name="ssid" value="%SSID%" required>
        </div>
        <div class="form-group-full">
            <label>WiFi Password</label>
            <div class="password-wrapper">
                <input type="password" name="password" id="password" value="%PASSWORD%">
                <button type="button" class="toggle-password" onclick="togglePassword()">👁️</button>
            </div>
        </div>
        <div class="form-row">
            <div class="form-group">
                <label>Email</label>
                <input type="email" name="email" value="%EMAIL%" required>
            </div>
            <div class="form-group">
                <label>AI Level (1-8)</label>
                <div class="win-input-container">
                    <!-- Инпут остается функциональным -->
                    <input type="number" name="aiLevel" min="1" max="8" step="1" value="%AILEVEL%" required>
                    
                    <!-- Блок с новыми стрелками -->
                    <div class="win-controls">
                        <button type="button" class="win-btn up" 
                                onclick="this.parentElement.previousElementSibling.stepUp()">
                        </button>
                        <button type="button" class="win-btn down" 
                                onclick="this.parentElement.previousElementSibling.stepDown()">
                        </button>
                    </div>
                </div>
            </div>

            </div>
        <div class="form-group-full">
            <label>Lichess API Token</label>
            <input type="text" name="apiToken" value="%APITOKEN%" placeholder="lip_xxxxxxxxxxxxxxxx">
        </div>
        <div class="form-group-full">
            <button type="submit">Save & Reboot</button>
        </div>
    </form>
    <div class="info">After saving, device will reboot with new settings.</div>
</div>

<script>
function togglePassword() {
    var p = document.getElementById('password');
    var b = document.querySelector('.toggle-password');
    if (p.type === 'password') {
        p.type = 'text';
        b.innerHTML = '🙈';
    } else {
        p.type = 'password';
        b.innerHTML = '👁️';
    }
}
</script>
</body>
</html>
)rawliteral";

  // Замена плейсхолдеров
  html.replace("%SSID%", targetSSID);
  html.replace("%PASSWORD%", targetPassword);
  html.replace("%EMAIL%", currentEmail);
  html.replace("%APITOKEN%", apiToken);
  html.replace("%AILEVEL%", String(aiLevel));
  
  server.send(200, "text/html", html);
  Serial.println("[HTTP] Settings page sent");
}

void WiFiConnect::handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("password") || !server.hasArg("email")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }

  String newSSID = server.arg("ssid");
  String newPassword = server.arg("password");
  String newEmail = server.arg("email");
  String newApiToken = server.hasArg("apiToken") ? server.arg("apiToken") : "";  // ← ДОБАВИТЬ

  if (littleFSref) {
    // Загружаем текущий конфиг
    WorkLittleFS::ConfigData newConfig;
    littleFSref->loadConfig(newConfig);
    
    // Обновляем поля
    newConfig.ssid = newSSID;
    newConfig.password = newPassword;
    newConfig.email = newEmail;
    newConfig.apiToken = newApiToken;  // ← ДОБАВИТЬ
    
    // Сохраняем
    if (littleFSref->saveConfig(newConfig)) {
      String response = "<!DOCTYPE html><html><body><h1>Settings Saved!</h1>";
      response += "<p>Device will restart and connect to new WiFi...</p>";
      response += "</body></html>";
      server.send(200, "text/html", response);
      delay(1000);
      ESP.restart();
    } else {
      server.send(500, "text/plain", "Failed to save configuration");
    }
  } else {
    server.send(500, "text/plain", "LittleFS not available");
  }
}

void WiFiConnect::handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

String WiFiConnect::getIPAddress() {
  return WiFi.localIP().toString();
}

String WiFiConnect::getAPIPAddress() {
  return WiFi.softAPIP().toString();
}

bool WiFiConnect::isConnected() {
  return (WiFi.status() == WL_CONNECTED) && (currentMode == Mode::STA_MODE);
}

WiFiConnect::Mode WiFiConnect::getMode() {
  return currentMode;
}

WiFiConnect::Status WiFiConnect::getStatus() {
  return currentStatus;
}

String WiFiConnect::getCurrentSSID() {
  return targetSSID;
}

void WiFiConnect::disconnect() {
  WiFi.disconnect(true);
  currentMode = Mode::DISCONNECTED;
  currentStatus = Status::IDLE;
}

/*
void WiFiConnect::startWebServer(WebServer& srv) {
  if (isConnected()) {
    Serial.println("Starting web server in STA mode");
    srv.on("/", std::bind(&WiFiConnect::handleRoot, this));
    srv.on("/save", std::bind(&WiFiConnect::handleSave, this));
    srv.onNotFound(std::bind(&WiFiConnect::handleNotFound, this));
    srv.begin();
    Serial.print("Web server: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Starting web server in AP mode");
    apSSID = "WEB_TEST";
    apPassword = "12345678";
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    
    currentMode = Mode::AP_MODE;
    currentStatus = Status::AP_ACTIVE;
    
    srv.on("/", std::bind(&WiFiConnect::handleRoot, this));
    srv.on("/save", std::bind(&WiFiConnect::handleSave, this));
    srv.onNotFound(std::bind(&WiFiConnect::handleNotFound, this));
    srv.begin();
    
    Serial.println("AP Mode started");
    Serial.println("  SSID: " + apSSID);
    Serial.println("  Password: " + apPassword);
    Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

bool WiFiConnect::begin(const String& ssid, const String& password, unsigned long timeoutMs) {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    currentMode = Mode::STA_MODE;
    currentStatus = Status::CONNECTED;
    targetSSID = ssid;
    targetPassword = password;
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  
  currentMode = Mode::DISCONNECTED;
  currentStatus = Status::FAILED;
  Serial.println("Connection failed!");
  return false;
}
*/