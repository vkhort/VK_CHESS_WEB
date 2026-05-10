#include "chess_game.h"
#include "classes.h"           // ← ДОБАВИТЬ (для WorkLittleFS)
#include "config.h"

#if USE_ONLINE_API
    #include <WiFi.h>
    #include <WiFiClientSecure.h>
    #include <ArduinoJson.h>
#endif


ChessGame::ChessGame() 
    : playerIsWhite(true), currentFEN(""), 
      board(nullptr), info(nullptr), status(nullptr), gameStatus(0),
      firstTouchX(-1), firstTouchY(-1), lastMove(""),
      gamePly(0), aiLevel(AI_LEVEL) {}

void ChessGame::init(ChessBoard& cb, InfoPanel& ip, StatusString& ss) {
    board = &cb;
    info = &ip;
    status = &ss;
}

GameState ChessGame::parseGameState(String jsonLine) {
    GameState result;
    result.success = false;
    result.hasBotMove = false;
    
    // ========== ОТЛАДКА: выводим полученный JSON ==========
    Serial.println("=== parseGameState input JSON ===");
    Serial.println(jsonLine);
    // ====================================================
    
    // Парсим FEN (только для gameFull)
    int fenStart = jsonLine.indexOf("\"fen\":\"");
    if (fenStart != -1) {
        fenStart += 7;
        int fenEnd = jsonLine.indexOf("\"", fenStart);
        if (fenEnd != -1) {
            result.fen = jsonLine.substring(fenStart, fenEnd);
        }
    }
    
    // Парсим все ходы
    int movesStart = jsonLine.indexOf("\"moves\":\"");
    if (movesStart != -1) {
        movesStart += 9;
        int movesEnd = jsonLine.indexOf("\"", movesStart);
        if (movesEnd != -1) {
            result.allMoves = jsonLine.substring(movesStart, movesEnd);
            result.success = true;
            
            // ========== ОТЛАДКА: выводим извлечённые ходы ==========
//            Serial.print("All moves: ");
//            Serial.println(result.allMoves);
            // ======================================================
            
            // Определяем последний ход
            int lastSpace = result.allMoves.lastIndexOf(' ');
            String lastMoveUCI = (lastSpace != -1) ? result.allMoves.substring(lastSpace + 1) : result.allMoves;

            result.lastMove = lastMoveUCI;
            result.hasBotMove = true; // Мы всегда запоминаем последний ход...
       }
    }
    
    // Если FEN нет (это gameState), но есть ходы — генерируем новую FEN через thc
    if (result.fen.length() == 0 && result.allMoves.length() > 0) {
        thc::ChessRules cr;
        cr.Forsyth(START_FEN.c_str());   // всегда с начала        
        // ========== ОТЛАДКА: текущая FEN перед применением ходов ==========
//        Serial.print("Current FEN before applying moves: ");
//        Serial.println(currentFEN);
        // =================================================================
        
        // Применяем все ходы
        thc::Move mv;
        String moves = result.allMoves;
        int pos = 0;
        while (pos < moves.length()) {
            int space = moves.indexOf(' ', pos);
            String moveUCI = (space == -1) ? moves.substring(pos) : moves.substring(pos, space);
            if (moveUCI.length() >= 4) {
                // ========== ОТЛАДКА: выводим каждый ход ==========
//                Serial.print("Applying move: ");
//                Serial.println(moveUCI);
                // ================================================
                
                if (mv.NaturalIn(&cr, moveUCI.c_str())) {
                    cr.PlayMove(mv);
                } else {
                    // ========== ОТЛАДКА: ход не легален! ==========
//                   Serial.print("Move NOT legal: ");
//                    Serial.println(moveUCI);
                    // =============================================
                }
            }
            pos = (space == -1) ? moves.length() : space + 1;
        }
        result.fen = cr.ForsythPublish().c_str();
//Serial.print("Generated FEN from moves: ");
//Serial.println(result.fen);
    }
    
    // ========== ОТЛАДКА: итоговая FEN ==========
//    Serial.print("Result FEN: ");
//    Serial.println(result.fen);
    // ==========================================
    
    return result;
}

