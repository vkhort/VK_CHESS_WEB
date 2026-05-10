#include "chess_classes.h"
#include "ChessCuernavaca28.h"
#include "chess_game.h"      // ← ДОБАВИТЬ
#include <esp_heap_caps.h>

extern ChessGame chessGame;

int DEPTH = INITIAL_DEPTH;

// --- StatusString ---

StatusString::StatusString() : currentState(""), freeMem(""), currentColor(TFT_GREEN), tft(nullptr) {}

void StatusString::updateState(String message, uint16_t color) {
    if (!tft) return;
    
    currentState = message;
    currentColor = color;

    // Очищаем левую часть статусной строки (там где текст)
    tft->fillRect(0, STATUS_Y, SCREEN_WIDTH / 2, STATUS_HEIGHT, TFT_BLACK);
    tft->drawFastHLine(0, STATUS_Y, SCREEN_WIDTH, TFT_DARKGREY);
    
    // Выводим новое сообщение
    tft->setTextColor(currentColor, TFT_BLACK);
    tft->setTextSize(1);
    tft->setCursor(5, STATUS_Y + 5);
    tft->print(currentState);
}

void StatusString::updateFreeMem() {
    if (!tft) return;
    
    freeMem = "FM " + String(ESP.getFreeHeap());
    
    // Очищаем правую часть статусной строки (там где память)
    int memWidth = freeMem.length() * 6;
    tft->fillRect(SCREEN_WIDTH - memWidth - 10, STATUS_Y, memWidth + 10, STATUS_HEIGHT, TFT_BLACK);
    tft->drawFastHLine(0, STATUS_Y, SCREEN_WIDTH, TFT_DARKGREY);
    
    // Выводим новое значение памяти
    tft->setCursor(SCREEN_WIDTH - memWidth - 5, STATUS_Y + 5);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->print(freeMem);
}

// --- InfoPanel ---
void InfoPanel::init(TFT_eSPI &tft) {
    tft.fillRect(0, PANEL_Y, SCREEN_WIDTH, INFO_HEIGHT, COLOR_PANEL);
    tft.drawFastHLine(0, PANEL_Y, SCREEN_WIDTH, TFT_SILVER);
}

void InfoPanel::addWhiteMove(int moveNum, String move) {
    pendingMoveNumber = moveNum;
    pendingWhiteMove = move;
    
    // Сразу добавляем в историю (без хода чёрных)
    String newEntry = String(moveNum) + ". " + move;
    
    if (historyCount < 5) {
        history[historyCount] = newEntry;
        historyCount++;
    } else {
        for (int i = 0; i < 4; i++) {
            history[i] = history[i+1];
        }
        history[4] = newEntry;
    }
    
    // Перерисовываем
    if (tft) {
        tft->fillRect(2, PANEL_Y + 2, 138, 51, 0x2104);
        tft->setTextColor(TFT_WHITE, 0x2104);
        for (int i = 0; i < historyCount; i++) {
            String displayEntry = history[i];
            // Можно добавить отступ для незавершённых ходов, но пока так
            tft->drawString(displayEntry, 5, PANEL_Y + 5 + (i * 10), 1);
        }
    }
}

void InfoPanel::addBlackMove(String move) {
    pendingBlackMove = move;
    
    // Обновляем последнюю запись в истории
    if (historyCount > 0) {
        String updatedEntry = String(pendingMoveNumber) + ". " + pendingWhiteMove + "  " + move;
        history[historyCount - 1] = updatedEntry;
    }
    
    // Перерисовываем
    if (tft) {
        tft->fillRect(2, PANEL_Y + 2, 138, 51, 0x2104);
        tft->setTextColor(TFT_WHITE, 0x2104);
        for (int i = 0; i < historyCount; i++) {
            tft->drawString(history[i], 5, PANEL_Y + 5 + (i * 10), 1);
        }
    }
    
    pendingWhiteMove = "";
    pendingBlackMove = "";
    pendingMoveNumber = 0;
}

