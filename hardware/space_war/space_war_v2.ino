// ============================================================
//  SPACE WAR v2 - Cinematic Space Demo
//  Para ESP32 + OLED SSD1306 128x64 Bicolor
//  Franja AMARILLA (0-15px) | Franja AZUL (16-63px)
//  ~11 segundos de pura estetica espacial
// ============================================================
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================
//  SPRITES - Pixel Art de alta fidelidad
// ============================================================

// Nave jugador 18x10 (muy detallada)
const unsigned char PROGMEM spr_ship[] = {
    0b00000000, 0b00000100, 0b00, //         *
    0b00000000, 0b00001110, 0b00, //        ***
    0b00000000, 0b00011111, 0b00, //       *****
    0b00011111, 0b11111111, 0b10, //    ************
    0b01111111, 0b11111111, 0b11, //  **************
    0b11111111, 0b11111111, 0b11, // ****************
    0b11111111, 0b11111111, 0b11, // ****************
    0b01111111, 0b11111111, 0b11, //  ***************
    0b00011111, 0b10111111, 0b10, //    ******..*****
    0b00000001, 0b00000001, 0b00, //        *      *
};

// Nave con thrust llamas (18x10)
const unsigned char PROGMEM spr_ship_thrust[] = {
    0b00000000, 0b00000100, 0b00, 0b00000000, 0b00001110, 0b00,
    0b00000000, 0b00011111, 0b00, 0b00011111, 0b11111111, 0b10,
    0b01111111, 0b11111111, 0b11, 0b11111111, 0b11111111, 0b11,
    0b11111111, 0b11111111, 0b11, 0b01111111, 0b11111111, 0b11,
    0b00011111, 0b10111111, 0b10, 0b00000011, 0b01000011, 0b00, // llamas extra
};

// Boss ship 24x16 (enorme, detallado)
const unsigned char PROGMEM spr_boss[] = {
    0b00000001, 0b11111100, 0b00000000, 0b00000011, 0b11111110, 0b00000000,
    0b00000111, 0b11111111, 0b00000000, 0b00001111, 0b11111111, 0b10000000,
    0b01111111, 0b11111111, 0b11100000, 0b11111111, 0b11111111, 0b11111000,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111000, 0b01111111, 0b11111111, 0b11100000,
    0b00001111, 0b11111111, 0b10000000, 0b00000111, 0b11111111, 0b00000000,
    0b00000011, 0b11111110, 0b00000000, 0b00000001, 0b11111100, 0b00000000,
};

// Explosion frames 8x8
const unsigned char PROGMEM spr_exp0[] = {0x18, 0x18, 0x18, 0x7E,
                                          0x7E, 0x18, 0x18, 0x18};
const unsigned char PROGMEM spr_exp1[] = {0x24, 0x5A, 0x3C, 0xFF,
                                          0xFF, 0x3C, 0x5A, 0x24};
const unsigned char PROGMEM spr_exp2[] = {0x42, 0xBD, 0x7E, 0xFF,
                                          0xFF, 0x7E, 0xBD, 0x42};
const unsigned char PROGMEM spr_exp3[] = {0x81, 0x66, 0x3C, 0xDB,
                                          0xDB, 0x3C, 0x66, 0x81};
const unsigned char PROGMEM spr_exp4[] = {0x81, 0x42, 0x24, 0x18,
                                          0x18, 0x24, 0x42, 0x81};
const unsigned char PROGMEM spr_exp5[] = {0x42, 0x24, 0x18, 0x00,
                                          0x00, 0x18, 0x24, 0x42};

// Misil enemigo 6x4
const unsigned char PROGMEM spr_missile[] = {
    0b11110000,
    0b11111100,
    0b11111100,
    0b11110000,
};

// ============================================================
//  SISTEMA DE PARALLAX (4 capas)
// ============================================================
#define N_FAR 18
#define N_MID 10
#define N_NEAR 6
#define N_NEBULA 4

struct Star {
  int16_t x, y;
};
Star sFar[N_FAR], sMid[N_MID], sNear[N_NEAR];

// Nebulas (manchas difusas)
struct Nebula {
  int16_t x, y;
  uint8_t r;
  float spd;
};
Nebula nebulas[N_NEBULA];

