#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "space_war_bg.h" // bitmap generated from space_shooter1.png

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------------------------------------------------------
// Sprites (1‑bit)
// ---------------------------------------------------------------
const unsigned char PROGMEM spr_ship[] = {
  0b00000100,0b00000000,
  0b00001110,0b00000000,
  0b00011111,0b00000000,
  0b11111111,0b10000000,
  0b11111111,0b11000000,
  0b11111111,0b11000000,
  0b11111111,0b11000000,
  0b01111111,0b11000000,
  0b00111111,0b10000000,
  0b00000001,0b00000000
};

const unsigned char PROGMEM spr_ship_thr[] = {
  0b00000100,0b00000000,
  0b00001110,0b00000000,
  0b00011111,0b00000000,
  0b11111111,0b10000000,
  0b11111111,0b11000000,
  0b11111111,0b11000000,
  0b11111111,0b11000000,
  0b01111111,0b11000000,
  0b00111111,0b10000000,
  0b00000111,0b11000000
};

const unsigned char PROGMEM spr_missile[] = {
  0b11110000,
  0b11111100,
  0b11111100,
  0b11110000
};

const unsigned char PROGMEM spr_expl0[] = {0x18,0x18,0x18,0x7E,0x7E,0x18,0x18,0x18};
const unsigned char PROGMEM spr_expl1[] = {0x24,0x5A,0x3C,0xFF,0xFF,0x3C,0x5A,0x24};
const unsigned char PROGMEM spr_expl2[] = {0x42,0xBD,0x7E,0xFF,0xFF,0x7E,0xBD,0x42};
const unsigned char PROGMEM spr_expl3[] = {0x81,0x66,0x3C,0xDB,0xDB,0x3C,0x66,0x81};
const unsigned char * const explFrames[] = {spr_expl0,spr_expl1,spr_expl2,spr_expl3};

#define MAX_BULLETS 6
#define MAX_ENEMIES 4
#define MAX_EXPLOSIONS 4
#define MAX_PARTICLES 12

struct Bullet { float x,y,vx,vy; bool active; };
struct Enemy { float x,y,vx,vy; int hp; bool active; };
struct Explosion { int16_t x,y; uint8_t frame,timer; bool active; };
struct Particle { float x,y,vx,vy; int8_t life; };

Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
Explosion explosions[MAX_EXPLOSIONS];
Particle particles[MAX_PARTICLES];

float shipX=10.0f, shipY=36.0f, shipVy=0.0f;
bool shipThr=true;
int hull=100, shield=60, score=0;
uint16_t frameCnt=0;

uint16_t rng=0xACE1; uint16_t rnd(){ rng^=rng<<7; rng^=rng>>9; rng^=rng<<8; return rng; }

void spawnBullet(float x,float y,float vx,float vy){ for(int i=0;i<MAX_BULLETS;i++) if(!bullets[i].active){ bullets[i]={x,y,vx,vy,true}; break; } }
void spawnEnemy(float x,float y,float vx,float vy){ for(int i=0;i<MAX_ENEMIES;i++) if(!enemies[i].active){ enemies[i]={x,y,vx,vy,2,true}; break; } }
void spawnExplosion(int16_t x,int16_t y){ for(int i=0;i<MAX_EXPLOSIONS;i++) if(!explosions[i].active){ explosions[i]={x,y,0,0,true}; break; } }
void spawnParticle(float x,float y,float vx,float vy,int8_t life){ for(int i=0;i<MAX_PARTICLES;i++) if(particles[i].life<=0){ particles[i]={x,y,vx,vy,life}; break; } }

void drawHUD(){
  display.drawLine(0,15,127,15,SSD1306_WHITE);
  display.setCursor(0,1); display.print(F("HULL"));
  int w = map(constrain(hull,0,100),0,100,0,30);
  display.drawRect(30,1,32,6,SSD1306_WHITE);
  display.fillRect(31,2,w,4,SSD1306_WHITE);
  display.setCursor(70,1); display.print(F("SC:")); display.print(score);
}

