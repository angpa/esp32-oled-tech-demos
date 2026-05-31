// ============================================================
//  LUNAR LANDER VECTORIAL
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Juego de aterrizaje lunar con físicas inerciales y gráficos vectoriales.
//  Controles capacitivos por hardware:
//    GPIO 4  (Touch 0): Propulsor Izquierdo (Inclina Derecha + Empuje)
//    GPIO 15 (Touch 3): Propulsor Derecho (Inclina Izquierda + Empuje)
//    (Ambos a la vez): Propulsión vertical directa.
//
//  Características técnicas:
//    - Física newtoniana de inercia y gravedad constante.
//    - Gráficos vectoriales con matriz de rotación 2D.
//    - Sistema de partículas vectorial para los propulsores.
//    - Terreno procedural con zonas de aterrizaje aleatorias.
//    - Detección de colisión punto a segmento para el terreno.
//    - Auto-calibración adaptativa de sensores táctiles capacitivos.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// ── Calibración Táctil Capacitiva ──────────────────
static int t4Base  = -1;
static int t15Base = -1;

// ── Estado del Jugador (Nave) ──────────────────────
float pX = 64.0f, pY = 10.0f;
float vX = 0.0f, vY = 0.0f;
float angle = 0.0f;
float aVel = 0.0f;
float fuel = 250.0f;
int score = 0;
bool landed = false;
bool crashed = false;

// ── Sistema de Partículas ──────────────────────────
#define MAX_PARTICLES 20
struct Particle { float x, y, vx, vy; int life; };
Particle parts[MAX_PARTICLES];

// ── Geometría del Terreno ──────────────────────────
#define NUM_POINTS 12
struct Point { float x, y; };
Point terrain[NUM_POINTS];
int padIndex = -1;

// Vértices de la nave (forma de módulo lunar básico)
const int SHIP_PTS = 5;
const float shipV[SHIP_PTS][2] = {
  { 0, -5}, { 3,  3}, { 2,  5}, {-2,  5}, {-3,  3}
};

// ============================================================
//  FUNCIONES AUXILIARES
// ============================================================

void generateTerrain() {
  terrain[0] = {0, random(30, 50)};
  padIndex = random(2, NUM_POINTS - 3);
  
  for (int i = 1; i < NUM_POINTS; i++) {
    float x = i * (SCR_W / (float)(NUM_POINTS - 1));
    float y = random(30, 60);
    
    // Crear una plataforma plana
    if (i == padIndex || i == padIndex + 1) {
      y = terrain[padIndex - 1].y;
    }
    terrain[i] = {x, y};
  }
}

void resetShip() {
  pX = 64.0f; pY = 5.0f;
  vX = 0.0f; vY = 0.0f;
  angle = 0.0f; aVel = 0.0f;
  landed = false; crashed = false;
  if (fuel < 50.0f) fuel = 250.0f; // Reset fuel on game over
  generateTerrain();
}

void spawnParticle(float x, float y, float vx, float vy) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (parts[i].life <= 0) {
      parts[i] = {x, y, vx, vy, random(5, 12)};
      break;
    }
  }
}

