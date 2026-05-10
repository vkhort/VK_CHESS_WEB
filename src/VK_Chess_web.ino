#include <Arduino.h>

// СНАЧАЛА config.h (где определён TOUCH_CS)
#include "config.h"

// ПОТОМ библиотеки, которые используют TOUCH_CS
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "chess_game.h"

// ПОТОМ наши заголовки
#include "chess_classes.h"
#include "classes.h"

// Глобальный объект тачскрина
XPT2046_Touchscreen ts(TOUCH_CS);

// Глобальные объекты
DisplayManager display;
TouchManager touch(TOUCH_CS);  // ← ТОЛЬКО ПИН CS
WorkLittleFS littleFS;
WiFiConnect wifiConnect;
WorkLittleFS::ConfigData config;

ChessBoard myGame;
ChessGame chessGame;  // ← ДОБАВИТЬ
InfoPanel info;
StatusString status;

// Флаг начала игры
bool gameStarted = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== VK CHESS STARTING ===");
  
  // 1. Инициализация дисплея
  display.init();
  status.setTft(display.getTft());
  myGame.setTft(display.getTft());  // Передаём TFT в шахматную доску для отрисовки
  myGame.init();  // Инициализируем шахматную доску (устанавливаем начальную позицию)
  
#if USE_ONLINE_API
  // 2. Инициализация WiFiConnect с ссылкой на DisplayManager
  wifiConnect.init(display);  // ← ПЕРЕДАЁМ display, НЕ tft
  // 3. Инициализация HTTPS клиента (один раз)
  wifiConnect.getClient().setInsecure();
#endif

wifiConnect.setAiLevel(config.aiLevel);
  // 3. Инициализация тачскрина (если нужен)
  if (!touch.init()) {
    Serial.println("Warning: Touch screen not available");
  }
  
#if USE_ONLINE_API
  // 4. Инициализация LittleFS
  if (!littleFS.begin(true)) {
    display.getTft().fillScreen(TFT_BLACK);
    display.getTft().setTextColor(TFT_RED, TFT_BLACK);
    display.getTft().drawString("LittleFS FAILED!", 40, 150, 2);
    Serial.println("LittleFS mount failed!");
    while (true) delay(100);
  }
  Serial.println("LittleFS mounted");
  
  // 5. Загрузка или создание конфигурации
  if (!littleFS.loadConfig(config)) {
    littleFS.createDefaultConfig(config);
    Serial.println("Created default config");
  }
  Serial.print("Config loaded: SSID=");
  Serial.println(config.ssid);
  Serial.print("API Token: ");
  Serial.println(config.apiToken);

  // 6. Настройка WiFi (подключение или AP режим)
  wifiConnect.setupWiFi(littleFS, config, WIFI_CONNECT_TIMEOUT);

  // 7. Инициализация шахматной игры
  chessGame.init(myGame, info, status);
  chessGame.setGameStatus(0);  // меню
  
  // 8. Показываем главное меню
  display.showMenu();
  
//Serial.println("=== STARTING AP MODE DIRECTLY ===");
//wifiConnect.apSSID = "VK_CHESS";
//wifiConnect.apPassword = "87654321";
//wifiConnect.startAPMode();

  // 9. Показываем главное меню (закомментировано, так как уже есть)
  //tft.fillScreen(TFT_BLACK);
  //tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.setTextSize(2);
  //tft.drawString("VK CHESS", 70, 100, 4);
  //tft.drawString("Touch to start", 60, 200, 2);

  wifiConnect.updateStatus();  // ← ОБНОВЛЯЕМ СТАТУСНУЮ СТРОКУ
#else
  // ========== ОФЛАЙН-РЕЖИМ (эмуляция) ==========
  Serial.println("EMULATION MODE: WiFi disabled, using local engine");
  
  // Инициализация игры (офлайн)
  chessGame.init(myGame, info, status);
  chessGame.setGameStatus(0);  // меню
  display.showMenu();
#endif
  
  Serial.println("Setup completed!");
}

void loop() {
#if USE_ONLINE_API
  wifiConnect.handle();
  delay(10);

  if (!wifiConnect.isConnected() && wifiConnect.getMode() != WiFiConnect::Mode::AP_MODE) {
    delay(50);
    return;
  }
#endif

  if (touch.read()) {
//    Serial.print("Touch: X=");
//    Serial.print(touch.x);
//    Serial.print(" Y=");
//    Serial.println(touch.y);
    
    if (chessGame.getGameStatus() == 0) {
      // ========== РЕЖИМ МЕНЮ (выбор цвета) ==========
      if (touch.y > 100 && touch.y < 145 && touch.x > 40 && touch.x < 200) {
          display.pressStartButton(0);
          delay(200);
          chessGame.startNewGame(true);
      }
      // PLAY BLACK
      else if (touch.y > 160 && touch.y < 205 && touch.x > 40 && touch.x < 200) {
          display.pressStartButton(1);
          delay(200);
          chessGame.startNewGame(false);
      }
      // Кнопка PUZZLES (пока не реализована)
      else if (touch.y > 220 && touch.y < 265 && touch.x > 40 && touch.x < 200) {
        display.pressStartButton(2);
        delay(200);
        Serial.println("PUZZLES mode - coming soon");
      }
    } 
    else if (chessGame.getGameStatus() == 1) {
      // ========== РЕЖИМ ИГРЫ ==========
      // Передаём касание в шахматную доску
      myGame.handleTouch(touch.x, touch.y, info, status);
    }
  }
}