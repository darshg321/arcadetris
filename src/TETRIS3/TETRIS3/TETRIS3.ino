#include <Adafruit_Protomatter.h>

// -------- Panel size and pinout --------
#define PANEL_W 64
#define PANEL_H 32

uint8_t rgbPins[]  = {2, 10, 3, 4, 11, 5};
uint8_t addrPins[] = {6, 12, 7, 13};
int clockPin = 8;
int latchPin = 14;
int oePin    = 9;

Adafruit_Protomatter matrix(
  PANEL_W, 1, 1, rgbPins, 4, addrPins,
  clockPin, latchPin, oePin, false);

// -------- Game layout --------
#define BOARD_W 10
#define BOARD_H 20
#define OFFSET_X 8
#define OFFSET_Y 8

// -1 = empty, 0–6 = piece type stored in the board
int8_t board[BOARD_H][BOARD_W];

uint32_t score       = 0;
uint16_t linesCleared = 0;

// ---------------- Tetromino data (7 pieces, 4 rotations, 4 blocks) ----------------
const int8_t blockData[7][4][4][2] PROGMEM = {
  // I
  {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}},
   {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
  // J
  {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}},
   {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
  // L
  {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}},
   {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}},
  // O
  {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}},
   {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
  // S
  {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}},
   {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
  // T
  {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}},
   {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
  // Z
  {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}},
   {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}}
};

struct Piece {
  uint8_t type, rot;
  int8_t x, y;
} current, next;

// ---------------- Color helpers ----------------
uint16_t pieceColor(uint8_t type) {
  switch (type) {
    case 0: return matrix.color565(0,   255, 255);  // I – cyan
    case 1: return matrix.color565(0,     0, 255);  // J – blue
    case 2: return matrix.color565(255, 165,   0);  // L – orange
    case 3: return matrix.color565(255, 255,   0);  // O – yellow
    case 4: return matrix.color565(0,   255,   0);  // S – green
    case 5: return matrix.color565(160,   0, 240);  // T – purple
    case 6: return matrix.color565(255,   0,   0);  // Z – red
    default:return matrix.color565(255, 255, 255);
  }
}
inline uint16_t white() { return matrix.color565(255, 255, 255); }

// ---------------- Utility functions ----------------
bool inBounds(int x, int y) {
  return x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H;
}

bool collision(int nx, int ny, int nrot) {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx = pgm_read_byte(&blockData[current.type][nrot][i][0]) + nx;
    int8_t by = pgm_read_byte(&blockData[current.type][nrot][i][1]) + ny;
    if (!inBounds(bx, by) || board[by][bx] != -1) return true;
  }
  return false;
}

void lockPiece() {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx = pgm_read_byte(&blockData[current.type][current.rot][i][0]) + current.x;
    int8_t by = pgm_read_byte(&blockData[current.type][current.rot][i][1]) + current.y;
    if (inBounds(bx, by)) board[by][bx] = current.type;
  }

  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W && full; x++) full &= (board[y][x] != -1);
    if (full) {
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < BOARD_W; x++)
          board[yy][x] = board[yy - 1][x];
      for (int x = 0; x < BOARD_W; x++) board[0][x] = -1;
      score       += 100;
      linesCleared++;
      y++; // Re-check same row after shift
    }
  }
  score += 10;  // Placement bonus
}

void gameOverAnimation() {
  for (uint8_t i = 0; i < 3; i++) {
    matrix.fillScreen(0);
    matrix.setTextColor(white());
    matrix.setCursor(14, 12);
    matrix.print("GAME");
    matrix.setCursor(14, 20);
    matrix.print("OVER");
    matrix.show();
    delay(500);

    matrix.fillScreen(pieceColor(random(7)));
    matrix.show();
    delay(200);
  }
}

void newPiece() {
  current = next;
  current.x = (BOARD_W / 2) - 2;
  current.y = 0;
  if (collision(current.x, current.y, current.rot)) {
    gameOverAnimation();
    memset(board, -1, sizeof(board));
    score = 0;
    linesCleared = 0;
  }
  next.type = random(7);
  next.rot  = 0;
}

