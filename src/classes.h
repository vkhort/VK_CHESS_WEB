#ifndef CLASSES_H
#define CLASSES_H

#include "config.h"          // ← ДОБАВИТЬ ЭТУ СТРОКУ
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#if USE_ONLINE_API
    #include <WiFiClientSecure.h>
#endif
//#include <XPT2046_Touchscreen.h>

// Forward declaration
class DisplayManager;

// ==================== КЛАСС ДЛЯ РАБОТЫ С LITTLEFS ====================
class WorkLittleFS {
private:
  String configPath;
  String tempPath;
  
public:
  struct ConfigData {
    String ssid;
    String password;
    String email;
    String apiToken;
    int tftRotation;
    int playerColor;
    int aiLevel;          // уровень AI (1-8, по умолчанию AI_LEVEL 1)
    
    ConfigData() : ssid(""), password(""), email(""),
                   apiToken(""), tftRotation(0), playerColor(1), aiLevel(AI_LEVEL) {}
  };
  
  WorkLittleFS(const String& configFile = "/chess_config.json");
  bool begin(bool formatOnFail = true);
  bool exists();
  
  bool loadConfig(ConfigData &data);
  bool saveConfig(const ConfigData &data);
  bool saveWifiConfig(const String& ssid, const String& password, const String& email);
  bool saveFullConfig(const ConfigData &data);
  void createDefaultConfig(ConfigData &data);
  bool deleteConfig();
  size_t getFileSize();
  void setDefaults(ConfigData &data);
};

// ==================== КЛАСС ДЛЯ ПОДКЛЮЧЕНИЯ К WIFI ====================
class WiFiConnect {
public:
    enum class Mode { STA_MODE, AP_MODE, DISCONNECTED };
    enum class Status { IDLE, CONNECTING, CONNECTED, FAILED, AP_ACTIVE };
    
    WiFiConnect();
    
    void init(DisplayManager& disp);
    bool setupWiFi(WorkLittleFS& fs, WorkLittleFS::ConfigData& config, unsigned long timeoutMs = 15000);
    void handle();
    void updateStatus();
    
    void setAiLevel(int level) {aiLevel = level; };
    String getIPAddress();
    String getAPIPAddress();
    bool isConnected();
    Mode getMode();
    Status getStatus();
    String getCurrentSSID();
    void disconnect();
#if USE_ONLINE_API
    WiFiClientSecure& getClient() { return client; }
#endif

private:
    WebServer server;
    Mode currentMode;
    Status currentStatus;
    unsigned long connectTimeout;
    String targetSSID;
    String targetPassword;
    String currentEmail;
    String apiToken;
    String apSSID;
    String apPassword;
    int aiLevel;
    
    WorkLittleFS* littleFSref;
    DisplayManager* displayRef;
    
    bool connectToWiFi(const String& ssid, const String& password);
    void startAPMode();
    
    void handleRoot();
    void handleSave();
    void handleNotFound();
#if USE_ONLINE_API
    WiFiClientSecure client;
#endif
};

#endif // CLASSES_H