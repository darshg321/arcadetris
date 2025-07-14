#include <Adafruit_Protomatter.h>

// ---------- Panel geometry ----------
#define PANEL_W   64
#define PANEL_H   32

// ---------- Play-field geometry ----------
#define BOARD_W   10          // columns
#define BOARD_H   20          // rows
#define PX_SIZE   1           // 1 matrix pixel per Tetris cell
#define SCORE_H   8           // pixel height reserved for score text

// Compute pixel offsets so board is centred horizontally.
const int boardOffsetX = (PANEL_W - BOARD_W * PX_SIZE) / 2;
const int boardOffsetY = SCORE_H;

// ---------- HUB75 pinout (same as your sketch) ----------
uint8_t rgbPins[]  = {2, 10, 3, 4, 11, 5}; // R1 G1 B1 R2 G2 B2
uint8_t addrPins[] = {6, 12, 7, 13};       // A  B  C  D
int clockPin = 8;
int latchPin = 14;
int oePin    = 9;

Adafruit_Protomatter matrix(
  PANEL_W,
  1,                // bit-depth (one plane per colour)
  1,                // number of panels
  rgbPins,
  4, addrPins,
  clockPin,
  latchPin,
  oePin,
  false);

// ---------- Game state ----------
bool board[BOARD_H][BOARD_W] = {false};
uint32_t score = 0;

// 7 tetrominoes, each with 4 rotations, each rotation = 4 blocks (x,y)
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
  uint8_t type;     // 0-6
  uint8_t rot;      // 0-3
  int8_t  x, y;     // top-left of 4×4 bounding box in board coords
} current;

// ---------- Helpers ----------
uint16_t red() { return matrix.color565(255,0,0); }

bool inBounds(int x, int y) {
  return (x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H);
}

bool collision(int nx, int ny, int nrot) {
  for (uint8_t b = 0; b < 4; b++) {
    int8_t bx = pgm_read_byte(&blockData[current.type][nrot][b][0]) + nx;
    int8_t by = pgm_read_byte(&blockData[current.type][nrot][b][1]) + ny;
    if (!inBounds(bx, by) || board[by][bx]) return true;
  }
  return false;
}

void lockPiece() {
  for (uint8_t b = 0; b < 4; b++) {
    int8_t bx = pgm_read_byte(&blockData[current.type][current.rot][b][0]) + current.x;
    int8_t by = pgm_read_byte(&blockData[current.type][current.rot][b][1]) + current.y;
    if (inBounds(bx, by)) board[by][bx] = true;
  }

  // Check for full lines
  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W && full; x++) full &= board[y][x];
    if (full) {
      // shift everything above down one row
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < BOARD_W; x++)
          board[yy][x] = board[yy - 1][x];
      for (int x = 0; x < BOARD_W; x++) board[0][x] = false;
      score += 100;
      y++;                     // re-check same row
    }
  }
  score += 10;                // 10 pts per piece
}

void newPiece() {
  current.type = random(7);
  current.rot  = 0;
  current.x    = (BOARD_W / 2) - 2;
  current.y    = 0;
  if (collision(current.x, current.y, current.rot)) {
    // Game over – clear board & score
    memset(board, 0, sizeof(board));
    score = 0;
  }
}

void drawBoard() {
  matrix.fillScreen(0);                  // clear

  // Draw landed blocks
  for (int y = 0; y < BOARD_H; y++)
    for (int x = 0; x < BOARD_W; x++)
      if (board[y][x])
        matrix.drawPixel(boardOffsetX + x * PX_SIZE,
                         boardOffsetY + y * PX_SIZE,
                         red());

  // Draw active piece
  for (uint8_t b = 0; b < 4; b++) {
    int8_t bx = pgm_read_byte(&blockData[current.type][current.rot][b][0]) + current.x;
    int8_t by = pgm_read_byte(&blockData[current.type][current.rot][b][1]) + current.y;
    if (inBounds(bx, by))
      matrix.drawPixel(boardOffsetX + bx * PX_SIZE,
                       boardOffsetY + by * PX_SIZE,
                       red());
  }

  // Draw scoreboard
  matrix.setCursor(0, 0);
  matrix.setTextColor(red());
  matrix.print(F("SCORE "));
  matrix.print(score);

  matrix.show();
}

// ---------- Arduino boilerplate ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) ;                  // wait for USB-CDC

  ProtomatterStatus st = matrix.begin();
  if (st != PROTOMATTER_OK) {
    Serial.print(F("Matrix error: ")); Serial.println((int)st);
    while (1);
  }
  matrix.setTextSize(1);
  newPiece();
}

// ---------- Main loop ----------
const uint32_t DROP_MS = 500;        // auto-drop interval
uint32_t lastDrop = 0;

void loop() {
  // -------- Serial controls --------
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'l' && !collision(current.x - 1, current.y, current.rot))
      current.x--;
    else if (c == 'r' && !collision(current.x + 1, current.y, current.rot))
      current.x++;
  }

  // -------- Automatic drop --------
  uint32_t now = millis();
  if (now - lastDrop >= DROP_MS) {
    lastDrop = now;
    if (!collision(current.x, current.y + 1, current.rot))
      current.y++;
    else {
      lockPiece();
      newPiece();
    }
  }

  // -------- Render --------
  drawBoard();
}
