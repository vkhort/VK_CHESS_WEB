#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "chess_classes.h"
#include "thc.h"

#ifdef sq
#undef sq
#endif

// === СТРУКТУРА ДЛЯ СОСТОЯНИЯ ИГРЫ ===
struct GameState {
    String fen;
    String lastMove;
    String allMoves;
    bool success;
    bool hasBotMove;
    
    GameState() : success(false), hasBotMove(false) {}
};

class ChessGame {
const String START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
private:
    bool playerIsWhite;
    String currentFEN;
    ChessBoard* board;
    InfoPanel* info;
    StatusString* status;
    int gameStatus;
    
    int firstTouchX;
    int firstTouchY;
    String lastMove;
    int gamePly;  // количество полуходов
    int aiLevel;
    
    // Lichess API поля
    String currentGameId;
    String lichessAnswer;     // Ответ от Lichess

public:
    ChessGame();
    
    void setAiLevel(int level) { aiLevel = level; }
    int getAiLevel() const { return aiLevel; }
    void init(ChessBoard& cb, InfoPanel& ip, StatusString& ss);
    void startNewGame(bool playerWhite);
    void makeMove(String moveUCI);

    bool isPlayerWhite() const { return playerIsWhite; }
    String getCurrentFEN() const { return currentFEN; }
    int getGameStatus() const { return gameStatus; }
    void setGameStatus(int status) { gameStatus = status; }
    String getLastMove() const { return lastMove; }
    
    // Для работы с касаниями
    bool hasFirstTouch() const { return firstTouchX != -1; }
    int getFirstTouchX() const { return firstTouchX; }
    int getFirstTouchY() const { return firstTouchY; }
    void setFirstTouch(int x, int y);
    void resetFirstTouch();
    bool firstFlow(int x, int y, ChessBoard& board, TFT_eSPI& tft);
    
    // Вспомогательные методы
    String coordinatesToUCI(int x, int y);
    bool isLegalMove(String moveUCI);
    
    // Lichess API методы
    void setCurrentGameId(String id) { currentGameId = id; }
    String getCurrentGameId() const { return currentGameId; }
    
    int sendRequest(String endpoint, String method, String payload);
    void printCurlCommand(String method, String endpoint, String payload);
    void streamGame();
    GameState parseGameState(String jsonLine);
 
    int getGamePly() const { return gamePly; }
    void incrementGamePly() { gamePly++; }
    void resetGamePly() { gamePly = 0; }

    DisplayManager& getDisplay();
#if !USE_ONLINE_API
//public:
//    void applyBotMove(GameState& state);
#endif

};

#endif