void ChessGame::streamGame() {
    extern WorkLittleFS::ConfigData config;
    extern WiFiConnect wifiConnect;
    
    if (currentGameId.length() == 0) {
        Serial.println("ERROR: No game ID for streaming!");
        return;
    }
    
    String endpoint = "/api/bot/game/stream/" + currentGameId;
    String line;
    bool botMoveReceived = false;
    GameState lastState;

#if USE_ONLINE_API
    // ========== ОНЛАЙН-РЕЖИМ ==========
    if (!wifiConnect.getClient().connected()) {
        wifiConnect.getClient().setInsecure();
        if (!wifiConnect.getClient().connect("lichess.org", 443)) {
            Serial.println("Stream connection failed!");
            return;
        }
    }
    
    String request = "GET " + endpoint + " HTTP/1.1\r\n";
    request += "Host: lichess.org\r\n";
    request += "Authorization: Bearer " + config.apiToken + "\r\n";
    request += "Accept: application/x-ndjson\r\n";
    request += "Connection: keep-alive\r\n";
    request += "\r\n";
    
    wifiConnect.getClient().print(request);
    
    // Пропускаем заголовки
    while (wifiConnect.getClient().connected()) {
        line = wifiConnect.getClient().readStringUntil('\n');
        if (line == "\r") break;
    }
    
    // Бесконечный цикл чтения потока
    while (wifiConnect.getClient().connected() && !botMoveReceived) {
        if (wifiConnect.getClient().available()) {
            line = wifiConnect.getClient().readStringUntil('\n');
            line.trim();
#else
    // ========== ЭМУЛЯЦИЯ ==========
    Serial.println("=== EMULATION: Reading JSON from Serial ===");
    Serial.println("---- ВВЕДИТЕ JSON ОТВЕТ ОТ LICHESS -----");
    while (!Serial.available()) delay(10);
    line = Serial.readStringUntil('\n');
    line.trim();
#endif

            // ========== ОБЩАЯ ОБРАБОТКА line ==========
            if (line.length() > 0 && line[0] == '{') {
                GameState state = parseGameState(line);

                // === ГЛАВНОЕ ИЗМЕНЕНИЕ: проверяем, что это НЕ наш ход ===
                if (state.success && state.hasBotMove && state.lastMove != lastMove) {
//                    Serial.print("Bot move detected: ");
//                    Serial.println(state.lastMove);
                    // === ДОБАВЛЯЕМ ХОД БОТА В ИСТОРИЮ ===
#if USE_ONLINE_API
Serial.println("=== STREAM JSON ===");
Serial.println(line);
#endif
                    // Сохраняем состояние для последующей обработки
                    lastState = state;
                    botMoveReceived = true;
                    
                    if (info) {
                        int moveNumber = (gamePly / 2) + 1;
                        if (playerIsWhite) {
                            // Игрок белыми, бот чёрными
                            info->addBlackMove(state.lastMove);
                        } else {
                            // Игрок чёрными, бот белыми
                            info->addWhiteMove(moveNumber, state.lastMove);
                        }
                        incrementGamePly();
                   }
                    // ===================================

#if USE_ONLINE_API
                    // Закрываем соединение после получения хода бота
                    break;
#endif
                }
                // ====================================================
            }

#if USE_ONLINE_API
        }
        delay(10);
    }
    wifiConnect.getClient().stop();
//    Serial.println("Stream ended");
#else
    // В эмуляции просто выходим
    Serial.println("Emulation: JSON processed");
#endif

    // ========== ОБНОВЛЕНИЕ ДОСКИ И СТАТУСА (ОДИН РАЗ) ==========
    if (botMoveReceived) {
        // Применяем ход бота
//        if (board) {
//            board->applyAiMove(lastState.lastMove);
//        }

        // Обновляем FEN и доску
        if (lastState.fen.length() > 0) {
            currentFEN = lastState.fen;

            if (board) {
//                Serial.println("Calling board->fenToBoard()...");
                board->fenToBoard(currentFEN);
//                Serial.println("Calling board->draw()...");
                board->draw();
//                Serial.println("Board draw completed");
            } else {
                Serial.println("❌ board is NULL!");
            }
        }
Serial.print("✅ currentFEN updated: ");
Serial.println(currentFEN);
//Serial.print("status pointer: ");
//Serial.println((uint32_t)status);

        // ========== ОБНОВЛЕНИЕ СТАТУСА ==========
        if (status) {
            status->updateState("YOUR MOVE", TFT_GREEN);
            status->updateFreeMem();
        } else {
            Serial.println("status is NULL!");
        }
    }
}

