// ============================================================
//  CAVE SWING COPTER
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Juego tipo "Swing Copter" en una cueva infinita.
//  Controles capacitivos por hardware:
//    GPIO 4  (Touch 0): Propulsar Arriba-Izquierda
//    GPIO 15 (Touch 3): Propulsar Arriba-Derecha
//
//  Características técnicas:
//    - Física de gravedad y momento inercial X/Y.
//    - Generación procedural de túnel (techo y suelo) que avanza.
//    - Detección de colisión de círculo contra líneas del terreno.
//    - Partículas de humo según la dirección del propulsor.
//    - Auto-calibración adaptativa de sensores táctiles capacitivos.
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

// ── Estado del Jugador (Helicóptero) ───────────────
float pX = 64.0f, pY = 32.0f;
float vX = 0.0f, vY = 0.0f;
const float GRAVITY = 0.12f;
const float THRUST_Y = -0.3f;
const float THRUST_X = 0.2f;
const float MAX_VX = 3.0f;
const float MAX_VY = 4.0f;
const int RADIUS = 3;

int score = 0;
bool gameOver = false;
uint32_t fCnt = 0;

// ── Terreno Procedural (Cueva) ─────────────────────
#define CAVE_SEGMENTS 32
int caveTop[CAVE_SEGMENTS];
int caveBot[CAVE_SEGMENTS];
float scrollOffset = 0.0f;
float scrollSpeed = 1.5f;

// ── Partículas (Humo) ──────────────────────────────
#define MAX_PARTS 15
struct Part { float x, y, vx, vy; int life; };
Part parts[MAX_PARTS];

// ============================================================
//  FUNCIONES AUXILIARES
// ============================================================

void generateCaveSegment(int idx, int prevIdx) {
  int gap = 35 - min((score / 10), 15); // El espacio se reduce con el puntaje
  int prevTop = caveTop[prevIdx];
  int t = prevTop + random(-8, 9);
  
  if (t < 2) t = 2;
  if (t > SCR_H - gap - 2) t = SCR_H - gap - 2;
  
  caveTop[idx] = t;
  caveBot[idx] = t + gap;
}

void initCave() {
  caveTop[0] = 10;
  caveBot[0] = 50;
  for (int i = 1; i < CAVE_SEGMENTS; i++) {
    generateCaveSegment(i, i - 1);
  }
}

void resetGame() {
  pX = SCR_W / 2; pY = SCR_H / 2;
  vX = 0.0f; vY = 0.0f;
  score = 0;
  scrollSpeed = 1.5f;
  gameOver = false;
  initCave();
  for (int i = 0; i < MAX_PARTS; i++) parts[i].life = 0;
}

void spawnSmoke(float vx, float vy) {
  for (int i = 0; i < MAX_PARTS; i++) {
    if (parts[i].life <= 0) {
      parts[i] = {pX, pY, vx + random(-5,6)*0.1f, vy + random(-5,6)*0.1f, random(5, 12)};
      break;
    }
  }
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
  resetGame();
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

  if (gameOver) {
    display.setCursor(35, 20);
    display.setTextColor(SSD1306_WHITE);
    display.print(F("GAME OVER"));
    display.setCursor(15, 40);
    display.print(F("Touch to restart"));
    display.display();
    if (btnL || btnR) { resetGame(); delay(200); }
    return;
  }

  // 2. FÍSICAS Y CONTROLES
  vY += GRAVITY;
  
  if (btnL) {
    vX -= THRUST_X;
    vY += THRUST_Y;
    if (fCnt % 2 == 0) spawnSmoke(0.5f, 0.5f);
  }
  if (btnR) {
    vX += THRUST_X;
    vY += THRUST_Y;
    if (fCnt % 2 == 0) spawnSmoke(-0.5f, 0.5f);
  }

  // Fricción y límites de velocidad
  vX *= 0.95f;
  if (vX > MAX_VX) vX = MAX_VX;
  if (vX < -MAX_VX) vX = -MAX_VX;
  if (vY > MAX_VY) vY = MAX_VY;
  if (vY < -MAX_VY) vY = -MAX_VY;

  pX += vX;
  pY += vY;

  // Rebote horizontal suave si toca los bordes de la pantalla (opcional)
  if (pX < RADIUS) { pX = RADIUS; vX = -vX * 0.5f; }
  if (pX > SCR_W - RADIUS) { pX = SCR_W - RADIUS; vX = -vX * 0.5f; }

  // 3. ACTUALIZAR TERRENO
  scrollOffset += scrollSpeed;
  if (scrollOffset >= (SCR_W / CAVE_SEGMENTS)) {
    scrollOffset = 0;
    // Desplazar array
    for (int i = 0; i < CAVE_SEGMENTS - 1; i++) {
      caveTop[i] = caveTop[i + 1];
      caveBot[i] = caveBot[i + 1];
    }
    // Generar nuevo segmento al final
    generateCaveSegment(CAVE_SEGMENTS - 1, CAVE_SEGMENTS - 2);
    score++;
    if (score % 20 == 0) scrollSpeed += 0.1f;
  }

  // 4. COLISIONES
  int segW = SCR_W / CAVE_SEGMENTS;
  int pxIdx = constrain((int)(pX / segW), 0, CAVE_SEGMENTS - 1);
  
  // Interpolar altura aproximada del techo y suelo en X
  int localT = caveTop[pxIdx];
  int localB = caveBot[pxIdx];
  
  if (pY - RADIUS < localT || pY + RADIUS > localB) {
    gameOver = true;
  }

  // 5. RENDERIZADO
  
  // Dibujar terreno
  for (int i = 0; i < CAVE_SEGMENTS - 1; i++) {
    int x1 = i * segW - (int)scrollOffset;
    int x2 = (i + 1) * segW - (int)scrollOffset;
    
    // Rellenar techo
    display.fillTriangle(x1, 0, x2, 0, x1, caveTop[i], SSD1306_WHITE);
    display.fillTriangle(x2, 0, x1, caveTop[i], x2, caveTop[i+1], SSD1306_WHITE);
    
    // Rellenar suelo
    display.fillTriangle(x1, SCR_H, x2, SCR_H, x1, caveBot[i], SSD1306_WHITE);
    display.fillTriangle(x2, SCR_H, x1, caveBot[i], x2, caveBot[i+1], SSD1306_WHITE);
  }

  // Dibujar partículas (Humo)
  for (int i = 0; i < MAX_PARTS; i++) {
    if (parts[i].life > 0) {
      parts[i].x += parts[i].vx;
      parts[i].y += parts[i].vy;
      parts[i].life--;
      display.drawPixel((int)parts[i].x, (int)parts[i].y, SSD1306_WHITE);
    }
  }

  // Dibujar Copter
  display.fillCircle((int)pX, (int)pY, RADIUS, SSD1306_WHITE);
  // Aspas animadas
  if (fCnt % 4 < 2) {
    display.drawFastHLine((int)pX - 4, (int)pY - RADIUS - 1, 9, SSD1306_WHITE);
  } else {
    display.drawFastHLine((int)pX - 2, (int)pY - RADIUS - 1, 5, SSD1306_WHITE);
  }

  // Dibujar Puntaje
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setCursor(SCR_W / 2 - 10, 2);
  display.print(score);

  display.display();
  fCnt++;
  delay(16);
}
