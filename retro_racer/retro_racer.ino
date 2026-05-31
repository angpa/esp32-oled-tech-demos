// ============================================================
//  RETRO RACER — Juego de Carreras Pseudo-3D
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Juego de carreras pseudo-3D estilo OutRun / Pole Position.
//  Controles capacitivos por hardware:
//    GPIO 4  (Touch 0): Girar a la IZQUIERDA
//    GPIO 15 (Touch 3): Girar a la DERECHA
//
//  Características técnicas:
//    - Proyección pseudo-3D por segmentos con oclusión de colinas.
//    - Pista procedural con curvas suaves, hairpins y colinas.
//    - Franjas alternadas (rumble strips) para ilusión de velocidad.
//    - Silueta de montañas en el horizonte con estrellas titilantes.
//    - Árboles laterales escalados por perspectiva.
//    - Tráfico vehicular con detección de colisiones.
//    - Fuerza centrífuga realista en las curvas.
//    - Desaceleración sobre el pasto (fuera de pista).
//    - Líneas de velocidad y partículas de polvo dinámicas.
//    - Auto-calibración adaptativa de sensores táctiles capacitivos.
//    - HUD con velocímetro analógico, barra y distancia recorrida.
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

// ── Definición de Pista ────────────────────────────
#define TRACK_LEN  300       // Segmentos totales (circuito cerrado)
#define SEG_LEN    200.0f    // Longitud de cada segmento (unidades mundo)
#define ROAD_HW    120.0f    // Semi-ancho de la carretera (proyección)
#define DRAW_DIST  70        // Segmentos renderizados hacia adelante
#define CAM_DEPTH  100.0f    // Profundidad de cámara (controla el FOV)
#define CAM_H      80.0f     // Altura de cámara sobre la superficie

struct Seg { float curve, y; };
Seg track[TRACK_LEN];

// ── Segmentos Proyectados a Pantalla ───────────────
struct PSeg { float x, y, w; int idx; };
#define MAX_PROJ 70
PSeg proj[MAX_PROJ];
int nProj;

// ── Árboles Laterales ──────────────────────────────
#define N_TREES 50
struct RTree { int seg; float side; };
RTree trees[N_TREES];

// ── Vehículos de Tráfico ───────────────────────────
#define N_TRAFFIC 3
struct Traf { float z, lane, spd; };
Traf traf[N_TRAFFIC];

// ── Estado del Juego ───────────────────────────────
float pX       = 0.0f;    // Posición lateral (-1..1 = bordes de pista)
float spd      = 0.0f;    // Velocidad actual (unidades mundo / frame)
float maxSpd   = 10.0f;   // Velocidad máxima alcanzable
float pos      = 0.0f;    // Distancia recorrida (posición Z mundial)
uint32_t score = 0;        // Distancia en metros para el marcador
uint32_t fCnt  = 0;        // Contador de frames

// Silueta de montañas (precalculada)
int8_t mtns[SCR_W];

// ============================================================
//  GENERACIÓN PROCEDURAL DE PISTA
// ============================================================
void buildTrack() {
  for (int i = 0; i < TRACK_LEN; i++) {
    float t = (float)i;
    // Secciones variadas: rectas, curvas suaves, hairpins
    if      (i < 25)  track[i].curve = 0;
    else if (i < 60)  track[i].curve = sinf(t * 0.07f) * 0.5f;
    else if (i < 100) track[i].curve = sinf(t * 0.12f) * 1.3f;
    else if (i < 130) track[i].curve = 0;
    else if (i < 180) track[i].curve = sinf(t * 0.05f) * 0.7f + cosf(t * 0.11f) * 0.5f;
    else if (i < 230) track[i].curve = sinf(t * 0.14f) * 1.5f;
    else if (i < 270) track[i].curve = cosf(t * 0.06f) * 0.5f;
    else               track[i].curve = sinf(t * 0.09f) * 1.1f;

    // Colinas ondulantes (superposición de dos sinusoidales)
    track[i].y = sinf(t * 0.03f) * 150.0f + sinf(t * 0.07f) * 70.0f;
  }

  // Árboles al costado de la ruta
  for (int i = 0; i < N_TREES; i++) {
    trees[i].seg  = 5 + random(TRACK_LEN - 10);
    trees[i].side = random(2) ? 1.0f : -1.0f;
  }

  // Coches de tráfico a esquivar
  for (int i = 0; i < N_TRAFFIC; i++) {
    traf[i].z    = (float)(40 + i * 60) * SEG_LEN;
    traf[i].lane = random(2) ? 0.4f : -0.4f;
    traf[i].spd  = 3.0f + random(30) * 0.1f;
  }

  // Silueta de montañas en el horizonte
  float seed = random(1000) * 0.1f;
  for (int x = 0; x < SCR_W; x++) {
    float fx = (float)x / SCR_W;
    mtns[x] = (int8_t)(
      sinf(fx * 9 + seed) * 3.5f +
      sinf(fx * 22 + seed * 2.3f) * 1.5f +
      sinf(fx * 4 + seed * 0.4f) * 5.0f
    );
  }
}