void ChessGame::printCurlCommand(String method, String endpoint, String payload) {
    extern WorkLittleFS::ConfigData config;
    Serial.println("\n=== MANUAL CURL COMMAND ===");
    Serial.print("curl -X ");
    Serial.print(method);
    Serial.print(" \"https://lichess.org");
    Serial.print(endpoint);
    Serial.print("\" -H \"Authorization: Bearer ");
    Serial.print(config.apiToken);
    Serial.print("\" -H \"Content-Type: application/json\"");
    
    if (payload.length() > 0) {
        Serial.print(" -d \"");
        Serial.print(payload);
        Serial.print("\"");
    }
    
    Serial.println("\n===========================\n");
}


void ChessGame::startNewGame(bool playerWhite) {
    playerIsWhite = playerWhite;
    extern WorkLittleFS::ConfigData config;
    currentFEN = START_FEN; // "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    String endpoint = "/api/challenge/ai";
    String payload = "variant=standard&rated=false&level=" + String(aiLevel);
    int code;
    resetGamePly();
    
#if USE_ONLINE_API
    // ========== СОЗДАЁМ ИГРУ НА LICHESS ==========
    Serial.println("=== CREATING NEW GAME ON LICHESS ===");
    
    if (playerIsWhite) {
        payload += "&color=white";
        Serial.println("Player: WHITE");
    } else {
        payload += "&color=black";
        Serial.println("Player: BLACK");
    }
    
    Serial.print("Endpoint: "); Serial.println(endpoint);
    Serial.print("Payload: "); Serial.println(payload);
    Serial.print("Token being sent: ");
    Serial.println(config.apiToken);
#else
        // ========== ОФЛАЙН-РЕЖИМ (эмуляция) ==========
        Serial.println("EMULATION MODE: Starting local game");
        
        // Фиктивный вызов sendRequest для получения Game ID
        payload = "";
#endif
        code = sendRequest(endpoint, "POST", payload);
        // Временно добавим вывод ответа
        Serial.print("Response code: ");
        Serial.println(code);
        Serial.print("Response body: ");
        Serial.println(lichessAnswer);

        if (code == 200) {
        if (lichessAnswer.length() > 0) {
            int start = lichessAnswer.indexOf("\"id\":\"");
            if (start != -1) {
                start += 6;
                int end = lichessAnswer.indexOf("\"", start);
                if (end != -1) {
                    currentGameId = lichessAnswer.substring(start, end);
                    Serial.print("Game ID: ");
                    Serial.println(currentGameId);
                    // Выводим ссылку на игру в браузере
                    Serial.print("Game URL: https://lichess.org/");
                    Serial.println(currentGameId);
                    // Выводим curl команду для ручной проверки
                    printCurlCommand("POST", endpoint, payload);
                } else {
                    Serial.println("ERROR: Failed to parse game ID");
                    gameStatus = 0;
                    extern DisplayManager display;
                    display.showMenu();
                    return;
                }
            } else {
                Serial.println("ERROR: No game ID in response");
                gameStatus = 0;
                extern DisplayManager display;
                display.showMenu();
                return;
            }
        } else {
            Serial.println("ERROR: Empty response from Lichess");
            gameStatus = 0;
            extern DisplayManager display;
            display.showMenu();
            return;
        }
    } else {
        Serial.print("Failed to create game. HTTP code: ");
        Serial.println(code);
        gameStatus = 0;
        extern DisplayManager display;
        display.showMenu();
        return;
    }
    // ============================================
    
    // Запускаем локальную игру
    gameStatus = 1;
    extern DisplayManager display;
    display.startGame(*board, *info, *status, playerIsWhite);

    // ========== ЕСЛИ ИГРОК ЧЁРНЫМИ – ЖДЁМ ПЕРВЫЙ ХОД БОТА ==========
    if (!playerIsWhite) {
        Serial.println("Waiting for bot's first move...");
        streamGame();   // получаем и отображаем первый ход бота
    }
    // =============================================================
}