void InfoPanel::drawControls(TFT_eSPI &tft, int depth) {
    int ctrlX = 145;
    tft.setTextColor(TFT_GOLD, COLOR_PANEL);
    tft.drawString("<", ctrlX, PANEL_Y + 10, 2);
    tft.drawNumber(depth, ctrlX + 25, PANEL_Y + 10, 2);
    tft.drawString(">", ctrlX + 55, PANEL_Y + 10, 2);

    tft.drawRect(ctrlX, PANEL_Y + 30, 40, 20, TFT_WHITE);
    tft.drawString("NEW", ctrlX + 5, PANEL_Y + 33, 1);
    
    tft.drawRect(ctrlX + 45, PANEL_Y + 30, 45, 20, TFT_WHITE);
    tft.drawString("INFO", ctrlX + 50, PANEL_Y + 33, 1);
}

// --- TouchManager ---
static bool _lastTouchState = false;
static unsigned long _lastTouchTime = 0;

TouchManager::TouchManager(uint8_t csPin) 
    : ts(nullptr), mySpi(HSPI), initialized(false), x(0), y(0) 
{
    ts = new XPT2046_Touchscreen(csPin, 255);  // Создаём объект динамически
}

bool TouchManager::init() {
    if (!ts) return false;
    
    mySpi.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    if (!ts->begin(mySpi)) {
        Serial.println("Touch controller NOT FOUND!");
        initialized = false;
        return false;
    }
    ts->setRotation(UpDownBoard ? 0 : 2);
    initialized = true;
    Serial.println("Touch controller initialized");
    return true;
}

bool TouchManager::read() {
    if (!initialized || !ts) return false;
    
    bool currentTouchState = ts->touched();
    unsigned long currentTime = millis();

    if (currentTouchState && !_lastTouchState && (currentTime - _lastTouchTime > 100)) {
        TS_Point p = ts->getPoint();
        
        if (p.x < 100 || p.x > 4000 || p.y < 100 || p.y > 4000) {
            _lastTouchState = currentTouchState;
            return false;
        }

        if (UpDownBoard) {
            x = map(p.x, 370, 3740, 0, SCREEN_WIDTH);
            y = map(p.y, 355, 3480, 0, SCREEN_HEIGHT);
        } else {
            // Используем полученные значения из калибровки
            x = map(p.x, 446, 3778, 0, SCREEN_WIDTH);   // minX=446, maxX=3778
            y = map(p.y, 626, 3821, 0, SCREEN_HEIGHT);  // minY=626, maxY=3821
        }

        x = constrain(x, 0, SCREEN_WIDTH - 1);
        y = constrain(y, 0, SCREEN_HEIGHT - 1);

        _lastTouchState = currentTouchState;
        _lastTouchTime = currentTime;
        return true;
    }

    if (!currentTouchState && _lastTouchState) {
        _lastTouchState = false;
        _lastTouchTime = currentTime;
    }
    
    return false;
}

TouchZone TouchManager::getZone() {
    if (y < PANEL_Y) return ZONE_BOARD;
    if (y >= PANEL_Y && y < STATUS_Y) {
        if (x > 145 && x < 175) return ZONE_PREV_DEPTH;
        if (x > 205 && x < 240) return ZONE_NEXT_DEPTH;
        if (y > PANEL_Y + 25) {
            if (x > 150 && x < 190) return ZONE_NEW_GAME;
            if (x > 195 && x < 240) return ZONE_INFO;
        }
    }
    return ZONE_NONE;
}