uint16_t rng = 0xACE1;
uint16_t rnd() {
  rng ^= rng << 7;
  rng ^= rng >> 9;
  rng ^= rng << 8;
  return rng;
}

void initStars() {
  for (int i = 0; i < N_FAR; i++) {
    sFar[i].x = rnd() % 128;
    sFar[i].y = 16 + rnd() % 48;
  }
  for (int i = 0; i < N_MID; i++) {
    sMid[i].x = rnd() % 128;
    sMid[i].y = 16 + rnd() % 48;
  }
  for (int i = 0; i < N_NEAR; i++) {
    sNear[i].x = rnd() % 128;
    sNear[i].y = 16 + rnd() % 48;
  }
  for (int i = 0; i < N_NEBULA; i++) {
    nebulas[i].x = rnd() % 128;
    nebulas[i].y = 16 + rnd() % 40;
    nebulas[i].r = 4 + rnd() % 6;
    nebulas[i].spd = 0.3f + i * 0.1f;
  }
}

void updateStars(float speedMul) {
  // Capa 1: muy lenta
  for (int i = 0; i < N_FAR; i++) {
    if ((rng + i) % 3 == 0)
      sFar[i].x -= speedMul > 0 ? 1 : 0;
    if (sFar[i].x < 0) {
      sFar[i].x = 127;
      sFar[i].y = 16 + rnd() % 48;
    }
  }
  // Capa 2: normal
  for (int i = 0; i < N_MID; i++) {
    sMid[i].x -= 1 * speedMul;
    if (sMid[i].x < 0) {
      sMid[i].x = 127;
      sMid[i].y = 16 + rnd() % 48;
    }
  }
  // Capa 3: rapida (lineas de velocidad)
  for (int i = 0; i < N_NEAR; i++) {
    sNear[i].x -= 3 * speedMul;
    if (sNear[i].x < 0) {
      sNear[i].x = 127;
      sNear[i].y = 16 + rnd() % 48;
    }
  }
  // Nebulas
  for (int i = 0; i < N_NEBULA; i++) {
    nebulas[i].x -= nebulas[i].spd * speedMul;
    if (nebulas[i].x < -10) {
      nebulas[i].x = 135;
      nebulas[i].y = 16 + rnd() % 40;
      nebulas[i].r = 4 + rnd() % 6;
    }
  }
}

void drawStars() {
  // Nebulas difusas (solo borde)
  for (int i = 0; i < N_NEBULA; i++) {
    display.drawCircle(nebulas[i].x, nebulas[i].y, nebulas[i].r, SSD1306_WHITE);
    // Puntos interiores escasos para dar volumen
    display.drawPixel(nebulas[i].x, nebulas[i].y, SSD1306_WHITE);
    display.drawPixel(nebulas[i].x + 2, nebulas[i].y - 1, SSD1306_WHITE);
    display.drawPixel(nebulas[i].x - 2, nebulas[i].y + 1, SSD1306_WHITE);
  }
  for (int i = 0; i < N_FAR; i++)
    display.drawPixel(sFar[i].x, sFar[i].y, SSD1306_WHITE);
  for (int i = 0; i < N_MID; i++) {
    display.drawPixel(sMid[i].x, sMid[i].y, SSD1306_WHITE);
    if (i % 3 == 0)
      display.drawPixel(sMid[i].x + 1, sMid[i].y, SSD1306_WHITE);
  }
  for (int i = 0; i < N_NEAR; i++)
    display.drawLine(sNear[i].x, sNear[i].y, sNear[i].x + 4, sNear[i].y,
                     SSD1306_WHITE);
}

// ============================================================
//  EFECTOS ESPECIALES
// ============================================================
#define MAX_EXP 6
struct Explosion {
  int16_t x, y;
  int8_t frame, timer;
  bool active;
};
Explosion exps[MAX_EXP];
const unsigned char *const expFrames[] = {spr_exp0, spr_exp1, spr_exp2,
                                          spr_exp3, spr_exp4, spr_exp5};

void spawnExp(int x, int y) {
  for (int i = 0; i < MAX_EXP; i++)
    if (!exps[i].active) {
      exps[i] = {x, y, 0, 0, true};
      break;
    }
}
void updateExps() {
  for (int i = 0; i < MAX_EXP; i++) {
    if (!exps[i].active)
      continue;
    if (++exps[i].timer >= 3) {
      exps[i].timer = 0;
      if (++exps[i].frame >= 6)
        exps[i].active = false;
    }
  }
}
void drawExps() {
  for (int i = 0; i < MAX_EXP; i++) {
    if (!exps[i].active)
      continue;
    display.drawBitmap(exps[i].x, exps[i].y, expFrames[exps[i].frame], 8, 8,
                       SSD1306_WHITE);
  }
}

