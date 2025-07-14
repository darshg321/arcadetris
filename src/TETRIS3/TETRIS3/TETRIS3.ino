#include <Adafruit_Protomatter.h>

// ───────── Panel pinout ─────────
#define PANEL_W 64
#define PANEL_H 32
uint8_t rgbPins[]  = {2, 10, 3, 4, 11, 5};     // R1 G1 B1 R2 G2 B2
uint8_t addrPins[] = {6, 12, 7, 13};           // A  B  C  D
const int clockPin = 8, latchPin = 14, oePin = 9;

Adafruit_Protomatter matrix(
  PANEL_W, 1, 1, rgbPins, 4, addrPins,
  clockPin, latchPin, oePin, false);

// ───────── Game parameters ─────────
#define BOARD_W 10
#define BOARD_H 20
#define P1_OX   2    // x-origin of player 1 board
#define P2_OX   34   // x-origin of player 2 board
#define OY      8    // y-origin shared by both boards
const uint32_t DROP_MS = 500;   // gravity interval

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
  uint8_t  ox;               // screen offset X for this board
};

// ─────────--- Globals ---─────────
GameState p1, p2;            // two independent players

// ─────────--- Color helpers ---─────────
uint16_t pieceColor(uint8_t t) {
  switch (t) {
    case 0: return matrix.color565(0  ,255,255); // I
    case 1: return matrix.color565(0  ,0  ,255); // J
    case 2: return matrix.color565(255,165,0  ); // L
    case 3: return matrix.color565(255,255,0  ); // O
    case 4: return matrix.color565(0  ,255,0  ); // S
    case 5: return matrix.color565(160,0  ,240); // T
    case 6: return matrix.color565(255,0  ,0  ); // Z
    default:return matrix.color565(255,255,255);
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

void gameOver(GameState& g){
  for(uint8_t i=0;i<3;i++){
    matrix.fillScreen(0);
    matrix.setTextColor(white());
    matrix.setCursor(g.ox+1,12);
    matrix.print("GAME");
    matrix.setCursor(g.ox+1,20);
    matrix.print("OVER");
    matrix.show(); delay(500);
    matrix.fillScreen(pieceColor(random(7))); matrix.show(); delay(200);
  }
}

void newPiece(GameState& g){
  g.cur = g.nxt;
  g.cur.x = (BOARD_W/2)-2;
  g.cur.y = 0;
  if(collides(g,g.cur.x,g.cur.y,g.cur.rot)){      // top-out
    gameOver(g);
    memset(g.board,-1,sizeof(g.board));
    g.score=g.lines=0;
  }
  g.nxt.type = random(7);
  g.nxt.rot  = 0;
}

void rotate(GameState& g,int dir){
  int nr=(g.cur.rot+dir+4)%4;
  if(!collides(g,g.cur.x,g.cur.y,nr)) g.cur.rot=nr;
}

void hardDrop(GameState& g){ while(!collides(g,g.cur.x,g.cur.y+1,g.cur.rot)) g.cur.y++;
  lockPiece(g); newPiece(g); }

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
  g.nxt.type = random(7); g.nxt.rot = 0;
  newPiece(g);
}

void setup(){
  Serial.begin(115200); while(!Serial);
  if(matrix.begin()!=PROTOMATTER_OK) while(1);
  matrix.setTextSize(1);               // smallest font
  initPlayer(p1,P1_OX); initPlayer(p2,P2_OX);
}

void handleInput(char c){
  // Player 1
  if     (c=='a' && !collides(p1,p1.cur.x-1,p1.cur.y,p1.cur.rot)) p1.cur.x--;
  else if(c=='d' && !collides(p1,p1.cur.x+1,p1.cur.y,p1.cur.rot)) p1.cur.x++;
  else if(c=='s') hardDrop(p1);
  else if(c=='r'||c=='x') rotate(p1,1);
  else if(c=='z') rotate(p1,-1);
  // Player 2
  else if(c=='j' && !collides(p2,p2.cur.x-1,p2.cur.y,p2.cur.rot)) p2.cur.x--;
  else if(c=='l' && !collides(p2,p2.cur.x+1,p2.cur.y,p2.cur.rot)) p2.cur.x++;
  else if(c=='k') hardDrop(p2);
  else if(c=='n') rotate(p2,1);
  else if(c=='m') rotate(p2,-1);
}

void loop(){
  // ── serial input ──
  while(Serial.available()) handleInput(Serial.read());

  // ── gravity for each player ──
  uint32_t now=millis();
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
