struct Pose {
  int8_t hx,hy;
  int8_t sx,sy;
  int8_t bx,by;
  int8_t lax,lay;
  int8_t lhx,lhy;
  int8_t rax,ray;
  int8_t rhx,rhy;
  int8_t lkx,lky;
  int8_t lfx,lfy;
  int8_t rkx,rky;
  int8_t rfx,rfy;
};

struct Scene {
  const Pose* f1a;
  const Pose* f1b;
  const Pose* f2a;
  const Pose* f2b;
  uint8_t  dur;
  int8_t   hitFrame;
  int8_t   hitX, hitY;
  int8_t   hitParts;
  int8_t   hp1dmg;
  int8_t   hp2dmg;
};

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCR_W 128
#define SCR_H 64
#define GND   54      
#define OLED_RESET -1
#define OLED_ADDR  0x3C

Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

uint16_t _rng = 0xABCD;
uint16_t rnd16() { _rng^=_rng<<7; _rng^=_rng>>9; _rng^=_rng<<8; return _rng; }

const Pose PROGMEM K1_IDLE = { 28,31, 28,36, 28,44, 22,40, 17,45, 34,40, 39,45, 24,50, 21,GND, 32,50, 35,GND };
const Pose PROGMEM K1_STANCE = { 27,33, 27,38, 29,45, 21,41, 16,44, 34,40, 38,44, 24,50, 20,GND, 33,50, 36,GND };
const Pose PROGMEM K1_PUNCH_R = { 28,31, 28,36, 28,44, 22,39, 18,43, 34,36, 58,36, 25,50, 22,GND, 33,50, 36,GND };
const Pose PROGMEM K1_PUNCH_L = { 28,31, 28,36, 28,44, 22,36, 55,36, 34,39, 38,43, 25,50, 22,GND, 33,50, 36,GND };
const Pose PROGMEM K1_KICK_HIGH = { 28,27, 28,32, 28,40, 22,33, 18,28, 34,33, 39,28, 23,47, 20,GND, 42,34, 62,28 };
const Pose PROGMEM K1_KICK_LOW = { 28,29, 28,34, 29,42, 22,37, 17,41, 34,37, 39,41, 24,48, 20,GND, 38,50, 55,GND };
const Pose PROGMEM K1_JUMP = { 28,17, 28,23, 28,31, 22,26, 17,21, 34,26, 39,21, 24,37, 20,43, 32,37, 36,43 };
const Pose PROGMEM K1_FLIP_MID = { 28,18, 31,23, 35,29, 36,22, 41,17, 38,28, 44,24, 32,34, 28,39, 36,32, 42,28 };
const Pose PROGMEM K1_FLY_KICK = { 48,22, 46,28, 44,35, 40,30, 36,25, 50,30, 56,25, 40,40, 36,47, 54,28, 70,26 };
const Pose PROGMEM K1_KNOCKED = { 14,25, 17,30, 22,37, 12,27, 6,22, 26,30, 32,25, 17,43, 11,49, 25,41, 31,47 };
const Pose PROGMEM K1_FALLEN = { 8,52, 17,52, 28,53, 12,49, 7,46, 23,50, 29,48, 30,GND, 23,GND, 38,54, 44,GND };
const Pose PROGMEM K1_GETUP = { 20,43, 22,48, 28,52, 15,46, 10,42, 24,49, 30,47, 25,GND, 20,GND, 34,GND, 40,GND };
const Pose PROGMEM K1_VICTORY = { 28,26, 28,31, 28,39, 22,30, 17,23, 34,30, 40,23, 24,46, 21,GND, 32,46, 35,GND };
const Pose PROGMEM K1_BLOCK = { 26,32, 27,37, 28,44, 22,36, 19,31, 31,36, 34,31, 24,50, 21,GND, 32,50, 35,GND };
const Pose PROGMEM K1_CROUCH = { 26,38, 26,43, 28,49, 20,43, 16,47, 33,43, 38,47, 23,GND, 18,GND, 33,GND, 37,GND };
const Pose PROGMEM K1_SPIN_KICK = { 35,24, 33,29, 30,36, 24,28, 19,23, 38,26, 43,21, 25,41, 21,48, 40,29, 58,22 };