void TouchManager::calibrate(TFT_eSPI &tft) {
    struct Point { int x; int y; const char* label; };
    Point points[] = {
        {10, 10, "Top-Left"},
        {SCREEN_WIDTH - 10, 10, "Top-Right"},
        {10, SCREEN_HEIGHT - 10, "Bottom-Left"},
        {SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10, "Bottom-Right"},
        {SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "Center"}
    };

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("CALIBRATION", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 40, 2);
    tft.drawCentreString("Touch the crosses", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20, 2);

    for (int i = 0; i < 5; i++) {
        // Рисуем крестик
        tft.drawLine(points[i].x - 5, points[i].y, points[i].x + 5, points[i].y, TFT_RED);
        tft.drawLine(points[i].x, points[i].y - 5, points[i].x, points[i].y + 5, TFT_RED);
        
        Serial.printf("Waiting for touch on %s...\n", points[i].label);

        // Ждем касания
        while (!ts->touched()) delay(10);  // ← ts-> вместо ts.
        
        // Берем серию замеров для точности
        long rX = 0, rY = 0;
        int count = 0;
        for(int j=0; j<20; j++) {
            TS_Point p = ts->getPoint();  // ← ts-> вместо ts.
            if (p.x > 0 && p.x < 4095) {
                rX += p.x; rY += p.y; count++;
            }
            delay(10);
        }
        
        if (count > 0) {
            Serial.printf("%s Raw: X=%ld, Y=%ld\n", points[i].label, rX/count, rY/count);
        }

        // Ждем, пока отпустите палец
        while (ts->touched()) delay(10);  // ← ts-> вместо ts.
        delay(500);
        
        // Стираем крестик
        tft.drawLine(points[i].x - 5, points[i].y, points[i].x + 5, points[i].y, TFT_BLACK);
        tft.drawLine(points[i].x, points[i].y - 5, points[i].x, points[i].y + 5, TFT_BLACK);
    }
    
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("DONE! Check Serial.", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
    delay(2000);
}

// --- ChessBoard ---
ChessBoard::ChessBoard() : selectedX(-1), selectedY(-1), blinkX(-1), blinkY(-1), blinkState(false) {}

void ChessBoard::init() {
    // tft = nullptr;
    fenToBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

char ChessBoard::getPieceChar(char piece) {
    switch (toupper(piece)) {
        case 'K': return 'K';
        case 'Q': return 'Q';
        case 'R': return 'R';
        case 'B': return 'B';
        case 'N': return 'N';
        default:  return '\0';
    }
}

String ChessBoard::getChessNotation(int x, int y) {
    char file = 'a' + x;
    char rank = '8' - y;
    return String(file) + String(rank);
}

void ChessBoard::handleTouch(int tx, int ty, InfoPanel &info, StatusString &status) {
    if (tx < BOARD_START_X || tx >= BOARD_START_X + 8 * CELL_SIZE ||
        ty < BOARD_START_Y || ty >= BOARD_START_Y + 8 * CELL_SIZE) return;

    int cellX = (tx - BOARD_START_X) / CELL_SIZE;
    int cellY = (ty - BOARD_START_Y) / CELL_SIZE;
    
    extern ChessGame chessGame;
    
    if (!chessGame.hasFirstTouch()) {
        // ПЕРВОЕ КАСАНИЕ: проверяем через firstFlow
        chessGame.firstFlow(cellX, cellY, *this, *tft);
    } else {
        // ВТОРОЕ КАСАНИЕ: делаем ход
        int oldX = chessGame.getFirstTouchX();
        int oldY = chessGame.getFirstTouchY();
//        drawCell(oldX, oldY, false);

        if (oldX == cellX && oldY == cellY) {
            // Отмена выбора
            drawCell(oldX, oldY, false);
            selectedX = -1;
            selectedY = -1;
            chessGame.resetFirstTouch();
        } else {
            // Формируем ход
            String moveUCI = chessGame.coordinatesToUCI(oldX, oldY) + 
                            chessGame.coordinatesToUCI(cellX, cellY);
            // Проверка на превращение на 8-ой горизонтале
            char piece = board[oldX][oldY];
            if (toupper(piece) == 'P' && (cellY == 0 || cellY == 7)) {
                moveUCI += "q";
            }
//            Serial.print("MOVE UCI: ");
//            Serial.println(moveUCI);
            // ← СОХРАНЯЕМ ХОД В ChessGame
            chessGame.makeMove(moveUCI);

            // Снимаем подсветку
//            drawCell(oldX, oldY, false);
//            selectedX = -1;
//            selectedY = -1;
//            chessGame.resetFirstTouch();
            
            // Обновляем статус
            status.updateState("YOUR MOVE", TFT_GREEN);
        }
    }
}

void ChessBoard::fenToBoard(String fen) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            board[i][j] = ' ';
            pieceColors[i][j] = 0;
        }
    }
    String position = fen.substring(0, fen.indexOf(' '));
    int x = 0, y = 0;
    for (char c : position) {
        if (c == '/') { y++; x = 0; }
        else if (isdigit(c)) { x += (c - '0'); }
        else {
            board[x][y] = c;
            pieceColors[x][y] = (c >= 'A' && c <= 'Z') ? 1 : 2;
            x++;
        }
    }
}

void ChessBoard::draw() {
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            drawCell(x, y, (selectedX == x && selectedY == y));
        }
    }
}

