#include <Adafruit_Protomatter.h>

// ───────── Panel pinout ─────────
#define PANEL_W 64
#define PANEL_H 32
uint8_t rgbPins[]  = {2, 10, 3, 4, 11, 5};     // R1 G1 B1 R2 G2 B2
uint8_t addrPins[] = {6, 12, 7, 13};           // A  B  C  D
const int clockPin = 8, latchPin = 14, oePin = 9;
// raccoon is p1s1
const int p1x = 27, p1y = 26, p1s1 = 28, p1s2 = 22;
const int p2x = 15, p2y = 16, p2s1 = 17, p2s2 = 18;

// ───────── Joystick tuning ─────────
const int JOY_CENTER_X  = 3176 >> 5;   // ≈ 199
const int JOY_CENTER_Y  = 3133 >> 5;   // ≈ 196
const int JOY_DEADZONE_X = 12;         // 12 × 16 = 192 raw counts  ≈ 4.7 %
const int JOY_DEADZONE_Y = 18;         // a little wider on Y
const uint32_t JOY_REPEAT_MS  = 150;   // Left/right repeat
const uint32_t DROP_MS        = 500;   // Normal gravity
const uint32_t FAST_DROP_MS   = 50;    // While Y-axis held down

struct GameState;
void rotate(GameState& g, int dir);
void handleInput(int c);

Adafruit_Protomatter matrix(
  PANEL_W, 1, 1, rgbPins, 4, addrPins,
  clockPin, latchPin, oePin, false);

#define BOARD_W 10
#define BOARD_H 20
#define P1_OX   2
#define P2_OX   34
#define OY      8

// ───────── Tetromino shapes (7 pcs × 4 rots × 4 blocks) ─────────
const int8_t blockData[7][4][4][2] PROGMEM = {
  /* I */ {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}},
            {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
  /* J */ {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}},
            {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
  /* L */ {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}},
            {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}},
  /* O */ {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}},
            {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
  /* S */ {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}},
            {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
  /* T */ {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}},
            {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
  /* Z */ {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}},
            {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}}
};

// ─────────--- Types ---─────────
struct Piece { uint8_t type, rot; int8_t x, y; };

struct GameState {
  int8_t  board[BOARD_H][BOARD_W];
  Piece   cur, nxt;
  uint32_t score;
  uint16_t lines;
  uint32_t lastDrop;
  uint8_t  ox;
  bool     gameOver = false;
};

struct JoyState {
  uint8_t xPin, yPin, b1Pin, b2Pin;
  int8_t  prevXDir = 0;
  bool    prevB1   = false, prevB2 = false;
  uint32_t lastRepeat = 0;
};

GameState p1, p2;
JoyState  joys1{p1x, p1y, p1s1, p1s2}, joys2{p2x, p2y, p2s1, p2s2};

// ─────────--- Color helpers ---─────────
uint16_t pieceColor(uint8_t t) {
  switch (t) {
    case 0: return matrix.color565(84,198,157); // I
    case 1: return matrix.color565(187,108,61); // J
    case 2: return matrix.color565(89,72,171); // L
    case 3: return matrix.color565(187,164,60); // O
    case 4: return matrix.color565(141,189,61); // S
    case 5: return matrix.color565(175,74,165); // T
    case 6: return matrix.color565(194,68,75); // Z
    default:return matrix.color565(95,96,100);
  }
}
inline uint16_t white() { return matrix.color565(255,255,255); }

// ─────────--- Logic helpers ---─────────
bool inBounds(int x,int y){ return x>=0&&x<BOARD_W&&y>=0&&y<BOARD_H; }

bool collides(GameState& g,int nx,int ny,int nrot){
  for(uint8_t i=0;i<4;i++){
    int8_t bx=pgm_read_byte(&blockData[g.cur.type][nrot][i][0])+nx;
    int8_t by=pgm_read_byte(&blockData[g.cur.type][nrot][i][1])+ny;
    if(!inBounds(bx,by)||g.board[by][bx]!=-1) return true;
  }
  return false;
}

void lockPiece(GameState& g){
  for(uint8_t i=0;i<4;i++){
    int8_t bx=pgm_read_byte(&blockData[g.cur.type][g.cur.rot][i][0])+g.cur.x;
    int8_t by=pgm_read_byte(&blockData[g.cur.type][g.cur.rot][i][1])+g.cur.y;
    if(inBounds(bx,by)) g.board[by][bx]=g.cur.type;
  }
  for(int y=BOARD_H-1;y>=0;y--){
    bool full=true;
    for(int x=0;x<BOARD_W && full;x++) full&=(g.board[y][x]!=-1);
    if(full){
      for(int yy=y;yy>0;yy--)
        for(int x=0;x<BOARD_W;x++) g.board[yy][x]=g.board[yy-1][x];
      for(int x=0;x<BOARD_W;x++) g.board[0][x]=-1;
      g.score+=100; g.lines++; y++;               // re-check shifted row
    }
  }
  g.score+=10;                                    // placement bonus
}