const Pose PROGMEM K2_IDLE = { 100,31, 100,36, 100,44, 94,40, 89,45, 106,40, 111,45, 96,50, 93,GND, 104,50, 107,GND };
const Pose PROGMEM K2_STANCE = { 101,33, 101,38, 99,45, 95,40, 91,44, 108,41, 113,44, 96,50, 92,GND, 105,50, 108,GND };
const Pose PROGMEM K2_PUNCH_L = { 100,31, 100,36, 100,44, 94,36, 70,36, 106,39, 111,43, 96,50, 93,GND, 104,50, 107,GND };
const Pose PROGMEM K2_PUNCH_R = { 100,31, 100,36, 100,44, 94,39, 89,43, 106,36, 73,36, 96,50, 93,GND, 104,50, 107,GND };
const Pose PROGMEM K2_KICK_HIGH = { 100,27, 100,32, 100,40, 94,33, 89,28, 106,33, 111,28, 101,47, 104,GND, 82,34, 62,28 };
const Pose PROGMEM K2_KICK_SPIN = { 98,25, 101,31, 103,38, 96,29, 92,23, 108,31, 113,26, 103,44, 107,GND, 88,30, 70,22 };
const Pose PROGMEM K2_JUMP = { 100,17, 100,23, 100,31, 94,26, 89,21, 106,26, 111,21, 96,37, 93,43, 104,37, 107,43 };
const Pose PROGMEM K2_FLY_KICK = { 80,22, 82,28, 84,35, 90,30, 95,25, 78,30, 72,25, 88,40, 92,47, 74,28, 58,26 };
const Pose PROGMEM K2_KNOCKED = { 113,25, 110,30, 105,37, 115,27, 121,22, 101,30, 95,25, 110,43, 116,49, 102,41, 96,47 };
const Pose PROGMEM K2_FALLEN = { 120,52, 111,52, 100,53, 116,49, 121,46, 105,50, 99,48, 98,GND, 104,GND, 90,54, 84,GND };
const Pose PROGMEM K2_GETUP = { 108,43, 106,48, 100,52, 113,46, 118,42, 104,49, 98,47, 103,GND, 108,GND, 95,GND, 89,GND };
const Pose PROGMEM K2_UPPERCUT = { 100,27, 100,32, 100,40, 94,36, 89,30, 106,30, 104,16, 103,46, 107,GND, 97,46, 94,GND };
const Pose PROGMEM K2_BLOCK = { 102,32, 101,37, 100,44, 95,36, 92,31, 107,36, 110,31, 96,50, 93,GND, 104,50, 107,GND };

// Pre-declare lerpPose to bypass Arduino builder quirk (sometimes this works)
void _lerpPose(const Pose* PA, const Pose* PB, int t, int tmax, Pose* out) {
  Pose a,b;
  memcpy_P(&a, PA, sizeof(Pose));
  memcpy_P(&b, PB, sizeof(Pose));
  if(tmax <= 0) tmax = 1;
  out->hx = a.hx + ((b.hx-a.hx)*t)/tmax; out->hy = a.hy + ((b.hy-a.hy)*t)/tmax;
  out->sx = a.sx + ((b.sx-a.sx)*t)/tmax; out->sy = a.sy + ((b.sy-a.sy)*t)/tmax;
  out->bx = a.bx + ((b.bx-a.bx)*t)/tmax; out->by = a.by + ((b.by-a.by)*t)/tmax;
  out->lax= a.lax+ ((b.lax-a.lax)*t)/tmax; out->lay= a.lay+ ((b.lay-a.lay)*t)/tmax;
  out->lhx= a.lhx+ ((b.lhx-a.lhx)*t)/tmax; out->lhy= a.lhy+ ((b.lhy-a.lhy)*t)/tmax;
  out->rax= a.rax+ ((b.rax-a.rax)*t)/tmax; out->ray= a.ray+ ((b.ray-a.ray)*t)/tmax;
  out->rhx= a.rhx+ ((b.rhx-a.rhx)*t)/tmax; out->rhy= a.rhy+ ((b.rhy-a.rhy)*t)/tmax;
  out->lkx= a.lkx+ ((b.lkx-a.lkx)*t)/tmax; out->lky= a.lky+ ((b.lky-a.lky)*t)/tmax;
  out->lfx= a.lfx+ ((b.lfx-a.lfx)*t)/tmax; out->lfy= a.lfy+ ((b.lfy-a.lfy)*t)/tmax;
  out->rkx= a.rkx+ ((b.rkx-a.rkx)*t)/tmax; out->rky= a.rky+ ((b.rky-a.rky)*t)/tmax;
  out->rfx= a.rfx+ ((b.rfx-a.rfx)*t)/tmax; out->rfy= a.rfy+ ((b.rfy-a.rfy)*t)/tmax;
}

