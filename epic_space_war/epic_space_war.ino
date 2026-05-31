// ============================================================
//  EPIC SPACE WAR
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Shooter espacial arcade con controles capacitivos.
//  Basado en los assets de "Space War v2".
//  
//  Controles capacitivos por hardware:
//    GPIO 4  (Touch 0): Mover ARRIBA
//    GPIO 15 (Touch 3): Mover ABAJO
//    (El disparo es automático y constante)
//
//  Características:
//    - Parallax de 4 capas continuas.
//    - Enemigos que disparan misiles.
//    - Batalla contra jefe (Boss Fight) con barra de vida.
//    - Sistema de partículas y explosiones detalladas.
//    - Auto-calibración adaptativa táctil.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// ── Calibración Táctil Capacitiva ──────────────────
static int t4Base  = -1;
static int t15Base = -1;

// ============================================================
//  SPRITES
// ============================================================
const unsigned char PROGMEM spr_ship[] = {
    0b00000000, 0b00000100, 0b00000000, 
    0b00000000, 0b00001110, 0b00000000, 
    0b00000000, 0b00011111, 0b00000000, 
    0b00011111, 0b11111111, 0b10000000, 
    0b01111111, 0b11111111, 0b11000000, 
    0b11111111, 0b11111111, 0b11000000, 
    0b11111111, 0b11111111, 0b11000000, 
    0b01111111, 0b11111111, 0b11000000, 
    0b00011111, 0b10111111, 0b10000000, 
    0b00000001, 0b00000001, 0b00000000,
};

const unsigned char PROGMEM spr_ship_thrust[] = {
    0b00000000, 0b00000100, 0b00000000, 
    0b00000000, 0b00001110, 0b00000000, 
    0b00000000, 0b00011111, 0b00000000, 
    0b00011111, 0b11111111, 0b10000000, 
    0b01111111, 0b11111111, 0b11000000, 
    0b11111111, 0b11111111, 0b11000000, 
    0b11111111, 0b11111111, 0b11000000, 
    0b01111111, 0b11111111, 0b11000000, 
    0b00011111, 0b10111111, 0b10000000, 
    0b00000011, 0b01000011, 0b00000000, 
};

// Boss ship 24x16 
const unsigned char PROGMEM spr_boss[] = {
    0b00000001, 0b11111100, 0b00000000, 
    0b00000011, 0b11111110, 0b00000000,
    0b00000111, 0b11111111, 0b00000000, 
    0b00001111, 0b11111111, 0b10000000,
    0b01111111, 0b11111111, 0b11100000, 
    0b11111111, 0b11111111, 0b11111000,
    0b11111111, 0b11111111, 0b11111111, 
    0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 
    0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111000, 
    0b01111111, 0b11111111, 0b11100000,
    0b00001111, 0b11111111, 0b10000000, 
    0b00000111, 0b11111111, 0b00000000,
    0b00000011, 0b11111110, 0b00000000, 
    0b00000001, 0b11111100, 0b00000000,
};

// Enemy ship 12x8
const unsigned char PROGMEM spr_enemy[] = {
    0b00001111, 0b10000000,
    0b01111111, 0b11000000,
    0b11111111, 0b11100000,
    0b11111111, 0b11110000,
    0b11111111, 0b11110000,
    0b11111111, 0b11100000,
    0b01111111, 0b11000000,
    0b00001111, 0b10000000,
};

const unsigned char PROGMEM spr_exp0[] = {0x18, 0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x18};
const unsigned char PROGMEM spr_exp1[] = {0x24, 0x5A, 0x3C, 0xFF, 0xFF, 0x3C, 0x5A, 0x24};
const unsigned char PROGMEM spr_exp2[] = {0x42, 0xBD, 0x7E, 0xFF, 0xFF, 0x7E, 0xBD, 0x42};
const unsigned char PROGMEM spr_exp3[] = {0x81, 0x66, 0x3C, 0xDB, 0xDB, 0x3C, 0x66, 0x81};
const unsigned char PROGMEM spr_exp4[] = {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};
const unsigned char PROGMEM spr_exp5[] = {0x42, 0x24, 0x18, 0x00, 0x00, 0x18, 0x24, 0x42};
const unsigned char *const expFrames[] = {spr_exp0, spr_exp1, spr_exp2, spr_exp3, spr_exp4, spr_exp5};

const unsigned char PROGMEM spr_missile[] = {
    0b11110000,
    0b11111100,
    0b11111100,
    0b11110000,
};

// ============================================================
//  VARIABLES GLOBALES
// ============================================================
uint16_t fCnt = 0;
int score = 0;
bool gameOver = false;
bool bossActive = false;

// Jugador
float shipY = 32.0f;
float shipVy = 0.0f;
int hp = 10;
const int MAX_HP = 10;
int invulTimer = 0;