void ChessBoard::drawCell(int x, int y, bool highlight) {
    if (!tft) return;
    
    int posX = BOARD_START_X + x * CELL_SIZE;
    int posY = BOARD_START_Y + y * CELL_SIZE;
    uint16_t color = ((x + y) % 2 == 0) ? COLOR_WHITE_SQUARE : COLOR_BLACK_SQUARE;
    if (highlight) color = COLOR_HIGHLIGHT;

    tft->fillRect(posX, posY, CELL_SIZE, CELL_SIZE, color);

    if (board[x][y] != ' ') {
        tft->loadFont(ChessCuernavaca28);
        uint16_t pColor = (pieceColors[x][y] == 1) ? TFT_WHITE : TFT_BLACK;
        if (blinkX == x && blinkY == y && blinkState) pColor = TFT_YELLOW;
        
        tft->setTextColor(pColor, color);
        tft->setCursor(posX + 1, posY + 5); 
        tft->print(getPieceSymbol(board[x][y]));
        tft->unloadFont();
    }
}


bool ChessBoard::isPlayerPiece(char piece) {
    if (piece == ' ') return false;
    bool isWhite = isupper(piece);
    extern bool playerIsWhite;  // из config.h
    return (isWhite == playerIsWhite);
}

String ChessBoard::getPieceSymbol(char piece) {
    switch(piece) {
        case 'K': case 'k': return "l";
        case 'Q': case 'q': return "w";
        case 'R': case 'r': return "t";
        case 'B': case 'b': return "v";
        case 'N': case 'n': return "m";
        case 'P': case 'p': return "o";
        default: return " ";
    }
}

// --- DisplayManager ---
DisplayManager::DisplayManager() {}

void DisplayManager::init() {
    tft.init();
    tft.setRotation(UpDownBoard ? 0 : 2); 
    tft.fillScreen(TFT_BLACK);
}

void DisplayManager::clearMenu() {
    tft.fillScreen(TFT_BLACK);
}

void DisplayManager::drawAPScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("VK CHESS", SCREEN_WIDTH / 2, 30);
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("WiFi NOT connected", SCREEN_WIDTH / 2, 80);
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("AP Mode: VK_CHESS", SCREEN_WIDTH / 2, 120);
    tft.drawString("Password: 87654321", SCREEN_WIDTH / 2, 145);
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("http://192.168.4.1", SCREEN_WIDTH / 2, 185);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Open in browser", SCREEN_WIDTH / 2, 220);
    tft.drawString("to configure WiFi", SCREEN_WIDTH / 2, 245);
    
    tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawConnectedScreen(const String& ip) {
    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);
    tft.drawString("CONNECTED!", SCREEN_WIDTH / 2, 100);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("IP: " + ip, SCREEN_WIDTH / 2, 160);
    
    tft.setTextDatum(TL_DATUM);
    delay(2000);
}

void DisplayManager::drawWifiStatus(bool isConnected, const String& ip, bool isAPMode) {
    int statusY = STATUS_Y;
    int statusHeight = STATUS_HEIGHT;
    
    tft.fillRect(0, statusY, SCREEN_WIDTH, statusHeight, TFT_BLACK);
    tft.drawFastHLine(0, statusY, SCREEN_WIDTH, TFT_DARKGREY);
    
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    String line;
    if (isConnected) {
        line = "STA " + ip;
    } else if (isAPMode) {
        line = "AP " + ip;
    } else {
        line = "NO WIFI";
    }
    
    tft.drawString(line, 5, statusY + (statusHeight - 8) / 2);
}

void DisplayManager::drawConnectingScreen(const String& ssid) {
    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("VK CHESS", SCREEN_WIDTH / 2, 30);
    
    tft.setTextSize(1);
    tft.drawString("Connecting to WiFi...", SCREEN_WIDTH / 2, 80);
    tft.drawString("SSID: " + ssid, SCREEN_WIDTH / 2, 110);
    
    dotX = SCREEN_WIDTH / 2 - 30;
    dotY = 145;
    dotStage = 0;
    dotCount = 0;
    
    tft.setTextDatum(TL_DATUM);
    drawConnectingDots();
}

void DisplayManager::drawConnectingDots() {
    tft.fillRect(dotX, dotY, 120, 15, TFT_BLACK);
    
    dotStage++;
    if (dotStage >= 20) dotStage = 0;
    
    if (dotStage < 10) {
        dotCount = dotStage + 1;
    } else {
        dotCount = 19 - dotStage;
    }
    
    tft.setCursor(dotX, dotY);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    for (int i = 0; i < dotCount; i++) {
        tft.print(".");
    }
}