void drawPiece(const Piece& p, int ox, int oy) {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t x = pgm_read_byte(&blockData[p.type][p.rot][i][0]) + p.x;
    int8_t y = pgm_read_byte(&blockData[p.type][p.rot][i][1]) + p.y;
    if (inBounds(x, y)) {
      matrix.drawPixel(ox + x, oy + y, pieceColor(p.type));
    }
  }
}

void drawNextPiece() {
  Piece temp = next; temp.x = 0; temp.y = 0;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t x = pgm_read_byte(&blockData[temp.type][temp.rot][i][0]);
    int8_t y = pgm_read_byte(&blockData[temp.type][temp.rot][i][1]);
    matrix.drawPixel(50 + x, 10 + y, pieceColor(temp.type));
  }
}

void drawBorders() {
  // Playfield box
  for (int y = 0; y <= BOARD_H; y++) {
    matrix.drawPixel(OFFSET_X - 1,     OFFSET_Y + y, white());
    matrix.drawPixel(OFFSET_X + BOARD_W, OFFSET_Y + y, white());
  }
  for (int x = 0; x <= BOARD_W; x++) {
    matrix.drawPixel(OFFSET_X + x, OFFSET_Y - 1, white());
    matrix.drawPixel(OFFSET_X + x, OFFSET_Y + BOARD_H, white());
  }
  // Next piece box
  for (int y = 9; y <= 15; y++) {
    matrix.drawPixel(48, y, white());
    matrix.drawPixel(58, y, white());
  }
  for (int x = 48; x <= 58; x++) {
    matrix.drawPixel(x, 9, white());
    matrix.drawPixel(x, 15, white());
  }
}

void drawHUD() {
  matrix.setTextColor(white());
  matrix.setCursor(0, 0);
  matrix.print("S:");
  matrix.print(score);
  matrix.setCursor(0, 6);
  matrix.print("L:");
  matrix.print(linesCleared);
}

void drawBoard() {
  matrix.fillScreen(0);
  for (int y = 0; y < BOARD_H; y++)
    for (int x = 0; x < BOARD_W; x++)
      if (board[y][x] != -1)
        matrix.drawPixel(OFFSET_X + x, OFFSET_Y + y, pieceColor(board[y][x]));

  drawPiece(current, OFFSET_X, OFFSET_Y);
  drawNextPiece();
  drawBorders();
  drawHUD();
  matrix.show();
}

void rotatePiece(int dir) {
  int newRot = (current.rot + dir + 4) % 4;
  if (!collision(current.x, current.y, newRot))
    current.rot = newRot;
}

void hardDrop() {
  while (!collision(current.x, current.y + 1, current.rot)) current.y++;
  lockPiece();
  newPiece();
}

// ---------------- Arduino setup / loop ----------------
const uint32_t DROP_MS = 500;
uint32_t lastDrop      = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  if (matrix.begin() != PROTOMATTER_OK) while (1);
  matrix.setTextSize(1);  // Smallest available font

  memset(board, -1, sizeof(board));

  next.type = random(7);
  next.rot  = 0;
  newPiece();
}

void loop() {
  // ------------ Input handling ------------
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'a' && !collision(current.x - 1, current.y, current.rot)) current.x--;      // Left
    else if (c == 'd' && !collision(current.x + 1, current.y, current.rot)) current.x++; // Right
    else if (c == 's') hardDrop();                                                      // Hard drop
    else if (c == 'r' || c == 'x') rotatePiece(1);                                      // CW
    else if (c == 'z') rotatePiece(-1);                                                 // CCW
  }

  // ------------ Gravity ------------
  if (millis() - lastDrop > DROP_MS) {
    lastDrop = millis();
    if (!collision(current.x, current.y + 1, current.rot)) current.y++;
    else {
      lockPiece();
      newPiece();
    }
  }

  drawBoard();
}