void gameOver(GameState& g, int winner){
  g.gameOver = true;
  for(uint8_t i=0; i<3; i++){
    matrix.fillScreen(0);
    matrix.setTextColor(white());
    matrix.setCursor(10, 10);
    matrix.print("PLAYER");
    matrix.setCursor(22, 18);
    matrix.print(winner);
    matrix.setCursor(8, 26);
    matrix.print("WINS!");
    matrix.show(); delay(600);

    matrix.fillScreen(pieceColor(random(7)));
    matrix.show(); delay(300);
  }
}

void newPiece(GameState& g){
  g.cur = g.nxt;
  g.cur.x = (BOARD_W/2)-2;
  g.cur.y = 0;
  if(collides(g,g.cur.x,g.cur.y,g.cur.rot)){
    g.gameOver = true;
  }
  g.nxt.type = random(7);
  g.nxt.rot  = 0;
}

void rotate(GameState& g, int dir)
{
  uint8_t newRot = (g.cur.rot + (dir > 0 ? 1 : 3)) & 3;   // 0-3
  static const int8_t kicks[5][2] = {
    { 0, 0}, {-1, 0}, { 1, 0}, { 0,-1}, { 0, 1}
  };
  for (uint8_t k = 0; k < 5; k++) {
    int nx = g.cur.x + kicks[k][0];
    int ny = g.cur.y + kicks[k][1];
    if (!collides(g, nx, ny, newRot)) {        // found a valid kick
      g.cur.x = nx; g.cur.y = ny; g.cur.rot = newRot;
      return;
    }
  }
}

void pollJoystick(JoyState& js, GameState& g, uint32_t now)
{
// ─── Read & scale ───────────────────────────────────────────────
int rawX = analogRead(js.xPin);
int rawY = analogRead(js.yPin);

int xVal = rawX >> 5;        // 0-255   (= rawX / 16)
int yVal = rawY >> 5;        // 0-255

int8_t xDir = 0;
if      (xVal < JOY_CENTER_X - JOY_DEADZONE_X) xDir = -1;
else if (xVal > JOY_CENTER_X + JOY_DEADZONE_X) xDir = +1;

int8_t yDir = 0;
if      (yVal < JOY_CENTER_Y - JOY_DEADZONE_Y) yDir = -1;
else if (yVal > JOY_CENTER_Y + JOY_DEADZONE_Y) yDir = +1;

  if (xDir && (xDir != js.prevXDir || now - js.lastRepeat > JOY_REPEAT_MS)) {
    if (!collides(g, g.cur.x + xDir, g.cur.y, g.cur.rot)) g.cur.x += xDir;
    js.lastRepeat = now;
  }
  js.prevXDir = xDir;

  Serial.print("X="); Serial.print(xVal);
  Serial.print(" Y="); Serial.println(yVal);

  // ---------- Soft-drop ----------
  bool isDown = (yDir == +1);
  uint32_t dropInterval = isDown ? FAST_DROP_MS : DROP_MS;
  if (now - g.lastDrop >= dropInterval) {
    g.lastDrop = now;
    if (!collides(g, g.cur.x, g.cur.y + 1, g.cur.rot)) g.cur.y++;
    else { lockPiece(g); newPiece(g); }
  }

  // ---------- Buttons (rotate) ----------
  bool b1 = !digitalRead(js.b1Pin);  // LOW = pressed (INPUT_PULLUP)
  bool b2 = !digitalRead(js.b2Pin);

  if (b1 && !js.prevB1) rotate(g, +1);   // CW
  if (b2 && !js.prevB2) rotate(g, -1);   // CCW

  js.prevB1 = b1;
  js.prevB2 = b2;
}

void handleInput(int c)
{
  switch (c) {
    // ── Player 1  (a d s r z) ──
    case 'a': if(!collides(p1,p1.cur.x-1,p1.cur.y,p1.cur.rot)) p1.cur.x--; break;
    case 'd': if(!collides(p1,p1.cur.x+1,p1.cur.y,p1.cur.rot)) p1.cur.x++; break;
    case 's': while(!collides(p1,p1.cur.x,p1.cur.y+1,p1.cur.rot)) p1.cur.y++;
              lockPiece(p1); newPiece(p1); break;
    case 'r': rotate(p1, +1); break;
    case 'z': rotate(p1, -1); break;

    // ── Player 2  (j l k x c) ──
    case 'j': if(!collides(p2,p2.cur.x-1,p2.cur.y,p2.cur.rot)) p2.cur.x--; break;
    case 'l': if(!collides(p2,p2.cur.x+1,p2.cur.y,p2.cur.rot)) p2.cur.x++; break;
    case 'k': while(!collides(p2,p2.cur.x,p2.cur.y+1,p2.cur.rot)) p2.cur.y++;
              lockPiece(p2); newPiece(p2); break;
    case 'x': rotate(p2, +1); break;
    case 'c': rotate(p2, -1); break;
  }
}

// ─────────--- Drawing ---─────────
void drawPiece(const GameState& g,const Piece& p){
  for(uint8_t i=0;i<4;i++){
    int8_t x=pgm_read_byte(&blockData[p.type][p.rot][i][0])+p.x;
    int8_t y=pgm_read_byte(&blockData[p.type][p.rot][i][1])+p.y;
    if(inBounds(x,y)) matrix.drawPixel(g.ox+x,OY+y,pieceColor(p.type));
  }
}