// ============================================================
//  SPRITE DEL AUTO DEL JUGADOR (Vista trasera)
// ============================================================
void drawCar(int cx, int cy) {
  // Carrocería principal
  display.fillRect(cx - 4, cy, 9, 5, SSD1306_WHITE);
  // Techo y parabrisas
  display.fillRect(cx - 2, cy - 2, 5, 2, SSD1306_WHITE);
  display.fillRect(cx - 1, cy - 1, 3, 1, SSD1306_BLACK);
  // Ruedas laterales
  display.fillRect(cx - 5, cy + 1, 2, 3, SSD1306_WHITE);
  display.fillRect(cx + 4, cy + 1, 2, 3, SSD1306_WHITE);
  // Chispas de escape a alta velocidad
  if (spd > 5.0f && fCnt % 3 == 0) {
    display.drawPixel(cx - 1, cy + 5, SSD1306_WHITE);
    display.drawPixel(cx + 1, cy + 5, SSD1306_WHITE);
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
  buildTrack();
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  display.clearDisplay();

  // ── 1. LECTURA DE SENSORES CAPACITIVOS ──────────
  int t4  = touchRead(4);
  int t15 = touchRead(15);
  if (t4Base  == -1 && t4  > 10) t4Base  = t4;
  if (t15Base == -1 && t15 > 10) t15Base = t15;
  if (t4Base  < 15) t4Base  = 60;
  if (t15Base < 15) t15Base = 60;
  bool sL = (t4  < t4Base  * 0.65f);
  bool sR = (t15 < t15Base * 0.65f);

  // ── 2. ACTUALIZACIÓN DEL ESTADO DE JUEGO ────────
  // Dirección (proporcional a la velocidad)
  if (sL) pX -= 0.045f * (spd / maxSpd);
  if (sR) pX += 0.045f * (spd / maxSpd);

  // Fuerza centrífuga de la curva actual
  int curSeg = ((int)(pos / SEG_LEN)) % TRACK_LEN;
  pX += track[curSeg].curve * spd * 0.0004f;
  pX = constrain(pX, -2.5f, 2.5f);

  // Velocidad: auto-aceleración con frenado sobre pasto
  bool offRoad = (pX < -1.0f || pX > 1.0f);
  if (offRoad) {
    spd -= 0.1f;
    if (spd < 1.0f) spd = 1.0f;
  } else {
    spd += 0.06f;
    if (spd > maxSpd) spd = maxSpd;
  }
  pos += spd;
  score = (uint32_t)(pos / 80.0f);

  // Avanzar tráfico y re-generar si queda atrás
  for (int i = 0; i < N_TRAFFIC; i++) {
    traf[i].z += traf[i].spd;
    if (traf[i].z < pos - 500 || pos > traf[i].z + 300) {
      traf[i].z    = pos + (float)random(30, 70) * SEG_LEN;
      traf[i].lane = random(2) ? 0.4f : -0.4f;
    }
  }

  // ── 3. PROYECCIÓN 3D → 2D DE SEGMENTOS ─────────
  int baseSeg = (int)(pos / SEG_LEN);
  float camX  = pX * ROAD_HW;
  float camY  = CAM_H + track[baseSeg % TRACK_LEN].y;
  float camZ  = pos;

  float curveAcc = 0.0f;
  float clipY    = (float)SCR_H;  // Clip descendente para oclusión de colinas
  nProj = 0;

  for (int i = 1; i < DRAW_DIST && nProj < MAX_PROJ; i++) {
    int si   = (baseSeg + i) % TRACK_LEN;
    float dz = (float)(baseSeg + i) * SEG_LEN - camZ;
    if (dz < 20.0f) continue;

    float scale = CAM_DEPTH / dz;
    float sx = 64.0f + (curveAcc - camX) * scale;
    float sy = 22.0f + (camY - track[si].y) * scale;
    float sw = ROAD_HW * scale;

    curveAcc += track[si].curve * 10.0f;

    // Solo proyectar si está por encima del último segmento (oclusión de colinas)
    if (sy < clipY && sy >= 0.0f) {
      proj[nProj] = { sx, sy, (sw > 0.5f ? sw : 0.5f), si };
      nProj++;
      clipY = sy;
    }
  }

  // ── 4. CIELO: Montañas y Estrellas ──────────────
  int hrzY = (nProj > 0) ? (int)proj[nProj - 1].y : 22;
  hrzY = constrain(hrzY, 2, 40);

  // Silueta de montañas sólida
  for (int x = 0; x < SCR_W; x++) {
    int mTop = hrzY + mtns[x];
    mTop = constrain(mTop, 0, hrzY);
    if (mTop < hrzY)
      display.drawFastVLine(x, mTop, hrzY - mTop, SSD1306_WHITE);
  }

  // Estrellas titilantes
  for (int i = 0; i < 15; i++) {
    int sx = (i * 41 + fCnt * 2) % SCR_W;
    int sy = (i * 17 + 2) % max(1, hrzY - 4);
    if ((fCnt + i) % 5 != 0)
      display.drawPixel(sx, sy, SSD1306_WHITE);
  }

  // ── 5. RENDERIZADO DE CARRETERA (lejos → cerca) ─
  for (int i = nProj - 1; i > 0; i--) {
    float x1 = proj[i].x,   y1 = proj[i].y,   w1 = proj[i].w;
    float x2 = proj[i-1].x, y2 = proj[i-1].y, w2 = proj[i-1].w;
    int si   = proj[i].idx;
    bool stripe = ((si % 4) < 2);

    int iy1 = constrain((int)y1, 0, SCR_H - 9);
    int iy2 = constrain((int)y2, 0, SCR_H - 9);
    if (iy2 <= iy1) continue;

    for (int sy = iy1; sy <= iy2; sy++) {
      float t  = (float)(sy - iy1) / (float)(iy2 - iy1);
      float cx = x1 + (x2 - x1) * t;
      float rw = w1 + (w2 - w1) * t;

      int rL = (int)(cx - rw);
      int rR = (int)(cx + rw);

      // Bordes de la carretera (siempre visibles)
      if (rL >= 0 && rL < SCR_W) display.drawPixel(rL, sy, SSD1306_WHITE);
      if (rR >= 0 && rR < SCR_W) display.drawPixel(rR, sy, SSD1306_WHITE);

      if (stripe) {
        // Franjas de borde (rumble strips) — efecto de velocidad
        int rumW = max(1, (int)(rw * 0.12f));
        int ls = max(0, rL - rumW), le = max(0, rL);
        if (le > ls) display.drawFastHLine(ls, sy, le - ls, SSD1306_WHITE);
        int rs = min((int)SCR_W, rR + 1), re = min((int)SCR_W, rR + rumW + 1);
        if (re > rs) display.drawFastHLine(rs, sy, re - rs, SSD1306_WHITE);

        // Textura del pasto (solo donde la carretera es ancha)
        if (sy % 3 == 0 && rw > 3.0f) {
          for (int gx = ls - 5; gx >= 0; gx -= 5)
            display.drawPixel(gx, sy, SSD1306_WHITE);
          for (int gx = re + 3; gx < SCR_W; gx += 5)
            display.drawPixel(gx, sy, SSD1306_WHITE);
        }
      }

      // Línea central punteada
      if (!stripe && sy % 2 == 0) {
        int cc = (int)cx;
        if (cc >= 0 && cc < SCR_W) display.drawPixel(cc, sy, SSD1306_WHITE);
      }
    }
  }

  // ── 6. ÁRBOLES LATERALES ───────────────────────
  for (int t = 0; t < N_TREES; t++) {
    for (int p = 0; p < nProj; p++) {
      if (proj[p].idx != trees[t].seg) continue;
      float tx = proj[p].x + trees[t].side * proj[p].w * 1.5f;
      float ty = proj[p].y;
      int th   = max(2, (int)(proj[p].w * 0.2f));

      if (tx > 1 && tx < SCR_W - 1 && ty > hrzY && ty < SCR_H - 15) {
        // Tronco
        display.drawFastVLine((int)tx, (int)(ty - th), th, SSD1306_WHITE);
        // Copa triangular
        int cw = max(1, th / 2);
        display.fillTriangle(
          (int)tx,      (int)(ty - th - cw),
          (int)(tx-cw), (int)(ty - th),
          (int)(tx+cw), (int)(ty - th),
          SSD1306_WHITE
        );
      }
      break;
    }
  }

  // ── 7. VEHÍCULOS DE TRÁFICO ────────────────────
  for (int i = 0; i < N_TRAFFIC; i++) {
    float dz = traf[i].z - camZ;
    if (dz < SEG_LEN * 2 || dz > DRAW_DIST * SEG_LEN) continue;
    int tSeg = ((int)(traf[i].z / SEG_LEN)) % TRACK_LEN;

    for (int p = 0; p < nProj; p++) {
      if (proj[p].idx != tSeg) continue;
      float tx = proj[p].x + traf[i].lane * proj[p].w * 2.0f;
      float ty = proj[p].y;
      int cw   = max(2, (int)(proj[p].w * 0.16f));
      int ch   = max(1, cw * 2 / 3);

      if (tx > 0 && tx < SCR_W && ty > hrzY + 2 && ty < SCR_H - 14) {
        display.fillRect((int)(tx - cw/2), (int)(ty - ch), cw, ch, SSD1306_WHITE);
        // Luces traseras
        display.drawPixel((int)(tx - cw/2), (int)ty, SSD1306_WHITE);
        display.drawPixel((int)(tx + cw/2 - 1), (int)ty, SSD1306_WHITE);
      }

      // Detección de colisión (cercano + mismo carril)
      if (dz < SEG_LEN * 3 && fabsf(pX - traf[i].lane * 2.0f) < 0.5f) {
        spd *= 0.35f;
        traf[i].z += SEG_LEN * 5;
      }
      break;
    }
  }

  // ── 8. AUTO DEL JUGADOR + EFECTOS VISUALES ─────
  int carX = constrain(64 + (int)(pX * 10.0f), 8, SCR_W - 8);
  drawCar(carX, SCR_H - 16);

  // Partículas de polvo al salir de la pista
  if (offRoad && spd > 2.0f) {
    for (int i = 0; i < 4; i++) {
      int dx = carX + random(-8, 9);
      int dy = SCR_H - 12 + random(5);
      if (dx >= 0 && dx < SCR_W) display.drawPixel(dx, dy, SSD1306_WHITE);
    }
  }

  // Líneas de velocidad a alta velocidad
  if (spd > 6.0f) {
    for (int i = 0; i < 3; i++) {
      int lx = (fCnt * 7 + i * 43) % SCR_W;
      int ly = 28 + (fCnt * 3 + i * 19) % 20;
      int ll = 2 + (int)(spd - 6.0f);
      display.drawFastHLine(lx, ly, ll, SSD1306_WHITE);
    }
  }

  // ── 9. HUD (Barra inferior) ────────────────────
  display.fillRect(0, SCR_H - 8, SCR_W, 8, SSD1306_BLACK);
  display.drawFastHLine(0, SCR_H - 9, SCR_W, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Velocidad en km/h simulados
  display.setCursor(1, SCR_H - 7);
  display.print((int)(spd * 28.0f));
  display.print(F("km"));

  // Barra de velocímetro analógica
  int barL = (int)(spd / maxSpd * 28.0f);
  display.drawRect(30, SCR_H - 7, 30, 5, SSD1306_WHITE);
  display.fillRect(31, SCR_H - 6, barL, 3, SSD1306_WHITE);

  // Distancia recorrida (score)
  display.setCursor(64, SCR_H - 7);
  display.print(score);
  display.print(F("m"));

  // Indicador de dirección / estado
  display.setCursor(104, SCR_H - 7);
  if (sL)           display.print(F("< L"));
  else if (sR)      display.print(F("R >"));
  else if (offRoad) display.print(F(" !!"));
  else              display.print(F(" --"));

  display.display();
  fCnt++;
  delay(16);
}