int8_t shX=0, shY=0;
uint8_t shTimer=0;

void triggerShake(uint8_t strength) { shTimer=strength; }

void drawFigure(const Pose* fptr) {
  int ox=shX, oy=shY;
  display.drawCircle    (fptr->hx+ox, fptr->hy+oy, 3, SSD1306_WHITE);
  display.drawLine(fptr->hx+ox, fptr->hy+3+oy, fptr->sx+ox, fptr->sy+oy, SSD1306_WHITE);
  display.drawLine(fptr->sx+ox, fptr->sy+oy, fptr->bx+ox, fptr->by+oy, SSD1306_WHITE);
  display.drawLine(fptr->sx+ox, fptr->sy+oy, fptr->lax+ox, fptr->lay+oy, SSD1306_WHITE);
  display.drawLine(fptr->lax+ox,fptr->lay+oy, fptr->lhx+ox, fptr->lhy+oy, SSD1306_WHITE);
  display.drawLine(fptr->sx+ox, fptr->sy+oy, fptr->rax+ox, fptr->ray+oy, SSD1306_WHITE);
  display.drawLine(fptr->rax+ox,fptr->ray+oy, fptr->rhx+ox, fptr->rhy+oy, SSD1306_WHITE);
  display.drawLine(fptr->bx+ox, fptr->by+oy, fptr->lkx+ox, fptr->lky+oy, SSD1306_WHITE);
  display.drawLine(fptr->lkx+ox,fptr->lky+oy, fptr->lfx+ox, fptr->lfy+oy, SSD1306_WHITE);
  display.drawLine(fptr->bx+ox, fptr->by+oy, fptr->rkx+ox, fptr->rky+oy, SSD1306_WHITE);
  display.drawLine(fptr->rkx+ox,fptr->rky+oy, fptr->rfx+ox, fptr->rfy+oy, SSD1306_WHITE);
}

#define MAX_PARTS 32
struct Particle { float x,y,vx,vy; int8_t life; bool big; };
Particle parts[MAX_PARTS];

bool flashOn = false;
uint8_t flashTimer = 0;

struct Burst { int8_t x,y; uint8_t timer; bool active; };
#define MAX_BURST 4
Burst bursts[MAX_BURST];

void spawnBurst(int x, int y) {
  for(int i=0;i<MAX_BURST;i++) if(!bursts[i].active) {
    bursts[i]={x,y,8,true}; break;
  }
  triggerShake(10);
  flashOn=true; flashTimer=3;
}

void spawnParticles(float x, float y, int count, float speed) {
  for(int c=0;c<count;c++) {
    for(int i=0;i<MAX_PARTS;i++) if(parts[i].life<=0) {
      float angle = (rnd16()%628)*0.01f;
      float spd   = speed * (0.5f + (float)(rnd16()%100)/100.0f);
      parts[i] = {x,y, cosf(angle)*spd, sinf(angle)*spd - 0.5f,
                  (int8_t)(8+rnd16()%12), (bool)(rnd16()%3==0)};
      break;
    }
  }
}

