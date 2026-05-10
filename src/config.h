#ifndef CONFIG_H
#define CONFIG_H

// ==================== ПИНЫ ДЛЯ ДИСПЛЕЯ (ILI9341) ====================
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   21

// ==================== ПИНЫ ДЛЯ ТАЧСКРИНА (XPT2046) ====================
#define TOUCH_SCLK 25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
#define TOUCH_CS   33
#define TOUCH_IRQ  -1

// ==================== ПАРАМЕТРЫ ЭКРАНА ====================
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define CELL_SIZE     30
#define BOARD_START_X 0
#define BOARD_START_Y 0

// ==================== ЦВЕТА ====================
#define COLOR_WHITE_SQUARE 0xDEDB 
#define COLOR_BLACK_SQUARE 0x4208
#define COLOR_HIGHLIGHT    0xFFE0
#define COLOR_BTN_NORMAL   0x7BEF
#define COLOR_BTN_PRESSED  0x3186
#define COLOR_PANEL        0x2104

// ==================== ПОЛОЖЕНИЕ ПАНЕЛЕЙ ====================
#define PANEL_Y      240
#define INFO_HEIGHT  55
#define STATUS_HEIGHT 20
#define STATUS_Y     (PANEL_Y + INFO_HEIGHT)

// ==================== НАСТРОЙКИ ПО УМОЛЧАНИЮ ====================
#define DEFAULT_SSID     "WiFI"
#define DEFAULT_PASSWORD "87654321"
#define DEFAULT_EMAIL    "your_email@example.com"
#define API_LICHESS      "API-lichess"
#define AI_LEVEL         1 // Это значение по умолчанию уровня игры

// ==================== ВЫБОР РЕЖИМА ИГРЫ ====================
#define USE_ONLINE_API      1   // 1 - Lichess, 0 - локальный движок
// ============================================================

// ==================== ТАЙМАУТЫ ====================
#define WIFI_CONNECT_TIMEOUT 10000

// ==================== ПОВОРОТ ЭКРАНА ====================
// true - стандарт, false - вверх ногами
inline bool UpDownBoard = false;

// ==================== ЦВЕТ ИГРОКА ПО УМОЛЧАНИЮ ====================
inline bool playerIsWhite = true;

// ==================== ГЛУБИНА ПОИСКА AI ====================
extern int DEPTH;
#define INITIAL_DEPTH 2

// ==================== ЗОНЫ КАСАНИЯ ====================
enum TouchZone {
    ZONE_NONE,
    ZONE_BOARD,
    ZONE_PREV_DEPTH,
    ZONE_NEXT_DEPTH,
    ZONE_NEW_GAME,
    ZONE_INFO
};

#endif // CONFIG_H