// Particulas de motor
#define MAX_PART 20
struct Particle {
  float x, y, vx, vy;
  int8_t life, maxLife;
};
Particle parts[MAX_PART];

void spawnPart(float x, float y, float vx, float vy, int life) {
  for (int i = 0; i < MAX_PART; i++)
    if (parts[i].life <= 0) {
      parts[i] = {x, y, vx, vy, life, life};
      break;
    }
}
void updateParts() {
  for (int i = 0; i < MAX_PART; i++) {
    if (parts[i].life <= 0)
      continue;
    parts[i].x += parts[i].vx;
    parts[i].y += parts[i].vy;
    parts[i].life--;
  }
}
void drawParts() {
  for (int i = 0; i < MAX_PART; i++) {
    if (parts[i].life <= 0)
      continue;
    // Fade out: solo dibuja si life > maxLife/2
    display.drawPixel((int)parts[i].x, (int)parts[i].y, SSD1306_WHITE);
  }
}

// ============================================================
//  LASER CHARGE + BEAM
// ============================================================
int laserChargeTimer = 0;
bool laserFiring = false;
int laserLen = 0;
int laserY = 0;

void drawLaserBeam(int fromX, int y, int len) {
  // Doble linea con brillo
  display.drawLine(fromX, y - 1, fromX + len, y - 1, SSD1306_WHITE);
  display.drawLine(fromX, y, fromX + len, y, SSD1306_WHITE);
  display.drawLine(fromX, y + 1, fromX + len, y + 1, SSD1306_WHITE);
  // Bordes del beam - parpadeo
  display.drawLine(fromX, y - 2, fromX + len / 2, y - 2, SSD1306_WHITE);
  display.drawLine(fromX, y + 2, fromX + len / 2, y + 2, SSD1306_WHITE);
}

