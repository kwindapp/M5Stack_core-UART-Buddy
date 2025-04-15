#include <M5Stack.h>

// === SETTINGS ===
#define TEXT_SIZE        1
#define REFRESH_INTERVAL 100
#define THEME_ID         3   // Choose theme: 1–5
#define BUFFER_SECONDS   5   // Buffer data for the last N seconds (can change this value)

// === THEMES ===
uint16_t COLOR_BG, COLOR_TEXT, COLOR_HEADER, COLOR_BORDER;

void applyTheme(int id) {
  switch (id) {
    case 1:  // Matrix
      COLOR_BG = BLACK;
      COLOR_TEXT = GREEN;
      COLOR_HEADER = YELLOW;
      COLOR_BORDER = DARKGREY;
      break;
    case 2:  // Cyberpunk
      COLOR_BG = BLACK;
      COLOR_TEXT = MAGENTA;
      COLOR_HEADER = CYAN;
      COLOR_BORDER = DARKGREY;
      break;
    case 3:  // Retro Amber
      COLOR_BG = BLACK;
      COLOR_TEXT = ORANGE;
      COLOR_HEADER = RED;
      COLOR_BORDER = DARKGREY;
      break;
    case 4:  // Arctic
      COLOR_BG = WHITE;
      COLOR_TEXT = NAVY;
      COLOR_HEADER = BLUE;
      COLOR_BORDER = DARKGREY;
      break;
    case 5:  // Classic Grey
      COLOR_BG = DARKGREY;
      COLOR_TEXT = WHITE;
      COLOR_HEADER = LIGHTGREY;
      COLOR_BORDER = BLACK;
      break;
    default:
      applyTheme(1);
      break;
  }
}

// === DISPLAY SIZING ===
#define CHAR_PER_LINE    (TEXT_SIZE == 1 ? 80 : (TEXT_SIZE == 2 ? 42 : 28))
#define LINE_HEIGHT      (TEXT_SIZE * 10)
#define MAX_LINES        (240 / LINE_HEIGHT - 1)

// === UART CONFIG ===
int baudRates[] = {9600, 19200, 38400, 57600, 115200};
int baudIndex = 4;

// === HISTORY CONFIG ===
#define HISTORY_LINES 100
String history[HISTORY_LINES];
unsigned long timestamps[HISTORY_LINES];
int historyLineCount = 0;
int scrollIndex = 0;
bool scrollMode = false;

// === STATE ===
String bufferLine = "";
bool displayEnabled = true;
unsigned long lastRefresh = 0;

void startUART() {
  Serial2.begin(baudRates[baudIndex], SERIAL_8N1, 16, 17);
  Serial.printf("UART started at %d baud\n", baudRates[baudIndex]);
}

void drawScreen() {
  M5.Lcd.fillScreen(COLOR_BG);
  M5.Lcd.setTextColor(COLOR_TEXT, COLOR_BG);
  M5.Lcd.setTextSize(TEXT_SIZE);

  unsigned long now = millis();
  int shown = 0;

  if (scrollMode) {
    // Scrollback mode: Show data from the last BUFFER_SECONDS seconds
    for (int i = historyLineCount - 1; i >= 0 && shown < MAX_LINES; i--) {
      if (now - timestamps[i] <= BUFFER_SECONDS * 1000) {  // Convert seconds to milliseconds
        if (i >= scrollIndex) {
          M5.Lcd.setCursor(0, (MAX_LINES - 1 - shown) * LINE_HEIGHT);
          M5.Lcd.print(history[i]);
          shown++;
        }
      }
    }
  } else {
    // Live mode: Display current data stream
    int start = (historyLineCount > MAX_LINES ? historyLineCount - MAX_LINES : 0);
    for (int i = start; i < historyLineCount && shown < MAX_LINES; i++) {
      M5.Lcd.setCursor(0, (MAX_LINES - 1 - shown) * LINE_HEIGHT);
      M5.Lcd.print(history[i]);
      shown++;
    }
  }

  M5.Lcd.setTextColor(COLOR_HEADER, COLOR_BG);
  M5.Lcd.setCursor(0, MAX_LINES * LINE_HEIGHT);
  if (scrollMode) {
    M5.Lcd.print(" BtnC: Exit");
  } else {
    M5.Lcd.printf("B:%d  Scroll   %s", baudRates[baudIndex], displayEnabled ? "ON" : "OFF");
  }

  //M5.Lcd.drawLine(0, MAX_LINES * LINE_HEIGHT - 2, 320, MAX_LINES * LINE_HEIGHT - 2, COLOR_BORDER);
}

void setup() {
  M5.begin();
  M5.Lcd.setTextSize(TEXT_SIZE);
  applyTheme(THEME_ID);
  M5.Lcd.fillScreen(COLOR_BG);

  for (int i = 0; i < HISTORY_LINES; i++) history[i] = "";

  // Show "UART Buddy Ready"
  M5.Lcd.setTextColor(RED);
  M5.Lcd.setCursor(80, 100);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("KWIND UART");
  delay(5000);
  M5.Lcd.fillScreen(COLOR_BG);

  startUART();
  drawScreen();
  Serial.println("UART SNIFFER READY");
}

void loop() {
  M5.update();

  // A: Change baud rate
  if (M5.BtnA.wasPressed()) {
    baudIndex = (baudIndex + 1) % (sizeof(baudRates) / sizeof(baudRates[0]));
    startUART();
    drawScreen();
  }

  // B: Toggle Scrollback Mode (show last BUFFER_SECONDS seconds of data)
  if (M5.BtnB.wasPressed()) {
    scrollMode = !scrollMode;
    if (!scrollMode) {
      // Reset scroll index when exiting scroll mode
      scrollIndex = 0;
    }
    drawScreen();
  }

  // C: Toggle display
  if (M5.BtnC.wasPressed()) {
    displayEnabled = !displayEnabled;
    drawScreen();
  }

  // Scroll through history in scrollback mode
  if (scrollMode) {
    if (M5.BtnA.wasPressed() && scrollIndex > 0) {
      scrollIndex--;
      drawScreen();
    }
    if (M5.BtnB.wasPressed() && scrollIndex < historyLineCount - MAX_LINES) {
      scrollIndex++;
      drawScreen();
    }
  }

  // UART read
  while (Serial2.available()) {
    char ch = Serial2.read();
    Serial.write(ch);

    if (ch == '\r') continue;
    if (ch == '\n') {
      if (historyLineCount < HISTORY_LINES) {
        history[historyLineCount] = bufferLine;
        timestamps[historyLineCount] = millis();
        historyLineCount++;
      } else {
        // Shift lines
        for (int i = 0; i < HISTORY_LINES - 1; i++) {
          history[i] = history[i + 1];
          timestamps[i] = timestamps[i + 1];
        }
        history[HISTORY_LINES - 1] = bufferLine;
        timestamps[HISTORY_LINES - 1] = millis();
      }
      bufferLine = "";
    } else {
      if (bufferLine.length() < CHAR_PER_LINE) {
        bufferLine += ch;
      }
    }
  }

  // Display refresh
  if (displayEnabled && (millis() - lastRefresh > REFRESH_INTERVAL)) {
    drawScreen();
    lastRefresh = millis();
  }

  delay(2);
}
