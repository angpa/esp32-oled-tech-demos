// ============================================================
//  VERLET INTEGRATION PHYSICS SANDBOX
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Motor físico de Integración de Verlet rígido y soft-body.
//  - Resolvedor de restricciones (sticks) de 4 iteraciones.
//  - Colisiones elásticas y límites físicos en X: 0-127, Y: 0-53.
//  - Cuerpos físicos: Péndulo pesado, puente colgante y cubo gelatinoso.
//  - Controles capacitivos por hardware:
//    - GPIO 4 (Touch 0): Genera viento horizontal y ráfagas animadas.
//    - GPIO 15 (Touch 3): Invierte la gravedad hacia el techo.
//  - Auto-calibración adaptativa en el arranque para los sensores táctiles.
//  - HUD de diagnóstico que muestra valores analógicos crudos en tiempo real.
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

// Estructura de Partícula Verlet
struct Particle {
  float x, y;
  float px, py;
  float ax, ay;
  bool pinned;
};

// Estructura de Tensor/Restricción (Stick)
struct Constraint {
  int pA;
  int pB;
  float length;
  bool active;
};

// Límites de capacidad física
#define MAX_PARTICLES 20
#define MAX_CONSTRAINTS 25

Particle particles[MAX_PARTICLES];
Constraint constraints[MAX_CONSTRAINTS];
int pCount = 0;
int cCount = 0;

// Variables de calibración capacitiva
static int t4Base = -1;
static int t15Base = -1;

// Partículas de viento animadas
const int num_wind_streaks = 4;
float windX[num_wind_streaks] = {0, 32, 64, 96};
float windY[num_wind_streaks] = {12, 24, 38, 46};

uint16_t frameCnt = 0;

// ============================================================
//  INICIALIZACIÓN DE CUERPOS FÍSICOS
// ============================================================

void addParticle(float x, float y, bool pinned) {
  if (pCount >= MAX_PARTICLES) return;
  particles[pCount] = { x, y, x, y, 0.0f, 0.0f, pinned };
  pCount++;
}

void addConstraint(int pA, int pB, float length) {
  if (cCount >= MAX_CONSTRAINTS) return;
  constraints[cCount] = { pA, pB, length, true };
  cCount++;
}

void initPhysics() {
  pCount = 0;
  cCount = 0;

  // A. PÉNDULO PESADO (Partículas 0 a 4)
  addParticle(25.0f, 4.0f, true); // Pivote superior (Fijado)
  addParticle(25.0f, 12.0f, false);
  addParticle(25.0f, 20.0f, false);
  addParticle(25.0f, 28.0f, false);
  addParticle(25.0f, 36.0f, false); // Peso inferior
  
  addConstraint(0, 1, 8.0f);
  addConstraint(1, 2, 8.0f);
  addConstraint(2, 3, 8.0f);
  addConstraint(3, 4, 8.0f);

  // B. CUBO GELATINOSO / SOFT-BODY (Partículas 5 a 8)
  addParticle(85.0f, 10.0f, false); // Superior Izquierdo
  addParticle(101.0f, 10.0f, false); // Superior Derecho
  addParticle(101.0f, 26.0f, false); // Inferior Derecho
  addParticle(85.0f, 26.0f, false); // Inferior Izquierdo
  
  // Tensores perimetrales
  addConstraint(5, 6, 16.0f);
  addConstraint(6, 7, 16.0f);
  addConstraint(7, 8, 16.0f);
  addConstraint(8, 5, 16.0f);
  // Tensores estructurales cruzados (diagonales) para elasticidad
  addConstraint(5, 7, 22.6f);
  addConstraint(6, 8, 22.6f);

  // C. PUENTE COLANTE FLEXIBLE (Partículas 9 a 16)
  addParticle(8.0f, 42.0f, true);  // Anclaje Izquierdo (Fijado)
  addParticle(24.0f, 44.0f, false);
  addParticle(40.0f, 46.0f, false);
  addParticle(56.0f, 46.0f, false);
  addParticle(72.0f, 46.0f, false);
  addParticle(88.0f, 46.0f, false);
  addParticle(104.0f, 44.0f, false);
  addParticle(120.0f, 42.0f, true); // Anclaje Derecho (Fijado)
  
  addConstraint(9, 10, 16.0f);
  addConstraint(10, 11, 16.0f);
  addConstraint(11, 12, 16.0f);
  addConstraint(12, 13, 16.0f);
  addConstraint(13, 14, 16.0f);
  addConstraint(14, 15, 16.0f);
  addConstraint(15, 16, 16.0f);
}