void updateEffects() {
  if(shTimer > 0) {
    int amp = shTimer/2+1;
    shX = (shTimer%2==0) ? amp : -amp;
    shY = (shTimer%3==0) ? amp/2 : -amp/2;
    shTimer--;
  } else { shX=0; shY=0; }
  if(flashTimer > 0) { flashTimer--; if(!flashTimer) flashOn=false; }
  for(int i=0;i<MAX_BURST;i++) if(bursts[i].active) {
    if(--bursts[i].timer == 0) bursts[i].active=false;
  }
  for(int i=0;i<MAX_PARTS;i++) if(parts[i].life>0) {
    parts[i].x += parts[i].vx; parts[i].y += parts[i].vy; parts[i].vy += 0.25f;
    if(parts[i].y >= GND) { parts[i].vy *= -0.45f; parts[i].y = GND; }
    parts[i].life--;
  }
}

void drawEffects() {
  for(int i=0;i<MAX_BURST;i++) if(bursts[i].active) {
    int r = 11 - bursts[i].timer;
    int bx=bursts[i].x+shX, by=bursts[i].y+shY;
    for(int d=0;d<8;d++) {
      float a = d * 0.785f;
      int x1=bx + (int)(cosf(a)*3), y1=by + (int)(sinf(a)*3);
      int x2=bx + (int)(cosf(a)*r), y2=by + (int)(sinf(a)*r);
      display.drawLine(x1,y1,x2,y2,SSD1306_WHITE);
    }
    display.drawCircle(bx,by,r/2,SSD1306_WHITE);
  }
  for(int i=0;i<MAX_PARTS;i++) if(parts[i].life>0) {
    int px=(int)parts[i].x+shX, py=(int)parts[i].y+shY;
    if(parts[i].big) display.fillRect(px,py,2,2,SSD1306_WHITE);
    else display.drawPixel(px,py,SSD1306_WHITE);
  }
}

int hp1=100, hp2=100;
void drawHUD() {
  display.fillRect(0,0,40,10,SSD1306_BLACK); display.drawRect(0,0,40,10,SSD1306_WHITE);
  int w1=map(constrain(hp1,0,100),0,100,1,38); display.fillRect(1,1,w1,8,SSD1306_WHITE);
  display.setTextSize(1); display.setTextColor(SSD1306_BLACK); display.setCursor(4,2); display.print(F("P1"));
  display.setTextColor(SSD1306_WHITE); display.setCursor(56,2); display.print(F("VS"));
  display.fillRect(88,0,40,10,SSD1306_BLACK); display.drawRect(88,0,40,10,SSD1306_WHITE);
  int w2=map(constrain(hp2,0,100),0,100,1,38); display.fillRect(89,1,w2,8,SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK); display.setCursor(108,2); display.print(F("P2"));
  display.setTextColor(SSD1306_WHITE);
}