// ============================================================
//  WARP EFFECT (intro)
// ============================================================
void drawWarpFrame(int intensity) {
  // Las estrellas se estiran hacia el centro
  int cx = 64, cy = 32;
  for (int i = 0; i < 30; i++) {
    float angle = (i * 0.628f); // 2*PI/10 aprox
    float r = 8 + i * 3;
    int x1 = cx + cos(angle) * r;
    int y1 = cy + sin(angle) * r * 0.5;
    int x2 = cx + cos(angle) * (r + intensity * 0.4);
    int y2 = cy + sin(angle) * (r + intensity * 0.4) * 0.5;
    if (x1 >= 0 && x1 < 128 && y1 >= 16 && y1 < 64 && x2 >= 0 && x2 < 128 &&
        y2 >= 16 && y2 < 64)
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}

// ============================================================
//  SCREEN SHAKE
// ============================================================
int shakeX = 0, shakeY = 0, shakePow = 0;
void triggerShake(int power) { shakePow = power; }
void updateShake() {
  if (shakePow > 0) {
    shakeX = (rnd() % 5) - 2;
    shakeY = (rnd() % 3) - 1;
    shakePow--;
  } else {
    shakeX = 0;
    shakeY = 0;
  }
}

// ============================================================
//  HUD - Franja Amarilla
// ============================================================
void drawHUD(int hull, int score, int shield, bool danger, long frame) {
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  // Barras de estado
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // HULL
  display.setCursor(0, 1);
  display.print(F("HULL"));
  int hw = map(constrain(hull, 0, 100), 0, 100, 0, 24);
  display.drawRect(24, 1, 26, 6, SSD1306_WHITE);
  display.fillRect(25, 2, hw, 4, SSD1306_WHITE);

  // SHIELD
  display.setCursor(54, 1);
  display.print(F("SH"));
  int sw = map(constrain(shield, 0, 100), 0, 100, 0, 18);
  display.drawRect(66, 1, 20, 6, SSD1306_WHITE);
  display.fillRect(67, 2, sw, 4, SSD1306_WHITE);

  // SCORE
  display.setCursor(90, 1);
  display.print(F("SC:"));
  display.print(score);

  // DANGER parpadeante
  if (danger && (frame / 6) % 2 == 0) {
    display.setCursor(80, 8);
    display.print(F("!!!DANGER!!!"));
  }
}

// ============================================================
//  ESTADO DEL JUEGO - Maquina de escenas
// ============================================================
enum Scene {
  SCENE_WARP_IN,
  SCENE_PATROL,
  SCENE_AMBUSH,
  SCENE_BOSS_APPROACH,
  SCENE_BOSS_BATTLE,
  SCENE_LASER_CHARGE,
  SCENE_LASER_FIRE,
  SCENE_EXPLOSION_FINALE,
  SCENE_VICTORY
};

Scene currentScene = SCENE_WARP_IN;
long frame = 0;
long sceneFrame = 0;
int hull = 100, shield = 80, score = 0;
bool dangerFlag = false;

// Nave del jugador
float shipX = 10, shipY = 36;
float shipVY = 0;
bool thrustOn = true;

// Enemigos simples para la escena
struct Enemy {
  float x, y;
  int type;
  float phase;
  bool active;
  int hp;
};
#define MAX_E 6
Enemy enemies[MAX_E];
int eCount = 0;

// Balas
#define MAX_B 8
struct Bullet {
  float x, y;
  float vx, vy;
  bool active;
  bool isEnemy;
};
Bullet bullets[MAX_B];

void spawnBullet(float x, float y, float vx, float vy, bool isE) {
  for (int i = 0; i < MAX_B; i++)
    if (!bullets[i].active) {
      bullets[i] = {x, y, vx, vy, true, isE};
      break;
    }
}

void spawnEnemy(float x, float y, int type) {
  if (eCount >= MAX_E)
    return;
  for (int i = 0; i < MAX_E; i++)
    if (!enemies[i].active) {
      enemies[i] = {x, y, type, 0, true, 2};
      eCount++;
      break;
    }
}

// Boss
float bossX = 200, bossY = 24;
int bossHp = 30;
bool bossActive = false;

// ============================================================
//  TRANSICION DE ESCENA
// ============================================================
void setScene(int s_int) {
  Scene s = (Scene)s_int;
  currentScene = s;
  sceneFrame = 0;
  // Reset algunos estados por escena
  if (s == SCENE_AMBUSH) {
    for (int i = 0; i < MAX_E; i++)
      enemies[i].active = false;
    eCount = 0;
    // Spawnear 3 enemigos desde la derecha
    spawnEnemy(140, 22, 0);
    spawnEnemy(155, 34, 1);
    spawnEnemy(148, 46, 0);
  }
  if (s == SCENE_BOSS_APPROACH) {
    bossX = 170;
    bossY = 24;
    bossActive = true;
    bossHp = 30;
    for (int i = 0; i < MAX_E; i++)
      enemies[i].active = false;
    eCount = 0;
  }
  if (s == SCENE_LASER_CHARGE) {
    laserChargeTimer = 0;
    laserFiring = false;
    laserLen = 0;
  }
  if (s == SCENE_LASER_FIRE) {
    laserFiring = true;
    laserLen = 0;
    laserY = (int)shipY + 4;
  }
  if (s == SCENE_EXPLOSION_FINALE) {
    triggerShake(30);
    bossActive = false;
    // Montones de explosiones
    for (int i = 0; i < 5; i++)
      spawnExp(bossX + rnd() % 20, bossY + rnd() % 14);
  }
}

// ============================================================
//  DIBUJAR NAVE CON SHAKE
// ============================================================
void drawShip(float sx, float sy) {
  int x = (int)sx + shakeX;
  int y = (int)sy + shakeY;
  if ((frame / 3) % 2 == 0)
    display.drawBitmap(x, y, spr_ship, 18, 10, SSD1306_WHITE);
  else
    display.drawBitmap(x, y, spr_ship_thrust, 18, 10, SSD1306_WHITE);

  // Motor glow: particulas de exhaust cada 2 frames
  if (frame % 2 == 0) {
    float ex = x - 1, ey = y + 7;
    spawnPart(ex, ey, -(1.5f + (rnd() % 10) / 10.0f),
              (((int)(rnd() % 5)) - 2) * 0.3f, 5 + rnd() % 5);
    spawnPart(ex, ey + 2, -(1.5f + (rnd() % 10) / 10.0f),
              (((int)(rnd() % 5)) - 2) * 0.3f, 5 + rnd() % 5);
  }
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for (;;)
      ;
  }
  display.clearDisplay();
  initStars();
  for (int i = 0; i < MAX_PART; i++)
    parts[i].life = 0;
  for (int i = 0; i < MAX_B; i++)
    bullets[i].active = false;
  for (int i = 0; i < MAX_EXP; i++)
    exps[i].active = false;
  for (int i = 0; i < MAX_E; i++)
    enemies[i].active = false;
}