void updateShip(){
  float targetY = 32.0f + sin(frameCnt*0.04f)*12.0f;
  shipVy = (targetY-shipY)*0.07f; shipY+=shipVy;
  if(shipY<18) shipY=18; if(shipY>52) shipY=52;
  shipThr = ((frameCnt/4)%2)==0;
  if(frameCnt%12==0){ spawnBullet(shipX+10, shipY+4, 4.0f, 0.0f); spawnParticle(shipX-2, shipY+5, -1.2f, 0.0f, 6); }
}
void updateBullets(){
  for(int i=0;i<MAX_BULLETS;i++) if(bullets[i].active){
    bullets[i].x+=bullets[i].vx; bullets[i].y+=bullets[i].vy;
    if(bullets[i].x>127 || bullets[i].y<16 || bullets[i].y>63){ bullets[i].active=false; continue; }
    for(int e=0;e<MAX_ENEMIES;e++) if(enemies[e].active){
      if(bullets[i].x>enemies[e].x-4 && bullets[i].x<enemies[e].x+6 &&
         bullets[i].y>enemies[e].y-4 && bullets[i].y<enemies[e].y+4){
        bullets[i].active=false; enemies[e].hp--; spawnExplosion((int)enemies[e].x,(int)enemies[e].y);
        if(enemies[e].hp<=0){ enemies[e].active=false; score+=20; }
        break; }
    }
  }
}
void updateEnemies(){
  if(frameCnt%60==0){ float ey = 20 + (rnd()%30); spawnEnemy(130, ey, -1.6f, 0.0f); }
  for(int i=0;i<MAX_ENEMIES;i++) if(enemies[i].active){
    enemies[i].x+=enemies[i].vx; enemies[i].y+=enemies[i].vy;
    if(enemies[i].x<-10) enemies[i].active=false;
  }
}
void updateExplosions(){
  for(int i=0;i<MAX_EXPLOSIONS;i++) if(explosions[i].active){
    if(++explosions[i].timer>=3){ explosions[i].timer=0; if(++explosions[i].frame>=4) explosions[i].active=false; }
  }
}
void updateParticles(){
  for(int i=0;i<MAX_PARTICLES;i++) if(particles[i].life>0){ particles[i].x+=particles[i].vx; particles[i].y+=particles[i].vy; particles[i].life--; }
}

void drawBackground(){ display.drawBitmap(0,0,bg_bitmap,128,64,SSD1306_WHITE); }
void drawShip(){ if(shipThr) display.drawBitmap((int)shipX,(int)shipY,spr_ship,10,10,SSD1306_WHITE);
               else display.drawBitmap((int)shipX,(int)shipY,spr_ship_thr,10,10,SSD1306_WHITE);
}
void drawBullets(){ for(int i=0;i<MAX_BULLETS;i++) if(bullets[i].active){ int x=(int)bullets[i].x, y=(int)bullets[i].y; display.drawLine(x,y-1,x+6,y-1,SSD1306_WHITE); display.drawLine(x,y,x+6,y,SSD1306_WHITE); display.drawLine(x,y+1,x+6,y+1,SSD1306_WHITE); } }
void drawEnemies(){ for(int i=0;i<MAX_ENEMIES;i++) if(enemies[i].active) display.drawBitmap((int)enemies[i].x,(int)enemies[i].y,spr_missile,6,4,SSD1306_WHITE); }
void drawExplosions(){ for(int i=0;i<MAX_EXPLOSIONS;i++) if(explosions[i].active) display.drawBitmap(explosions[i].x,explosions[i].y,explFrames[explosions[i].frame],8,8,SSD1306_WHITE); }
void drawParticles(){ for(int i=0;i<MAX_PARTICLES;i++) if(particles[i].life>0) display.drawPixel((int)particles[i].x,(int)particles[i].y,SSD1306_WHITE); }

void setup(){ Serial.begin(115200); if(!display.begin(SSD1306_SWITCHCAPVCC,SCREEN_ADDRESS)){ Serial.println(F("Fallo I2C")); while(1); } display.clearDisplay(); }

void loop(){
  updateShip(); updateBullets(); updateEnemies(); updateExplosions(); updateParticles();
  display.clearDisplay(); drawBackground(); drawShip(); drawBullets(); drawEnemies(); drawExplosions(); drawParticles(); drawHUD(); display.display();
  delay(33);
  frameCnt++;
}