const Scene PROGMEM CHOREOGRAPHY[] = {
  {&K1_IDLE,&K1_STANCE,    &K2_IDLE,&K2_STANCE,     35, -1, 0,0, 0, 0, 0},
  {&K1_STANCE,&K1_PUNCH_L, &K2_STANCE,&K2_BLOCK,    14, -1, 0,0, 0, 0, 0},
  {&K1_PUNCH_L,&K1_STANCE, &K2_BLOCK,&K2_STANCE,    10,  3, 70,36, 6, 0, 5},
  {&K1_STANCE,&K1_KNOCKED, &K2_STANCE,&K2_PUNCH_L,  12,  6, 38,34, 14, 18, 0},
  {&K1_KNOCKED,&K1_FALLEN, &K2_PUNCH_L,&K2_STANCE,  18, -1, 0,0, 0, 0, 0},
  {&K1_FALLEN,&K1_GETUP,   &K2_STANCE,&K2_STANCE,   22, -1, 0,0, 0, 0, 0},
  {&K1_GETUP,&K1_STANCE,   &K2_STANCE,&K2_UPPERCUT, 16, -1, 0,0, 0, 0, 0},
  {&K1_STANCE,&K1_JUMP,    &K2_UPPERCUT,&K2_STANCE, 12,  4, 34,28, 16, 20, 0},
  {&K1_JUMP,&K1_FLIP_MID,  &K2_STANCE,&K2_STANCE,   14, -1, 0,0, 0, 0, 0},
  {&K1_FLIP_MID,&K1_FLY_KICK, &K2_STANCE,&K2_KNOCKED, 16, 10, 80,30, 20, 0, 25},
  {&K1_FLY_KICK,&K1_STANCE,  &K2_KNOCKED,&K2_FALLEN, 18, -1, 0,0, 0, 0, 0},
  {&K1_STANCE,&K1_STANCE,  &K2_FALLEN,&K2_GETUP,    22, -1, 0,0, 0, 0, 0},
  {&K1_STANCE,&K1_BLOCK,   &K2_GETUP,&K2_KICK_SPIN, 14, -1, 0,0, 0, 0, 0},
  {&K1_BLOCK,&K1_KNOCKED,  &K2_KICK_SPIN,&K2_STANCE, 12, 4, 32,34, 14, 15, 0},
  {&K1_KNOCKED,&K1_CROUCH, &K2_STANCE,&K2_KICK_HIGH,16, -1, 0,0, 0, 0, 0},
  {&K1_CROUCH,&K1_KICK_LOW,&K2_KICK_HIGH,&K2_FALLEN, 14, 7, 95,50, 18, 0, 22},
  {&K1_KICK_LOW,&K1_STANCE, &K2_FALLEN,&K2_GETUP,   20, -1, 0,0, 0, 0, 0},
  {&K1_STANCE,&K1_SPIN_KICK, &K2_GETUP,&K2_FLY_KICK, 16, 7, 64,32, 28, 10, 30},
  {&K1_SPIN_KICK,&K1_KNOCKED, &K2_FLY_KICK,&K2_KNOCKED, 18, -1, 0,0, 0, 0, 0},
  {&K1_KNOCKED,&K1_VICTORY, &K2_KNOCKED,&K2_FALLEN,  50, -1, 0,0, 0, 0, 0},
};
#define NUM_SCENES (sizeof(CHOREOGRAPHY)/sizeof(CHOREOGRAPHY[0]))

enum State { ST_TITLE, ST_ROUND_FLASH, ST_FIGHT, ST_KO };
State state = ST_TITLE;
uint8_t  stTimer = 0;
uint8_t  curScene = 0;
uint8_t  sceneFrame = 0;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
}

void drawTitle() {
  stTimer++; display.clearDisplay();
  for(int i=0;i<12;i++) { int sx=rnd16()%128, sy=rnd16()%64; if(rnd16()%3==0) display.drawPixel(sx,sy,SSD1306_WHITE); }
  if(stTimer < 8) { display.setTextSize(1); display.setCursor(34,20); display.print(F("STICK FIGHT")); }
  else { display.setTextSize(2); display.setCursor(6,10); display.print(F("STICK")); display.setCursor(16,30); display.print(F("FIGHT!")); }
  display.drawLine(0,8,127,8,SSD1306_WHITE); display.drawLine(0,56,127,56,SSD1306_WHITE);
  display.drawLine(0,8,0,56,SSD1306_WHITE); display.drawLine(127,8,127,56,SSD1306_WHITE);
  if((stTimer/12)%2==0) { display.setTextSize(1); display.setCursor(14,52); display.print(F("COMENZANDO...")); }
  display.display(); delay(33);
  if(stTimer >= 80) { state=ST_ROUND_FLASH; stTimer=0; hp1=100; hp2=100; curScene=0; sceneFrame=0; for(int i=0;i<MAX_PARTS;i++) parts[i].life=0; for(int i=0;i<MAX_BURST;i++) bursts[i].active=false; shTimer=0; shX=0; shY=0; }
}

