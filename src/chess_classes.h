#ifndef CHESS_CLASSES_H
#define CHESS_CLASSES_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"

// Класс для нижней статусной строки (самый низ экрана)
/*
class StatusString {
public:
    void update(TFT_eSPI &tft, String message, uint16_t color);
    void updateWithMemory(TFT_eSPI &tft, String message, uint16_t color);
};
*/
class StatusString {
private:
    String currentState;    // Текущее сообщение (например "YOUR MOVE")
    String freeMem;         // Строка с свободной памятью (например "FM 245672")
    uint16_t currentColor;  // Цвет текущего сообщения
    TFT_eSPI* tft;          // Указатель на дисплей
    
public:
    StatusString();
    void setTft(TFT_eSPI& display) { tft = &display; }
    void updateState(String message, uint16_t color);
    void updateFreeMem();
};

// Класс для панели с ходами (над статусной строкой)
class InfoPanel {
private:
    String history[5];
    int historyCount = 0;
    int pendingMoveNumber = 0;
    String pendingWhiteMove = "";
    String pendingBlackMove = "";
    TFT_eSPI* tft;
    
public:
    void setTft(TFT_eSPI& display) { tft = &display; }
    void addWhiteMove(int moveNum, String move);
    void addBlackMove(String move);
    void clearHistory() { historyCount = 0; }
    void init(TFT_eSPI &tft);
    void drawControls(TFT_eSPI &tft, int depth);
};

class TouchManager {
private:
    XPT2046_Touchscreen* ts;
    SPIClass mySpi;  // Добавляем SPI объект
    bool initialized;
public:
    int x, y;  // ← ПУБЛИЧНЫЕ ПЕРЕМЕННЫЕ
    TouchManager(uint8_t csPin);
    bool init();
    void calibrate(TFT_eSPI &tft);
    bool read();
    TouchZone getZone();
};

class ChessBoard {
public:
    char board[8][8];
    int8_t pieceColors[8][8];
    int selectedX, selectedY;
    int blinkX = -1;
    int blinkY = -1;
    bool blinkState = false;

    ChessBoard();
    bool isPlayerPiece(char piece);  // ← убрать второй параметр
    void init();
    void fenToBoard(String fen);
    void draw();
    void drawCell(int x, int y, bool highlight);
    String getPieceSymbol(char piece);
    void handleTouch(int tx, int ty, InfoPanel &info, StatusString &status);
    
    char getPieceChar(char piece);
    String getChessNotation(int x, int y);
//    void applyAiMove(String move);

    void setTft(TFT_eSPI& tftRef) { tft = &tftRef; }
    TFT_eSPI& getTft() { return *tft; }
    
private:
    TFT_eSPI* tft;
};

// --- ОБНОВЛЕННЫЙ: DisplayManager ---
class DisplayManager {
private:
    TFT_eSPI tft;
    
    // Переменные для анимации точек
    int dotX = 0;
    int dotY = 0;
    int dotStage = 0;
    int dotCount = 0;
    
public:
    DisplayManager();
    void init();
    void clearMenu();
    void showMenu();
    void pressStartButton(int btnIdx);
    void drawButton(int x, int y, String label, uint16_t kingColor, uint16_t bgColor);
    TFT_eSPI& getTft();
    void startGame(ChessBoard &cb, InfoPanel &ip, StatusString &ss, bool playerIsWhite);
    
    // НОВЫЕ МЕТОДЫ ДЛЯ ОТОБРАЖЕНИЯ СТАТУСА WiFi
    void drawAPScreen();
    void drawConnectedScreen(const String& ip);
    void drawWifiStatus(bool isConnected, const String& ip, bool isAPMode);
    void drawConnectingScreen(const String& ssid);
    void drawConnectingDots();
    void drawConnectionFailedScreen();
    
    // Обновление статусной строки
    void updateWifiStatus(bool isConnected, const String& ip, bool isAPMode);
};

#endif