void loop() {
  display.clearDisplay();

  // ---------- WARP IN (0-60f / ~2s) ----------
  if (currentScene == SCENE_WARP_IN) {
    // Fondo negro con efecto warp
    int intensity = sceneFrame * 2;
    if (intensity > 50)
      intensity = 50;
    drawWarpFrame(intensity);

    // HUD amarillo aparece gradual
    display.drawLine(0, 15, 127, 15, SSD1306_WHITE);
    if (sceneFrame > 20) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(20, 1);
      if ((frame / 8) % 2 == 0)
        display.print(F("-- WARPING IN --"));
    }

    // La nave "sale" del centro hacia la izquierda
    float t = sceneFrame / 60.0f;
    float sx = 64 - t * 54;
    float sy = 32 - 5 + sin(sceneFrame * 0.2) * 3;
    if (sx < 10)
      sx = 10;
    drawShip(sx, sy);
    shipX = sx;
    shipY = sy;

    updateStars(0.5);

    if (sceneFrame >= 60)
      setScene(SCENE_PATROL);
  }

  // ---------- PATROL (60-150f / ~3s) ----------
  else if (currentScene == SCENE_PATROL) {
    updateStars(1.0);
    drawStars();

    // Movimiento suave de nave
    float targetY =
        30 + sin(sceneFrame * 0.05) * 10 + cos(sceneFrame * 0.03) * 6;
    shipVY = (targetY - shipY) * 0.1f;
    shipY += shipVY;

    // Auto-disparo cada 18f
    if (sceneFrame % 18 == 0) {
      spawnBullet(shipX + 18, shipY + 4, 5, 0, false);
      spawnBullet(shipX + 18, shipY + 6, 5, 0.3, false);
    }

    // Actualizar balas
    for (int i = 0; i < MAX_B; i++) {
      if (!bullets[i].active)
        continue;
      bullets[i].x += bullets[i].vx;
      bullets[i].y += bullets[i].vy;
      if (bullets[i].x > 128 || bullets[i].y < 16 || bullets[i].y > 63)
        bullets[i].active = false;
    }

    // Score sube
    if (sceneFrame % 10 == 0) {
      score += 5;
    }

    // Dibujar balas
    for (int i = 0; i < MAX_B; i++) {
      if (!bullets[i].active || bullets[i].isEnemy)
        continue;
      display.drawLine(bullets[i].x, bullets[i].y, bullets[i].x + 6,
                       bullets[i].y, SSD1306_WHITE);
      display.drawLine(bullets[i].x + 1, bullets[i].y + 1, bullets[i].x + 5,
                       bullets[i].y + 1, SSD1306_WHITE);
    }

    updateParts();
    drawParts();
    drawShip(shipX, shipY);
    updateExps();
    drawExps();
    drawHUD(hull, score, shield, false, frame);

    if (sceneFrame >= 90)
      setScene(SCENE_AMBUSH);
  }

  // ---------- AMBUSH (150-260f / ~3.5s) ----------
  else if (currentScene == SCENE_AMBUSH) {
    updateStars(1.5); // Más rápido = más tensión
    drawStars();

    // Nave esquiva
    float targetY = 28 + sin(sceneFrame * 0.12) * 15;
    shipVY = (targetY - shipY) * 0.15f;
    shipY += shipVY;

    // Disparo rápido
    if (sceneFrame % 10 == 0) {
      spawnBullet(shipX + 18, shipY + 4, 6, 0, false);
    }

    // Mover enemigos
    for (int i = 0; i < MAX_E; i++) {
      if (!enemies[i].active)
        continue;
      enemies[i].phase += 0.1;
      enemies[i].x -= 1.5;
      if (enemies[i].type == 0)
        enemies[i].y += sin(enemies[i].phase) * 1.5;
      if (enemies[i].type == 1)
        enemies[i].y += cos(enemies[i].phase) * 2.0;

      // Disparo enemigo cada 30f desfasado
      if ((sceneFrame + i * 10) % 35 == 0 && enemies[i].x < 100) {
        spawnBullet(enemies[i].x, enemies[i].y + 4, -3, 0, true);
      }

      // Salir de pantalla
      if (enemies[i].x < -20) {
        enemies[i].active = false;
        eCount--;
      }
    }

    // Colisiones bala->enemigo
    for (int b = 0; b < MAX_B; b++) {
      if (!bullets[b].active || bullets[b].isEnemy)
        continue;
      bullets[b].x += bullets[b].vx;
      bullets[b].y += bullets[b].vy;
      if (bullets[b].x > 128) {
        bullets[b].active = false;
        continue;
      }
      for (int e = 0; e < MAX_E; e++) {
        if (!enemies[e].active)
          continue;
        if (abs(bullets[b].x - enemies[e].x) < 8 &&
            abs(bullets[b].y - enemies[e].y) < 8) {
          bullets[b].active = false;
          enemies[e].hp--;
          if (enemies[e].hp <= 0) {
            spawnExp(enemies[e].x, enemies[e].y);
            triggerShake(8);
            enemies[e].active = false;
            eCount--;
            score += 50;
          }
        }
      }
    }

    // Balas enemigas -> nave
    for (int b = 0; b < MAX_B; b++) {
      if (!bullets[b].active || !bullets[b].isEnemy)
        continue;
      bullets[b].x += bullets[b].vx;
      bullets[b].y += bullets[b].vy;
      if (bullets[b].x < 0 || bullets[b].y < 16 || bullets[b].y > 63) {
        bullets[b].active = false;
        continue;
      }
      if (abs(bullets[b].x - shipX) < 16 && abs(bullets[b].y - shipY) < 10) {
        bullets[b].active = false;
        if (shield > 0) {
          shield -= 15;
          triggerShake(5);
        } else {
          hull -= 10;
          triggerShake(10);
          dangerFlag = true;
        }
      }
    }

    // Dibujar balas jugador
    for (int i = 0; i < MAX_B; i++) {
      if (!bullets[i].active || bullets[i].isEnemy)
        continue;
      display.drawLine(bullets[i].x, bullets[i].y, bullets[i].x + 6,
                       bullets[i].y, SSD1306_WHITE);
      display.drawLine(bullets[i].x + 1, bullets[i].y + 1, bullets[i].x + 5,
                       bullets[i].y + 1, SSD1306_WHITE);
    }
    // Balas enemigas (rectangulo)
    for (int i = 0; i < MAX_B; i++) {
      if (!bullets[i].active || !bullets[i].isEnemy)
        continue;
      display.drawRect(bullets[i].x, bullets[i].y, 5, 3, SSD1306_WHITE);
    }

    // Dibujar enemigos (como rects con detalle pixelado)
    for (int i = 0; i < MAX_E; i++) {
      if (!enemies[i].active)
        continue;
      int ex = (int)enemies[i].x, ey = (int)enemies[i].y;
      // Dibujo de enemigo tipo "caza" manual
      display.drawRect(ex, ey, 10, 8, SSD1306_WHITE);
      display.drawLine(ex - 4, ey + 2, ex, ey + 2, SSD1306_WHITE);     // ala
      display.drawLine(ex - 4, ey + 5, ex, ey + 5, SSD1306_WHITE);     // ala
      display.drawLine(ex - 6, ey + 3, ex - 4, ey + 4, SSD1306_WHITE); // motor
      display.fillRect(ex + 2, ey + 2, 3, 4, SSD1306_WHITE); // cockpit
      // HP indicator
      display.drawPixel(ex + 5, ey - 2, SSD1306_WHITE);
      if (enemies[i].hp > 1)
        display.drawPixel(ex + 7, ey - 2, SSD1306_WHITE);
    }

    updateShake();
    updateParts();
    drawParts();
    drawShip(shipX, shipY);
    updateExps();
    drawExps();
    drawHUD(hull, score, shield, dangerFlag, frame);
    dangerFlag = false;

    if (sceneFrame >= 110)
      setScene(SCENE_BOSS_APPROACH);
  }

  // ---------- BOSS APPROACH (260-340f / ~2.5s) ----------
  else if (currentScene == SCENE_BOSS_APPROACH) {
    updateStars(2.0); // Muy rapido: tension maxima
    drawStars();

    // Nave mantiene posicion estable
    float targetY = 30 + sin(sceneFrame * 0.06) * 8;
    shipY += (targetY - shipY) * 0.1f;

    // Boss entra por la derecha lentamente
    if (bossX > 98)
      bossX -= 1.2;

    // Advertencia: "!!! DANGER !!!" parpadeante
    if ((frame / 5) % 2 == 0) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(45, 8);
      display.print(F("!!! BOSS !!!"));
    }

    // Boss ship con borde de alerta
    if (bossActive && bossX < 128) {
      display.drawBitmap((int)bossX, (int)bossY, spr_boss, 24, 16,
                         SSD1306_WHITE);
      // Animacion de escudo del boss
      if ((frame / 4) % 2 == 0)
        display.drawCircle(bossX + 12, bossY + 8, 14, SSD1306_WHITE);
    }

    updateParts();
    drawParts();
    drawShip(shipX, shipY);
    updateExps();
    drawExps();
    drawHUD(hull, score, shield, true, frame);

    if (sceneFrame >= 80)
      setScene(SCENE_LASER_CHARGE);
  }

  // ---------- LASER CHARGE (340-400f / ~2s) ----------
  else if (currentScene == SCENE_LASER_CHARGE) {
    updateStars(0.3); // Casi quieto, cargando
    drawStars();

    // Nave vibra al cargar
    float sx = shipX + ((rnd() % 3) - 1) * 0.5f;
    float sy = shipY + ((rnd() % 3) - 1) * 0.5f;

    // Indicador de carga: circulo creciente
    int chargeRadius = sceneFrame / 3;
    if (chargeRadius < 18) {
      display.drawCircle(sx + 18, sy + 4, chargeRadius, SSD1306_WHITE);
      // Particulas convergentes hacia la punta
      for (int i = 0; i < 3; i++) {
        float a = (rnd() % 628) / 100.0f;
        int px = sx + 18 + cos(a) * (chargeRadius + 2);
        int py = sy + 4 + sin(a) * (chargeRadius + 2);
        if (py >= 16 && py < 64)
          spawnPart(px, py, (sx + 18 - px) * 0.2f, (sy + 4 - py) * 0.2f, 4);
      }
    }

    // Boss disparos mientras la nave carga
    if (sceneFrame % 20 == 0 && bossX < 120) {
      spawnBullet(bossX, bossY + 8, -2, sin(sceneFrame * 0.3) * 2, true);
      spawnBullet(bossX, bossY + 8, -2, cos(sceneFrame * 0.3) * 2, true);
    }

    // Balas enemigas
    for (int b = 0; b < MAX_B; b++) {
      if (!bullets[b].active || !bullets[b].isEnemy)
        continue;
      bullets[b].x += bullets[b].vx;
      bullets[b].y += bullets[b].vy;
      if (bullets[b].x < 0 || bullets[b].y < 16 || bullets[b].y > 63) {
        bullets[b].active = false;
        continue;
      }
      display.fillRect(bullets[b].x, bullets[b].y, 5, 3, SSD1306_WHITE);
    }

    // Boss
    if (bossActive)
      display.drawBitmap((int)bossX, (int)bossY, spr_boss, 24, 16,
                         SSD1306_WHITE);

    // HUD: "CHARGING" parpadeante
    display.drawLine(0, 15, 127, 15, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    if ((frame / 4) % 2 == 0) {
      display.setCursor(30, 1);
      display.print(F("** LASER CHARGE **"));
    }

    updateParts();
    drawParts();
    drawShip(sx, sy);
    updateExps();
    drawExps();

    if (sceneFrame >= 60)
      setScene(SCENE_LASER_FIRE);
  }

  // ---------- LASER FIRE (400-460f / ~2s) ----------
  else if (currentScene == SCENE_LASER_FIRE) {
    updateStars(0.5);
    drawStars();

    // Laser crece
    if (laserLen < 115)
      laserLen += 8;

    // Shake al disparar
    if (sceneFrame < 5)
      triggerShake(15);

    // Dibujar laser masivo
    int ly = (int)shipY + 4;
    drawLaserBeam(shipX + 18, ly, laserLen);

    // Chispas en el impacto
    if (laserLen > 50) {
      int impX = shipX + 18 + laserLen;
      for (int i = 0; i < 3; i++) {
        spawnPart(impX, ly, (rnd() % 6) - 3, ((int)(rnd() % 6)) - 3, 6);
      }
      // El boss recibe daño visual
      if (laserLen > (int)(bossX - shipX - 18)) {
        bossHp--;
        if (bossHp % 5 == 0)
          spawnExp(bossX + (rnd() % 20), bossY + (rnd() % 14));
        if (bossHp <= 0)
          setScene(SCENE_EXPLOSION_FINALE);
        score += 10;
      }
    }

    // Boss
    if (bossActive) {
      display.drawBitmap((int)bossX, (int)bossY, spr_boss, 24, 16,
                         SSD1306_WHITE);
      // HP bar del boss
      int bw = map(max(0, bossHp), 0, 30, 0, 24);
      display.drawRect((int)bossX, (int)bossY - 4, 24, 3, SSD1306_WHITE);
      display.fillRect((int)bossX + 1, (int)bossY - 3, bw, 1, SSD1306_WHITE);
    }

    updateShake();
    updateParts();
    drawParts();
    drawShip(shipX + shakeX, shipY + shakeY);
    updateExps();
    drawExps();
    drawHUD(hull, score + sceneFrame * 5, shield, false, frame);

    if (sceneFrame >= 60)
      setScene(SCENE_EXPLOSION_FINALE);
  }

  // ---------- EXPLOSION FINALE ----------
  else if (currentScene == SCENE_EXPLOSION_FINALE) {
    updateStars(0.8);
    drawStars();

    // Explosiones en cascada
    if (sceneFrame % 6 == 0) {
      spawnExp(100 + (rnd() % 25), 24 + (rnd() % 14));
      spawnExp(90 + (rnd() % 30), 20 + (rnd() % 20));
      triggerShake(12);
      score += 30;
    }

    // Flash inicial
    if (sceneFrame < 4) {
      display.fillRect(85, 16, 40, 48, SSD1306_WHITE);
    }

    updateShake();
    updateParts();
    drawParts();
    drawShip(shipX + shakeX, shipY + shakeY);
    updateExps();
    drawExps();
    drawHUD(hull, score, shield, false, frame);

    if (sceneFrame >= 55)
      setScene(SCENE_VICTORY);
  }

  // ---------- VICTORY ----------
  else if (currentScene == SCENE_VICTORY) {
    updateStars(0.5);
    drawStars();

    // Nave hace una pasada de celebracion
    shipX = 10 + sceneFrame * 0.5;
    shipY = 30 + sin(sceneFrame * 0.1) * 12;
    if (shipX > 130)
      shipX = 10;

    drawShip(shipX, shipY);
    updateParts();
    drawParts();
    updateExps();
    drawExps();

    // Titulo de victoria
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(22, 20);
    display.print(F("** ENEMY DEFEATED **"));
    display.setCursor(22, 30);
    display.print(F("FINAL SCORE:"));
    display.print(score);

    // Estrellitas de celebracion
    for (int i = 0; i < 3; i++) {
      int sx2 = (sceneFrame * 3 + i * 40) % 128;
      display.drawLine(sx2, 16, sx2 + 2, 14, SSD1306_WHITE);
      display.drawLine(sx2, 16, sx2 - 2, 14, SSD1306_WHITE);
      display.drawLine(sx2, 13, sx2, 16, SSD1306_WHITE);
    }

    drawHUD(hull, score, shield, false, frame);

    // Reiniciar en loop
    if (sceneFrame >= 120) {
      score = 0;
      hull = 100;
      shield = 80;
      shipX = 10;
      shipY = 36;
      for (int i = 0; i < MAX_B; i++)
        bullets[i].active = false;
      for (int i = 0; i < MAX_E; i++)
        enemies[i].active = false;
      eCount = 0;
      for (int i = 0; i < MAX_PART; i++)
        parts[i].life = 0;
      for (int i = 0; i < MAX_EXP; i++)
        exps[i].active = false;
      initStars();
      setScene(SCENE_WARP_IN);
    }
  }

  display.display();
  frame++;
  sceneFrame++;
  delay(33); // ~30 FPS
}