void drawRoundFlash() {
  stTimer++; display.clearDisplay();
  display.drawLine(0, GND, 127, GND, SSD1306_WHITE);
  Pose p1, p2;
  memcpy_P(&p1, &K1_IDLE, sizeof(Pose)); memcpy_P(&p2, &K2_IDLE, sizeof(Pose));
  drawFigure(&p1); drawFigure(&p2);
  display.setTextSize(2);
  if(stTimer < 20) { display.setCursor(24,20); display.print(F("ROUND 1")); }
  else if(stTimer < 35) { if(stTimer%3!=0) { display.setCursor(28,20); display.print(F("FIGHT!")); } }
  display.display(); delay(33);
  if(stTimer >= 45) { state=ST_FIGHT; stTimer=0; }
}

void doFight() {
  Scene sc; memcpy_P(&sc, &CHOREOGRAPHY[curScene], sizeof(Scene));
  if(sc.hitFrame >= 0 && sceneFrame == (uint8_t)sc.hitFrame) {
    spawnBurst(sc.hitX, sc.hitY); spawnParticles(sc.hitX, sc.hitY, sc.hitParts, 3.5f);
    hp1 = constrain(hp1 - sc.hp1dmg, 0, 100); hp2 = constrain(hp2 - sc.hp2dmg, 0, 100);
  }
  updateEffects(); display.clearDisplay();
  if(flashOn && flashTimer > 0) { display.fillRect(0,0,SCR_W,SCR_H,SSD1306_WHITE); }
  display.drawLine(0+shX, GND+shY, SCR_W-1+shX, GND+shY, SSD1306_WHITE);
  display.drawLine(12+shX, GND+1+shY, 42+shX, GND+1+shY, SSD1306_WHITE);
  display.drawLine(86+shX, GND+1+shY,116+shX, GND+1+shY, SSD1306_WHITE);
  Pose f1, f2;
  _lerpPose(sc.f1a, sc.f1b, sceneFrame, sc.dur, &f1);
  _lerpPose(sc.f2a, sc.f2b, sceneFrame, sc.dur, &f2);
  drawFigure(&f1); drawFigure(&f2);
  drawEffects(); drawHUD(); display.display(); delay(30);
  sceneFrame++;
  if(sceneFrame >= sc.dur) { sceneFrame=0; curScene++; if(curScene >= NUM_SCENES) { state=ST_KO; stTimer=0; } }
}

void drawKO() {
  stTimer++; updateEffects(); display.clearDisplay();
  display.drawLine(0, GND, 127, GND, SSD1306_WHITE);
  Pose pv, pf; memcpy_P(&pv, &K1_VICTORY, sizeof(Pose)); memcpy_P(&pf, &K2_FALLEN, sizeof(Pose));
  drawFigure(&pv); drawFigure(&pf);
  if(stTimer%5==0) spawnParticles(pv.hx, pv.hy-4, 3, 2.5f);
  drawEffects(); drawHUD();
  if(stTimer < 60) {
    display.setTextSize(2); display.setCursor(32, 20);
    if((stTimer/4)%2==0) display.print(F("K.O.!")); else { display.setTextSize(1); display.setCursor(44,22); display.print(F("K.O.!")); }
  } else { display.setTextSize(1); display.setCursor(16,18); display.print(F("P1 WINS!")); display.setCursor(8,30); display.print(F("FLAWLESS VICTORY")); }
  display.display(); delay(33);
  if(stTimer >= 120) { state=ST_TITLE; stTimer=0; }
}

void loop() {
  switch(state) { case ST_TITLE: drawTitle(); break; case ST_ROUND_FLASH: drawRoundFlash(); break; case ST_FIGHT: doFight(); break; case ST_KO: drawKO(); break; default: state=ST_TITLE; break; }
}