void drawNext(const GameState& g){
  Piece t=g.nxt; t.x=0; t.y=0;
  for(uint8_t i=0;i<4;i++){
    int8_t x=pgm_read_byte(&blockData[t.type][t.rot][i][0]);
    int8_t y=pgm_read_byte(&blockData[t.type][t.rot][i][1]);
    matrix.drawPixel(g.ox+BOARD_W+2+x,OY+2+y,pieceColor(t.type));
  }
}

void drawBorders(const GameState& g){
  // playfield rectangle
  for(int y=0;y<=BOARD_H;y++){
    matrix.drawPixel(g.ox-1,      OY+y,white());
    matrix.drawPixel(g.ox+BOARD_W,OY+y,white());
  }
  for(int x=0;x<=BOARD_W;x++){
    matrix.drawPixel(g.ox+x,OY-1,         white());
    matrix.drawPixel(g.ox+x,OY+BOARD_H,   white());
  }
  // next-piece box (6×6 px)
  for(int y=1;y<=5;y++){
    matrix.drawPixel(g.ox+BOARD_W+1,OY+y,white());
    matrix.drawPixel(g.ox+BOARD_W+8,OY+y,white());
  }
  for(int x=g.ox+BOARD_W+1;x<=g.ox+BOARD_W+8;x++){
    matrix.drawPixel(x,OY+1, white());
    matrix.drawPixel(x,OY+6, white());
  }
}

void drawHUD(const GameState& g){
  matrix.setTextColor(white());
  matrix.setCursor(g.ox,0);  matrix.print("S:"); matrix.print(g.score);
  // matrix.setCursor(g.ox,6);  matrix.print("L:"); matrix.print(g.lines);
}

void drawAll(){
  matrix.fillScreen(0);
  drawBorders(p1); drawBorders(p2);
  // boards
  for(int y=0;y<BOARD_H;y++){
    for(int x=0;x<BOARD_W;x++){
      if(p1.board[y][x]!=-1) matrix.drawPixel(p1.ox+x,OY+y,pieceColor(p1.board[y][x]));
      if(p2.board[y][x]!=-1) matrix.drawPixel(p2.ox+x,OY+y,pieceColor(p2.board[y][x]));
    }
  }
  drawPiece(p1,p1.cur); drawPiece(p2,p2.cur);
  drawNext(p1); drawNext(p2);
  drawHUD(p1);  drawHUD(p2);
  matrix.show(); delay(20);
}

// ─────────--- Setup & main loop ---─────────
void initPlayer(GameState& g,uint8_t offsetX){
  memset(g.board,-1,sizeof(g.board));
  g.ox = offsetX;
  g.score=g.lines=0;
  g.lastDrop = millis();
  g.gameOver = false;
  g.nxt.type = random(7); g.nxt.rot = 0;
  newPiece(g);
}

void waitForStart(){
  matrix.fillScreen(0);
  matrix.setTextColor(white());
  matrix.setCursor(6, 10);
  matrix.print("PRESS");
  matrix.setCursor(2, 18);
  matrix.print("ANY BUTTON");
  matrix.setCursor(10, 26);
  matrix.print("TO START");
  matrix.show();

  while(true){
    if(!digitalRead(p1s1) || !digitalRead(p1s2) ||
       !digitalRead(p2s1) || !digitalRead(p2s2)){
      delay(200); // debounce
      break;
    }
  }
}

void setup(){
  Serial.begin(115200); while(!Serial);
  analogReadResolution(12);
  pinMode(p1s1, INPUT_PULLUP); pinMode(p1s2, INPUT_PULLUP);
  pinMode(p2s1, INPUT_PULLUP); pinMode(p2s2, INPUT_PULLUP);
  if(matrix.begin()!=PROTOMATTER_OK) while(1);
  matrix.setTextSize(1);
  waitForStart();
  initPlayer(p1,P1_OX);
  initPlayer(p2,P2_OX);
}

void loop(){
  uint32_t now=millis();
  if(p1.gameOver || p2.gameOver){
    int winner = p1.gameOver ? 2 : 1;
    gameOver(p1, winner);
    gameOver(p2, winner);
    waitForStart();
    initPlayer(p1,P1_OX);
    initPlayer(p2,P2_OX);
    return;
  }

  while(Serial.available()) handleInput(Serial.read());
  pollJoystick(joys1, p1, now);
  pollJoystick(joys2, p2, now);

  if(now-p1.lastDrop>DROP_MS){
    p1.lastDrop=now;
    if(!collides(p1,p1.cur.x,p1.cur.y+1,p1.cur.rot)) p1.cur.y++;
    else { lockPiece(p1); newPiece(p1); }
  }
  if(now-p2.lastDrop>DROP_MS){
    p2.lastDrop=now;
    if(!collides(p2,p2.cur.x,p2.cur.y+1,p2.cur.rot)) p2.cur.y++;
    else { lockPiece(p2); newPiece(p2); }
  }

  drawAll();
}