int ChessGame::sendRequest(String endpoint, String method, String payload) {
    extern WiFiConnect wifiConnect;
    extern WorkLittleFS::ConfigData config;

    // Определяем Content-Type для вывода curl
    String contentType = (endpoint == "/api/challenge/ai") ? "application/x-www-form-urlencoded" : "application/json";

/*// ВЫВОД CURL КОМАНДЫ В САМОМ НАЧАЛЕ
Serial.println("\n=== CURL COMMAND FOR DEBUG ===");
Serial.print("curl -X ");
Serial.print(method);
Serial.print(" \"https://lichess.org");
Serial.print(endpoint);
Serial.print("\" -H \"Authorization: Bearer ");
Serial.print(config.apiToken);
Serial.print("\" -H \"Content-Type: ");
Serial.print(contentType);
Serial.print("\"");
if (payload.length() > 0) {
    Serial.print(" -d \"");
    Serial.print(payload);
    Serial.print("\"");
}
Serial.println("\n================================");
*/
    
#if USE_ONLINE_API
    // ========== РЕАЛЬНЫЙ ЗАПРОС К LICHESS ==========
    if (!wifiConnect.isConnected()) {
        Serial.println("No WiFi connection!");
        return -1;
    }

    wifiConnect.getClient().setInsecure();

    if (!wifiConnect.getClient().connect("lichess.org", 443)) {
        Serial.println("Connection failed!");
        return -1;
    }
    
    // Определяем Content-Type для запроса
    String reqContentType;
    if (endpoint == "/api/challenge/ai") {
        reqContentType = "application/x-www-form-urlencoded";
    } else {
        reqContentType = "application/json";
    }
    
    String request = method + " " + endpoint + " HTTP/1.1\r\n";
    request += "Host: lichess.org\r\n";
    request += "Authorization: Bearer " + config.apiToken + "\r\n";
    request += "Content-Type: " + reqContentType + "\r\n";
    request += "Content-Length: " + String(payload.length()) + "\r\n";
    request += "\r\n";
    request += payload;

//Serial.println("=== SENDING REQUEST ===");
//Serial.println(request);
    
    wifiConnect.getClient().print(request);

    // Ждём ответ
    unsigned long timeout = millis();
    while (!wifiConnect.getClient().available()) {
        if (millis() - timeout > 10000) {
            Serial.println("Timeout waiting for response");
            wifiConnect.getClient().stop();
            return -1;
        }
        delay(10);
    }

    // Читаем заголовки
    while (wifiConnect.getClient().connected()) {
        String line = wifiConnect.getClient().readStringUntil('\n');
//        Serial.println(line);
        if (line == "\r") break;
    }

    // Читаем тело ответа
    lichessAnswer = "";
    while (wifiConnect.getClient().available()) {
        lichessAnswer += (char)wifiConnect.getClient().read();
    }

//Serial.println("=== RESPONSE BODY ===");
//Serial.println(lichessAnswer);

    wifiConnect.getClient().stop();
    
#else
    // ========== ЭМУЛЯЦИЯ ==========
    Serial.println("=== EMULATION MODE ===");
    Serial.println("Request would be sent to: " + endpoint);
    
    // Просто читаем JSON из Serial
    Serial.println("---- ВВЕДИТЕ JSON ОТВЕТ ОТ LICHESS -----");
    while (!Serial.available()) delay(10);
    lichessAnswer = Serial.readStringUntil('\n');
#endif

    lichessAnswer.trim();
    return 200;
}


void ChessGame::makeMove(String moveUCI) {
    Serial.print("Making move: ");
    Serial.println(moveUCI);
    
    // 1. Проверяем легальность хода через THC
    if (!isLegalMove(moveUCI)) {
        Serial.println("Illegal move!");
        if (status && board) {
            status->updateState("ILLEGAL MOVE!", TFT_YELLOW);
            status->updateFreeMem();
            delay(1000);
            status->updateState("YOUR MOVE", TFT_GREEN);
            status->updateFreeMem();
        }
        return;
    }
    
    // === ДОБАВЛЯЕМ ХОД В ИСТОРИЮ ===
    if (info) {
        int moveNumber = (gamePly / 2) + 1;
        if (playerIsWhite) {
            info->addWhiteMove(moveNumber, moveUCI);
        } else {
            info->addBlackMove(moveUCI);
        }
        incrementGamePly();
    }
    // ================================
    
    // ========== ДОБАВИТЬ ЭТОТ БЛОК ==========
    // 2. Обновляем локальную доску после легального хода
    if (board) {
        board->fenToBoard(currentFEN);
        board->draw();   // ← фигура двигается
    }
    // ======================================

    // 3. Снимаем подсветку выбранной клетки
    if (board) {
        int oldX = getFirstTouchX();
        int oldY = getFirstTouchY();
        board->drawCell(oldX, oldY, false);
        board->selectedX = -1;   // ← ДОБАВИТЬ
        board->selectedY = -1;   // ← ДОБАВИТЬ
        resetFirstTouch();
    }    

    // 4. Отправляем ход на Lichess
    if (currentGameId.length() == 0) {
        Serial.println("ERROR: No game ID!");
        return;
    }
    
    String endpoint = "/api/bot/game/" + currentGameId + "/move/" + moveUCI;
    
    printCurlCommand("POST", endpoint, "");
    int code = 200;
#if USE_ONLINE_API
    code = sendRequest(endpoint, "POST", "");
#endif

    if (code != 200) {
        Serial.print("Failed to send move. HTTP code: ");
        Serial.println(code);
        return;
    }
    
    lastMove = moveUCI;
    
    // 5. Ждём ответа бота через поток
    streamGame();
    
    // 6. Обновляем статус
    if (status) {
        status->updateState("YOUR MOVE", TFT_GREEN);
        status->updateFreeMem();
    } else {
        Serial.println("status is NULL!");
    }
}