// Jefe
float bossX = 140.0f;
float bossY = 32.0f;
float bossVy = 0.0f;
int bossHp = 50;
const int BOSS_MAX_HP = 50;
int bossState = 0; // 0=Intro, 1=Pelea

// ============================================================
//  SISTEMAS
// ============================================================
#define MAX_BULLETS 8
struct Bullet { float x, y, vx; bool active; bool isEnemy; };
Bullet bullets[MAX_BULLETS];

#define MAX_ENEMIES 3
struct Enemy { float x, y, vx, vy; int hp; int fireTimer; bool active; };
Enemy enemies[MAX_ENEMIES];

#define MAX_EXPS 6
struct Explosion { int16_t x, y; int8_t frame, timer; bool active; };
Explosion exps[MAX_EXPS];

// Parallax
#define N_FAR 18
#define N_MID 10
#define N_NEAR 6
#define N_NEBULA 3
struct Star { int16_t x, y; };
Star sFar[N_FAR], sMid[N_MID], sNear[N_NEAR];
struct Nebula { int16_t x, y; uint8_t r; float spd; };
Nebula nebulas[N_NEBULA];

// ============================================================
//  FUNCIONES
// ============================================================

uint16_t rng = 0xACE1;
uint16_t rnd() {
  rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8;
  return rng;
}

void initStars() {
  for (int i=0; i<N_FAR; i++) { sFar[i].x = rnd()%128; sFar[i].y = 10 + rnd()%54; }
  for (int i=0; i<N_MID; i++) { sMid[i].x = rnd()%128; sMid[i].y = 10 + rnd()%54; }
  for (int i=0; i<N_NEAR; i++) { sNear[i].x = rnd()%128; sNear[i].y = 10 + rnd()%54; }
  for (int i=0; i<N_NEBULA; i++) { 
    nebulas[i].x = rnd()%128; nebulas[i].y = 10 + rnd()%44; 
    nebulas[i].r = 3 + rnd()%5; nebulas[i].spd = 0.3f + i*0.1f; 
  }
}

void updateStars(float speedMul) {
  for (int i=0; i<N_FAR; i++) {
    if ((rng + i) % 3 == 0) sFar[i].x -= speedMul > 0 ? 1 : 0;
    if (sFar[i].x < 0) { sFar[i].x = 127; sFar[i].y = 10 + rnd()%54; }
  }
  for (int i=0; i<N_MID; i++) {
    sMid[i].x -= 1 * speedMul;
    if (sMid[i].x < 0) { sMid[i].x = 127; sMid[i].y = 10 + rnd()%54; }
  }
  for (int i=0; i<N_NEAR; i++) {
    sNear[i].x -= 3 * speedMul;
    if (sNear[i].x < 0) { sNear[i].x = 127; sNear[i].y = 10 + rnd()%54; }
  }
  for (int i=0; i<N_NEBULA; i++) {
    nebulas[i].x -= nebulas[i].spd * speedMul;
    if (nebulas[i].x < -10) {
      nebulas[i].x = 135; nebulas[i].y = 10 + rnd()%44;
      nebulas[i].r = 3 + rnd()%5;
    }
  }
}

void drawStars() {
  for (int i=0; i<N_NEBULA; i++) {
    display.drawCircle(nebulas[i].x, nebulas[i].y, nebulas[i].r, SSD1306_WHITE);
    display.drawPixel(nebulas[i].x, nebulas[i].y, SSD1306_WHITE);
  }
  for (int i=0; i<N_FAR; i++) display.drawPixel(sFar[i].x, sFar[i].y, SSD1306_WHITE);
  for (int i=0; i<N_MID; i++) {
    display.drawPixel(sMid[i].x, sMid[i].y, SSD1306_WHITE);
    if (i % 3 == 0) display.drawPixel(sMid[i].x + 1, sMid[i].y, SSD1306_WHITE);
  }
  for (int i=0; i<N_NEAR; i++) display.drawLine(sNear[i].x, sNear[i].y, sNear[i].x + 4, sNear[i].y, SSD1306_WHITE);
}

void spawnExp(int16_t x, int16_t y) {
  for (int i=0; i<MAX_EXPS; i++) {
    if (!exps[i].active) {
      exps[i] = {x, y, 0, 0, true};
      break;
    }
  }
}

void spawnBullet(float x, float y, float vx, bool isEnemy) {
  for (int i=0; i<MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i] = {x, y, vx, true, isEnemy};
      break;
    }
  }
}