void DisplayManager::drawConnectionFailedScreen() {
    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("CONNECTION FAILED!", SCREEN_WIDTH / 2, 40);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Can't connect to WiFi", SCREEN_WIDTH / 2, 90);
    tft.drawString("Change SSID and Password", SCREEN_WIDTH / 2, 115);
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("AP Mode: VK_CHESS", SCREEN_WIDTH / 2, 155);
    tft.drawString("Password: 87654321", SCREEN_WIDTH / 2, 180);
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("http://192.168.4.1", SCREEN_WIDTH / 2, 220);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Open in browser", SCREEN_WIDTH / 2, 255);
    tft.drawString("to configure WiFi", SCREEN_WIDTH / 2, 280);
    
    tft.setTextDatum(TL_DATUM);
}

void DisplayManager::updateWifiStatus(bool isConnected, const String& ip, bool isAPMode) {
    drawWifiStatus(isConnected, ip, isAPMode);
}

void DisplayManager::startGame(ChessBoard &cb, InfoPanel &ip, StatusString &ss, bool isWhite) {
    playerIsWhite = isWhite; 
    tft.fillScreen(TFT_BLACK);
    
    cb.init();
    cb.draw();
    ip.init(tft);
    ip.setTft(tft);
    ip.drawControls(tft, DEPTH);

    if (playerIsWhite) {
        ss.updateState("YOUR MOVE", TFT_GREEN);
    } else {
        ss.updateState("AI TURN (API)", TFT_RED);
        // TODO: Здесь будет вызов API для хода ИИ
    }
}

void DisplayManager::drawButton(int x, int y, String label, uint16_t kingColor, uint16_t bgColor) {
    // Рисуем кнопку
    tft.fillRoundRect(x, y, 160, 45, 8, bgColor);
    tft.drawRoundRect(x, y, 160, 45, 8, TFT_DARKGREY);
    
    // Если это не кнопка PUZZLES, рисуем короля
    if (label != "PUZZLES") {
        tft.loadFont(ChessCuernavaca28);
        tft.setTextColor(kingColor, bgColor);
        tft.setCursor(x + 12, y + 8);
        tft.print("l");
        tft.unloadFont();
        // Текст со сдвигом
        tft.setTextColor(TFT_BLACK, bgColor);
        tft.setTextSize(1);
        tft.setCursor(x + 60, y + 22);
        tft.print(label);
    } else {
        // Для кнопки PUZZLES — без короля, текст по центру
        tft.setTextColor(TFT_BLACK, bgColor);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(label, x + 80, y + 22, 2);
        tft.setTextDatum(TL_DATUM);
    }
}

void DisplayManager::showMenu() {
    tft.fillScreen(TFT_BLACK);
    
    // Заголовок по центру
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("VK CHESS", SCREEN_WIDTH / 2, 40, 2);
    tft.setTextDatum(TL_DATUM);
    
    // Кнопка PLAY WHITE
    drawButton(40, 100, "PLAY WHITE", TFT_WHITE, COLOR_BTN_NORMAL);
    
    // Кнопка PLAY BLACK
    drawButton(40, 160, "PLAY BLACK", TFT_BLACK, COLOR_BTN_NORMAL);
    
    // Кнопка PUZZLES (новая)
    drawButton(40, 220, "PUZZLES", TFT_CYAN, COLOR_BTN_NORMAL);
    
    // Возвращаем стандартное выравнивание
    tft.setTextDatum(TL_DATUM);
}

void DisplayManager::pressStartButton(int btnIdx) {
    int yPos;
    String label;
    uint16_t kingColor;
    
    if (btnIdx == 0) {
        yPos = 100;
        label = "PLAY WHITE";
        kingColor = TFT_WHITE;
    } else if (btnIdx == 1) {
        yPos = 160;
        label = "PLAY BLACK";
        kingColor = TFT_BLACK;
    } else {
        yPos = 220;
        label = "PUZZLES";
        kingColor = TFT_CYAN;
    }
    
    drawButton(40, yPos, label, kingColor, COLOR_BTN_PRESSED);
}

TFT_eSPI& DisplayManager::getTft() {
    return tft;
}