String ChessGame::coordinatesToUCI(int x, int y) {
    char file = 'a' + x;
    int rankNumber = 8 - y;
    char rank = '0' + rankNumber;
    return String(file) + String(rank);
}

bool ChessGame::isLegalMove(String moveUCI) {

    // Создаём временный объект движка для проверки
    thc::ChessRules cr;
    
//    Serial.print("=== isLegalMove called ===");
//    Serial.print("Current FEN for legality check: ");
//    Serial.println(currentFEN);

    // 1. Устанавливаем позицию из текущего FEN
    cr.Forsyth(currentFEN.c_str());

    // 2. Создаём объект хода
    thc::Move mv;
    
    // 3. Проверяем ход (NaturalIn проверяет и формат, и легальность)
    if (mv.NaturalIn(&cr, moveUCI.c_str())) {
        // 5. Выполняем ход на движке
        cr.PlayMove(mv);
        // 6. Получаем новую FEN после выполнения хода
        currentFEN = cr.ForsythPublish().c_str();
        return true;
    }

    Serial.print("Illegal move: ");
    Serial.println(moveUCI);
    return false;
}

void ChessGame::setFirstTouch(int x, int y) {
    firstTouchX = x;
    firstTouchY = y;
    Serial.print("First touch: ");
    Serial.println(coordinatesToUCI(x, y));
}

void ChessGame::resetFirstTouch() {
    firstTouchX = -1;
    firstTouchY = -1;
}

bool ChessGame::firstFlow(int x, int y, ChessBoard& board, TFT_eSPI& tft) {
    char piece = board.board[x][y];
    
    if (piece == ' ') {
        Serial.println("Empty cell - cannot select");
        resetFirstTouch();
        return false;
    }
    
    bool isWhitePiece = isupper(piece);
    bool isPlayerPiece = (playerIsWhite && isWhitePiece) || (!playerIsWhite && !isWhitePiece);
    
    if (isPlayerPiece) {
        setFirstTouch(x, y);
        board.selectedX = x;
        board.selectedY = y;
        board.drawCell(x, y, true);
//        Serial.print("Selected: ");
//        Serial.println(coordinatesToUCI(x, y));
        return true;
    } else {
        Serial.println("Not your piece!");
        resetFirstTouch();
        board.selectedX = -1;
        board.selectedY = -1;
        
        if (status) {
            status->updateState("NOT YOUR PIECE!", TFT_YELLOW);
            delay(800);
            status->updateState("YOUR MOVE", TFT_GREEN);
        }
        return false;
    }
}
/*
void ChessGame::processMoveChain() {
    if (firstTouchX == -1 || secondTouchX == -1) {
        Serial.println("Error: touches not set");
        resetTouches();
        if (board) {
            board->selectedX = -1;
            board->selectedY = -1;
        }
        return;
    }
    
    String moveUCI = coordinatesToUCI(firstTouchX, firstTouchY) + 
                     coordinatesToUCI(secondTouchX, secondTouchY);
    
    Serial.print("MOVE UCI: ");
    Serial.println(moveUCI);
    
    // Снимаем подсветку с первой клетки
    if (board) {
        board->drawCell(firstTouchX, firstTouchY, false);
        board->selectedX = -1;
        board->selectedY = -1;
    }
    
    // Проверяем легальность хода
    if (isLegalMoveUCI(moveUCI)) {
        makeMove(moveUCI);
    } else {
        Serial.println("Illegal move!");
        if (status && board) {
            status->update(board->getTft(), "ILLEGAL MOVE!", TFT_YELLOW);
            delay(1000);
            status->update(board->getTft(), "YOUR MOVE", TFT_GREEN);
        }
    }
    
    resetTouches();
}

bool ChessGame::isLegalMoveUCI(String moveUCI) {
    // Пока заглушка - все ходы легальны
    // TODO: добавить проверку через thc
    Serial.print("Checking legality (stub): ");
    Serial.println(moveUCI);
    return true;  // временно
}
*/