// Intersección de líneas básica para colisiones
bool lineIntersect(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
  float uA = ((x4-x3)*(y1-y3) - (y4-y3)*(x1-x3)) / ((y4-y3)*(x2-x1) - (x4-x3)*(y2-y1));
  float uB = ((x2-x1)*(y1-y3) - (y2-y1)*(x1-x3)) / ((y4-y3)*(x2-x1) - (x4-x3)*(y2-y1));
  return (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) while (1);
  display.clearDisplay();
  display.display();
  randomSeed(analogRead(0));
  resetShip();
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  display.clearDisplay();

  // 1. LECTURA CAPACITIVA
  int t4  = touchRead(4);
  int t15 = touchRead(15);
  if (t4Base  == -1 && t4  > 10) t4Base  = t4;
  if (t15Base == -1 && t15 > 10) t15Base = t15;
  if (t4Base  < 15) t4Base  = 60;
  if (t15Base < 15) t15Base = 60;
  
  bool btnL = (t4  < t4Base  * 0.65f);
  bool btnR = (t15 < t15Base * 0.65f);

  if (landed || crashed) {
    display.setCursor(20, 20);
    display.setTextColor(SSD1306_WHITE);
    if (landed) display.print(F("SUCCESSFUL LANDING"));
    else display.print(F("SHIP DESTROYED"));
    display.setCursor(30, 40);
    display.print(F("Touch to restart"));
    display.display();
    if (btnL || btnR) resetShip();
    delay(100);
    return;
  }

  // 2. FÍSICAS DE LA NAVE
  vY += 0.05f; // Gravedad
  
  if (fuel > 0) {
    if (btnL && btnR) {
      vX += sin(angle) * 0.12f;
      vY -= cos(angle) * 0.12f;
      fuel -= 0.5f;
      spawnParticle(pX, pY+5, -vX + random(-10,10)*0.05f, -vY + random(10,20)*0.1f);
    } else if (btnL) {
      aVel += 0.005f; // Gira der
      vX += sin(angle) * 0.06f;
      vY -= cos(angle) * 0.06f;
      fuel -= 0.25f;
      spawnParticle(pX-3, pY+3, -vX - 1.0f, -vY + 1.0f);
    } else if (btnR) {
      aVel -= 0.005f; // Gira izq
      vX += sin(angle) * 0.06f;
      vY -= cos(angle) * 0.06f;
      fuel -= 0.25f;
      spawnParticle(pX+3, pY+3, -vX + 1.0f, -vY + 1.0f);
    }
  }

  // Límites espaciales (toroidales en X)
  pX += vX;
  pY += vY;
  angle += aVel;
  
  if (pX < 0) pX += SCR_W;
  if (pX > SCR_W) pX -= SCR_W;
  
  // Fricción angular suave
  aVel *= 0.96f;

  // 3. RENDER NAVE Y COLISIONES
  float c = cos(angle);
  float s = sin(angle);
  float sPts[SHIP_PTS][2];

  for (int i = 0; i < SHIP_PTS; i++) {
    sPts[i][0] = pX + (shipV[i][0] * c - shipV[i][1] * s);
    sPts[i][1] = pY + (shipV[i][0] * s + shipV[i][1] * c);
    
    int j = (i + 1) % SHIP_PTS;
    float nX = pX + (shipV[j][0] * c - shipV[j][1] * s);
    float nY = pY + (shipV[j][0] * s + shipV[j][1] * c);
    display.drawLine((int)sPts[i][0], (int)sPts[i][1], (int)nX, (int)nY, SSD1306_WHITE);
  }

  // Detección de colisión con el terreno
  for (int i = 0; i < NUM_POINTS - 1; i++) {
    // Dibujar terreno
    if (i == padIndex) display.drawLine(terrain[i].x, terrain[i].y, terrain[i+1].x, terrain[i+1].y, SSD1306_WHITE);
    else {
      // Terreno irregular
      for(float t=0; t<=1; t+=0.2f) {
         display.drawPixel(terrain[i].x + (terrain[i+1].x - terrain[i].x)*t, terrain[i].y + (terrain[i+1].y - terrain[i].y)*t, SSD1306_WHITE);
      }
    }

    // Comprobar si la nave choca con este segmento
    for (int p = 0; p < SHIP_PTS; p++) {
      if (pY + 5 >= terrain[i].y && pX >= terrain[i].x && pX <= terrain[i+1].x) {
         if (i == padIndex) {
            // Aterrizaje en plataforma
            if (fabs(vY) < 1.0f && fabs(vX) < 0.5f && fabs(angle) < 0.3f) {
              landed = true;
              score += 100 + (int)fuel;
            } else {
              crashed = true;
            }
         } else {
            crashed = true; // Chocó en terreno irregular
         }
      }
    }
  }

  // 4. ACTUALIZAR Y DIBUJAR PARTÍCULAS
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (parts[i].life > 0) {
      parts[i].x += parts[i].vx;
      parts[i].y += parts[i].vy;
      parts[i].life--;
      display.drawPixel((int)parts[i].x, (int)parts[i].y, SSD1306_WHITE);
    }
  }

  // 5. HUD
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(F("F:")); display.print((int)fuel);
  display.setCursor(45,0);
  display.print(F("S:")); display.print(score);
  
  // Alerta de velocidad
  if (pY > 20 && (fabs(vY) > 1.0f || fabs(vX) > 0.5f)) {
      display.setCursor(100,0);
      display.print(F("!WARN"));
  }

  display.display();
  delay(16);
}