void spawnEnemy() {
  for (int i=0; i<MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].active = true;
      enemies[i].x = 135.0f;
      enemies[i].y = 15.0f + rnd()%40;
      enemies[i].vx = -1.5f - (rnd()%10)*0.1f;
      enemies[i].vy = (rnd()%3 == 0) ? ((rnd()%2==0)? 0.5f : -0.5f) : 0.0f;
      enemies[i].hp = 3;
      enemies[i].fireTimer = 20 + rnd()%30;
      break;
    }
  }
}

void resetGame() {
  score = 0;
  hp = MAX_HP;
  shipY = 32.0f;
  shipVy = 0.0f;
  gameOver = false;
  bossActive = false;
  bossHp = BOSS_MAX_HP;
  
  for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = false;
  for(int i=0; i<MAX_ENEMIES; i++) enemies[i].active = false;
  for(int i=0; i<MAX_EXPS; i++) exps[i].active = false;
  
  initStars();
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) while(1);
  resetGame();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  display.clearDisplay();

  int t4  = touchRead(4);
  int t15 = touchRead(15);
  if (t4Base  == -1 && t4  > 10) t4Base  = t4;
  if (t15Base == -1 && t15 > 10) t15Base = t15;
  if (t4Base  < 15) t4Base  = 60;
  if (t15Base < 15) t15Base = 60;
  
  bool btnUp   = (t4  < t4Base  * 0.65f);
  bool btnDown = (t15 < t15Base * 0.65f);

  if (gameOver) {
    updateStars(0.2f);
    drawStars();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(35, 20); display.print(F("GAME OVER"));
    display.setCursor(15, 40); display.print(F("Touch to restart"));
    display.display();
    if (btnUp || btnDown) { resetGame(); delay(300); }
    return;
  }

  // 1. Lógica del Jugador
  if (btnUp) shipVy -= 0.6f;
  if (btnDown) shipVy += 0.6f;
  shipVy *= 0.85f; // Fricción
  shipY += shipVy;
  
  if (shipY < 10) { shipY = 10; shipVy = 0; }
  if (shipY > 54) { shipY = 54; shipVy = 0; }
  
  if (invulTimer > 0) invulTimer--;

  // Auto-disparo
  if (fCnt % 6 == 0) {
    spawnBullet(18.0f, shipY + 5.0f, 6.0f, false);
  }

  // 2. Lógica de Balas
  for (int i=0; i<MAX_BULLETS; i++) {
    if (bullets[i].active) {
      bullets[i].x += bullets[i].vx;
      if (bullets[i].x < -10 || bullets[i].x > 138) {
        bullets[i].active = false;
      }
    }
  }

  // 3. Lógica Enemigos (Naves Pequeñas)
  if (!bossActive) {
    if (fCnt % 40 == 0 && (score < 500 || rnd()%3 != 0)) spawnEnemy();
    
    // Activar al jefe al llegar a 300 puntos
    if (score >= 300) {
      bossActive = true;
      bossX = 140.0f;
      bossY = 32.0f;
      bossState = 0;
      bossHp = BOSS_MAX_HP;
    }
  }

  for (int i=0; i<MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      enemies[i].x += enemies[i].vx;
      enemies[i].y += enemies[i].vy;
      if (enemies[i].y < 10 || enemies[i].y > 56) enemies[i].vy = -enemies[i].vy;
      
      if (enemies[i].fireTimer > 0) enemies[i].fireTimer--;
      else {
        spawnBullet(enemies[i].x - 2, enemies[i].y + 4, -4.0f, true);
        enemies[i].fireTimer = 30 + rnd()%40;
      }
      
      if (enemies[i].x < -15) enemies[i].active = false;
    }
  }

  // 4. Lógica Jefe
  if (bossActive) {
    if (bossState == 0) { // Intro
      bossX -= 0.5f;
      if (bossX <= 100.0f) { bossX = 100.0f; bossState = 1; }
    } else { // Pelea
      // Patrón de movimiento simple (seguir un poco al jugador)
      if (bossY + 8 < shipY) bossVy += 0.1f;
      else if (bossY + 8 > shipY + 4) bossVy -= 0.1f;
      bossVy *= 0.9f;
      bossY += bossVy;
      if (bossY < 10) bossY = 10;
      if (bossY > 48) bossY = 48;
      
      // Patrones de disparo
      if (fCnt % 25 == 0) {
        spawnBullet(bossX, bossY + 4, -5.0f, true);
        spawnBullet(bossX, bossY + 12, -5.0f, true);
      }
      if (fCnt % 60 == 0) {
        spawnBullet(bossX, bossY + 8, -6.0f, true);
      }
    }
  }

  // 5. Colisiones
  for (int i=0; i<MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    
    if (bullets[i].isEnemy) {
      // Choque contra jugador
      if (invulTimer == 0 && bullets[i].x < 18 && bullets[i].x > 2 && bullets[i].y > shipY && bullets[i].y < shipY + 10) {
        bullets[i].active = false;
        hp--;
        invulTimer = 20; // Frames de invulnerabilidad
        spawnExp(10, shipY);
        if (hp <= 0) gameOver = true;
      }
    } else {
      // Choque contra enemigos
      bool hit = false;
      for (int e=0; e<MAX_ENEMIES; e++) {
        if (enemies[e].active && bullets[i].x > enemies[e].x && bullets[i].x < enemies[e].x + 12 && bullets[i].y > enemies[e].y && bullets[i].y < enemies[e].y + 8) {
          hit = true;
          enemies[e].hp--;
          if (enemies[e].hp <= 0) {
            enemies[e].active = false;
            spawnExp(enemies[e].x, enemies[e].y);
            score += 20;
          }
          break;
        }
      }
      // Choque contra el jefe
      if (!hit && bossActive && bossState == 1) {
        if (bullets[i].x > bossX && bullets[i].y > bossY && bullets[i].y < bossY + 16) {
          hit = true;
          bossHp--;
          if (bossHp <= 0) {
            bossActive = false;
            for(int k=0; k<5; k++) spawnExp(bossX + (rnd()%20), bossY + (rnd()%16));
            score += 500;
          }
        }
      }
      
      if (hit) bullets[i].active = false;
    }
  }

  // Choque de cuerpo enemigo-jugador
  if (invulTimer == 0) {
    for (int e=0; e<MAX_ENEMIES; e++) {
      if (enemies[e].active && enemies[e].x < 18 && enemies[e].x > 2 && enemies[e].y + 8 > shipY && enemies[e].y < shipY + 10) {
        hp--;
        invulTimer = 20;
        enemies[e].active = false;
        spawnExp(10, shipY);
        spawnExp(enemies[e].x, enemies[e].y);
        if (hp <= 0) gameOver = true;
      }
    }
  }

  // 6. Actualizar Animaciones
  for (int i=0; i<MAX_EXPS; i++) {
    if (exps[i].active) {
      if (++exps[i].timer >= 2) {
        exps[i].timer = 0;
        if (++exps[i].frame >= 6) exps[i].active = false;
      }
    }
  }

  // ==========================================
  //  RENDER
  // ==========================================
  
  float speedMul = bossActive ? 1.5f : 1.0f;
  updateStars(speedMul);
  drawStars();

  // Dibujar Balas
  for (int i=0; i<MAX_BULLETS; i++) {
    if (bullets[i].active) {
      if (bullets[i].isEnemy) display.drawBitmap(bullets[i].x, bullets[i].y - 2, spr_missile, 6, 4, SSD1306_WHITE);
      else display.drawFastHLine(bullets[i].x, bullets[i].y, 6, SSD1306_WHITE);
    }
  }

  // Dibujar Enemigos
  for (int e=0; e<MAX_ENEMIES; e++) {
    if (enemies[e].active) {
      display.drawBitmap(enemies[e].x, enemies[e].y, spr_enemy, 12, 8, SSD1306_WHITE);
    }
  }

  // Dibujar Jefe
  if (bossActive) {
    display.drawBitmap(bossX, bossY, spr_boss, 24, 16, SSD1306_WHITE);
  }

  // Dibujar Jugador (parpadeo si invulnerable)
  if (invulTimer == 0 || (invulTimer % 4) > 1) {
    if (fCnt % 4 < 2) display.drawBitmap(2, (int)shipY, spr_ship_thrust, 18, 10, SSD1306_WHITE);
    else display.drawBitmap(2, (int)shipY, spr_ship, 18, 10, SSD1306_WHITE);
  }

  // Dibujar Explosiones
  for (int i=0; i<MAX_EXPS; i++) {
    if (exps[i].active) {
      display.drawBitmap(exps[i].x, exps[i].y, expFrames[exps[i].frame], 8, 8, SSD1306_WHITE);
    }
  }

  // Dibujar HUD
  display.fillRect(0, 0, SCR_W, 9, SSD1306_BLACK);
  display.drawFastHLine(0, 9, SCR_W, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(2, 1);
  display.print(F("HP:"));
  display.drawRect(20, 2, MAX_HP*2 + 2, 5, SSD1306_WHITE);
  display.fillRect(21, 3, hp*2, 3, SSD1306_WHITE);
  
  display.setCursor(60, 1);
  display.print(F("SC:"));
  display.print(score);

  if (bossActive) {
    display.setCursor(100, 1);
    display.print(F("BOSS"));
    display.drawRect(100, 9, 28, 3, SSD1306_WHITE);
    display.fillRect(100, 9, (int)((bossHp/(float)BOSS_MAX_HP)*28.0f), 3, SSD1306_WHITE);
  }

  display.display();
  fCnt++;
  delay(16);
}