// ============================================================
//  SETUP DE ARDUINO
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }
  display.clearDisplay();
  
  initPhysics();
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  display.clearDisplay();

  // 1. Leer pines capacitivos GPIO 4 (Touch 0) y GPIO 15 (Touch 3)
  int t4 = touchRead(4);
  int t15 = touchRead(15);
  
  // Calibración adaptativa dinámica en caliente
  if (t4Base == -1 && t4 > 10) t4Base = t4;
  if (t15Base == -1 && t15 > 10) t15Base = t15;
  
  // Valores mínimos de resguardo
  if (t4Base < 15) t4Base = 60;
  if (t15Base < 15) t15Base = 60;

  // Umbral: Si cae por debajo del 65% de la línea de base analógica, se considera pulsado
  bool touched4 = (t4 < t4Base * 0.65f);
  bool touched15 = (t15 < t15Base * 0.65f);

  // 2. Configurar aceleraciones de gravedad y viento
  float ax = 0.0f;
  float ay = touched15 ? -0.16f : 0.16f; // Gravedad Invertida
  
  if (touched4) {
    ax = 0.18f; // Fuerza de viento hacia la derecha
  }

  // 3. Actualizar posiciones con el paso de Verlet
  for (int i = 0; i < pCount; i++) {
    if (particles[i].pinned) continue;

    float tempX = particles[i].x;
    float tempY = particles[i].y;

    // Calcular velocidad implícita (x - px) con factor de rozamiento de aire (0.985)
    float vx = (particles[i].x - particles[i].px) * 0.985f;
    float vy = (particles[i].y - particles[i].py) * 0.985f;

    particles[i].x = particles[i].x + vx + ax;
    particles[i].y = particles[i].y + vy + ay;

    particles[i].px = tempX;
    particles[i].py = tempY;
  }

  // 4. Resolver restricciones de distancia y colisiones (Iterativo para rigidez)
  for (int iter = 0; iter < 4; iter++) {
    // Resolver tensores
    for (int i = 0; i < cCount; i++) {
      if (!constraints[i].active) continue;

      int idxA = constraints[i].pA;
      int idxB = constraints[i].pB;
      float targetL = constraints[i].length;

      float dx = particles[idxB].x - particles[idxA].x;
      float dy = particles[idxB].y - particles[idxA].y;
      float dist = sqrt(dx*dx + dy*dy);

      if (dist > 0.01f) {
        float diff = targetL - dist;
        float percent = (diff / dist) * 0.5f;
        float offsetX = dx * percent;
        float offsetY = dy * percent;

        if (!particles[idxA].pinned) {
          particles[idxA].x -= offsetX;
          particles[idxA].y -= offsetY;
        }
        if (!particles[idxB].pinned) {
          particles[idxB].x += offsetX;
          particles[idxB].y += offsetY;
        }
      }
    }

    // Resolver colisiones con bordes elásticos de rebote (Y: 0 a 53, X: 0 a 127)
    for (int i = 0; i < pCount; i++) {
      if (particles[i].pinned) continue;

      float bounce = -0.55f; // Factor de elasticidad de rebote

      // Suelo (Y: 53)
      if (particles[i].y > 53.0f) {
        particles[i].y = 53.0f;
        particles[i].py = 53.0f + (particles[i].y - particles[i].py) * bounce;
      }
      // Techo (Y: 0)
      if (particles[i].y < 0.0f) {
        particles[i].y = 0.0f;
        particles[i].py = 0.0f + (particles[i].y - particles[i].py) * bounce;
      }
      // Pared Izquierda (X: 0)
      if (particles[i].x < 0.0f) {
        particles[i].x = 0.0f;
        particles[i].px = 0.0f + (particles[i].x - particles[i].px) * bounce;
      }
      // Pared Derecha (X: 127)
      if (particles[i].x > 127.0f) {
        particles[i].x = 127.0f;
        particles[i].px = 127.0f + (particles[i].x - particles[i].px) * bounce;
      }
    }
  }

  // ==========================================
  //  5. DIBUJADO Y RENDERIZADO
  // ==========================================

  // Dibujar ráfagas de viento animadas si el sensor está activo
  if (touched4) {
    for (int i = 0; i < num_wind_streaks; i++) {
      windX[i] += 4.5f;
      if (windX[i] > 128.0f) {
        windX[i] = -20.0f;
        windY[i] = random(5, 48);
      }
      display.drawFastHLine((int16_t)windX[i], (int16_t)windY[i], 12, SSD1306_WHITE);
    }
  }

  // A. Dibujar tensores (cuerdas y enlaces rígidos)
  for (int i = 0; i < cCount; i++) {
    // Dibujar diagonales elásticas del cubo como líneas punteadas para diferenciarlas
    if (i == 8 || i == 9) {
      int16_t xA = (int16_t)particles[constraints[i].pA].x;
      int16_t yA = (int16_t)particles[constraints[i].pA].y;
      int16_t xB = (int16_t)particles[constraints[i].pB].x;
      int16_t yB = (int16_t)particles[constraints[i].pB].y;
      
      // Dibujar línea punteada
      int16_t dx = xB - xA;
      int16_t dy = yB - yA;
      float len = sqrt(dx*dx + dy*dy);
      for (float t = 0.0f; t < len; t += 3.5f) {
        int16_t px = xA + (int16_t)(dx * t / len);
        int16_t py = yA + (int16_t)(dy * t / len);
        display.drawPixel(px, py, SSD1306_WHITE);
      }
    } else {
      display.drawLine(
        (int16_t)particles[constraints[i].pA].x,
        (int16_t)particles[constraints[i].pA].y,
        (int16_t)particles[constraints[i].pB].x,
        (int16_t)particles[constraints[i].pB].y,
        SSD1306_WHITE
      );
    }
  }

  // B. Dibujar articulaciones de partículas
  for (int i = 0; i < pCount; i++) {
    // Pivotes fijos anclados
    if (particles[i].pinned) {
      display.fillRect((int16_t)particles[i].x - 2, (int16_t)particles[i].y - 2, 5, 5, SSD1306_WHITE);
    } else if (i == 4) {
      // Peso pesado del péndulo
      display.fillCircle((int16_t)particles[i].x, (int16_t)particles[i].y, 4, SSD1306_WHITE);
    } else {
      // Articulaciones normales
      display.drawPixel((int16_t)particles[i].x, (int16_t)particles[i].y, SSD1306_WHITE);
    }
  }

  // ==========================================
  //  6. BARRA DE DIAGNÓSTICO Y HUD
  // ==========================================
  
  // Línea divisoria inferior
  display.drawFastHLine(0, 54, SCR_W, SSD1306_WHITE);
  display.fillRect(0, 55, SCR_W, 9, SSD1306_BLACK);
  
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Lectura capacitiva pin 4 (T0)
  display.setCursor(2, 56);
  display.print(F("T4:"));
  display.print(t4);
  if (touched4) display.print(F(" W"));

  // Indicador de gravedad
  display.setCursor(54, 56);
  display.print(touched15 ? F(" G:UP") : F(" G:DN"));

  // Lectura capacitiva pin 15 (T3)
  display.setCursor(94, 56);
  display.print(F("T15:"));
  display.print(t15);
  if (touched15) display.print(F(" A"));

  display.display();
  frameCnt++;
  
  // Frecuencia de frames estable (~40 FPS)
  delay(25);
}
