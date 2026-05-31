// ============================================================
//  SOFT-BODY PONG (Física Deformable)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Un Pong donde la pelota es un objeto deformable (gelatina) 
//  simulado con física de Integración de Verlet.
//  Controles capacitivos por hardware:
//    GPIO 4  (Touch 0): Mover Paleta ARRIBA
//    GPIO 15 (Touch 3): Mover Paleta ABAJO
//
//  Características técnicas:
//    - Física de integración de Verlet para 4 nodos interconectados.
//    - Resortes estructurales y cruzados para volumen.
//    - Colisiones elásticas y deformación al chocar.
//    - IA simple para el oponente.
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

// ── Variables del Juego ────────────────────────────
int scoreP1 = 0;
int scoreP2 = 0;
float paddleP1_y = 24.0f;
float paddleP2_y = 24.0f;
const float PADDLE_H = 16.0f;
const float PADDLE_W = 3.0f;
const float PADDLE_SPEED = 2.5f;

// ── Física Soft-Body (Verlet) ──────────────────────
#define NUM_NODES 4
struct Node { float x, y, ox, oy; };
Node nodes[NUM_NODES];

// 6 Resortes para conectar 4 nodos (cuadrado con 2 diagonales)
struct Spring { int a, b; float length, stiffness; };
Spring springs[6];

// Velocidad global de la masa (para el pong)
float ballVx = 2.0f;
float ballVy = 1.0f;
float massX = 0, massY = 0;

void resetBall() {
  float cx = SCR_W / 2;
  float cy = SCR_H / 2;
  float r = 4.0f;
  
  nodes[0] = {cx - r, cy - r, cx - r, cy - r};
  nodes[1] = {cx + r, cy - r, cx + r, cy - r};
  nodes[2] = {cx + r, cy + r, cx + r, cy + r};
  nodes[3] = {cx - r, cy + r, cx - r, cy + r};

  // Resortes estructurales
  springs[0] = {0, 1, r*2, 0.4f};
  springs[1] = {1, 2, r*2, 0.4f};
  springs[2] = {2, 3, r*2, 0.4f};
  springs[3] = {3, 0, r*2, 0.4f};
  // Resortes cruzados (mantienen volumen)
  springs[4] = {0, 2, r*2*1.414f, 0.5f};
  springs[5] = {1, 3, r*2*1.414f, 0.5f};

  ballVx = (random(2) == 0 ? -1.0f : 1.0f) * (1.5f + random(10)*0.1f);
  ballVy = (random(2) == 0 ? -1.0f : 1.0f) * (0.5f + random(10)*0.1f);
}

void updatePhysics() {
  massX = 0; massY = 0;
  
  // 1. Integración de Verlet
  for (int i = 0; i < NUM_NODES; i++) {
    float vx = nodes[i].x - nodes[i].ox;
    float vy = nodes[i].y - nodes[i].oy;
    
    nodes[i].ox = nodes[i].x;
    nodes[i].oy = nodes[i].y;
    
    // Movimiento global sumado a la inercia interna
    nodes[i].x += vx * 0.98f + ballVx;
    nodes[i].y += vy * 0.98f + ballVy;
    
    massX += nodes[i].x;
    massY += nodes[i].y;
  }
  massX /= NUM_NODES;
  massY /= NUM_NODES;

  // 2. Resolver resortes (múltiples pasadas para rigidez)
  for (int iter = 0; iter < 3; iter++) {
    for (int i = 0; i < 6; i++) {
      Node* n1 = &nodes[springs[i].a];
      Node* n2 = &nodes[springs[i].b];
      float dx = n2->x - n1->x;
      float dy = n2->y - n1->y;
      float dist = sqrt(dx*dx + dy*dy);
      if (dist == 0) dist = 0.001f;
      
      float diff = (dist - springs[i].length) / dist;
      float ox = dx * 0.5f * diff * springs[i].stiffness;
      float oy = dy * 0.5f * diff * springs[i].stiffness;
      
      n1->x += ox; n1->y += oy;
      n2->x -= ox; n2->y -= oy;
    }

    // 3. Colisiones con paletas y paredes nodo por nodo
    for (int i = 0; i < NUM_NODES; i++) {
      // Pared Superior
      if (nodes[i].y < 0) {
         nodes[i].y = 0; 
         if (ballVy < 0) ballVy *= -1; 
      }
      // Pared Inferior
      if (nodes[i].y > SCR_H - 1) { 
         nodes[i].y = SCR_H - 1; 
         if (ballVy > 0) ballVy *= -1; 
      }
      
      // Paleta P1 (Izquierda)
      if (nodes[i].x < 10 + PADDLE_W && nodes[i].x > 10) {
        if (nodes[i].y > paddleP1_y && nodes[i].y < paddleP1_y + PADDLE_H) {
          nodes[i].x = 10 + PADDLE_W;
          if (ballVx < 0) {
            ballVx *= -1.05f; // Aumentar un poquito la velocidad global
            // Efecto de fricción de la paleta
            ballVy += (nodes[i].y - (paddleP1_y + PADDLE_H/2)) * 0.05f;
          }
        }
      }
      
      // Paleta P2 (Derecha)
      if (nodes[i].x > SCR_W - 10 - PADDLE_W && nodes[i].x < SCR_W - 10) {
        if (nodes[i].y > paddleP2_y && nodes[i].y < paddleP2_y + PADDLE_H) {
          nodes[i].x = SCR_W - 10 - PADDLE_W;
          if (ballVx > 0) {
            ballVx *= -1.05f;
            ballVy += (nodes[i].y - (paddleP2_y + PADDLE_H/2)) * 0.05f;
          }
        }
      }
    }
  }

  // 4. Chequeo de Goles
  if (massX < -10) {
    scoreP2++;
    resetBall();
  } else if (massX > SCR_W + 10) {
    scoreP1++;
    resetBall();
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
  resetBall();
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  display.clearDisplay();

  // 1. LECTURA CAPACITIVA (Jugador 1)
  int t4  = touchRead(4);
  int t15 = touchRead(15);
  if (t4Base  == -1 && t4  > 10) t4Base  = t4;
  if (t15Base == -1 && t15 > 10) t15Base = t15;
  if (t4Base  < 15) t4Base  = 60;
  if (t15Base < 15) t15Base = 60;
  
  bool btnUp   = (t4  < t4Base  * 0.65f);
  bool btnDown = (t15 < t15Base * 0.65f);

  if (btnUp)   paddleP1_y -= PADDLE_SPEED;
  if (btnDown) paddleP1_y += PADDLE_SPEED;
  
  // Limitar paleta P1
  if (paddleP1_y < 0) paddleP1_y = 0;
  if (paddleP1_y > SCR_H - PADDLE_H) paddleP1_y = SCR_H - PADDLE_H;

  // 2. IA BÁSICA (Jugador 2)
  if (massY < paddleP2_y + PADDLE_H / 2 - 2) {
    paddleP2_y -= PADDLE_SPEED * 0.7f; // IA un poco más lenta para que sea ganable
  } else if (massY > paddleP2_y + PADDLE_H / 2 + 2) {
    paddleP2_y += PADDLE_SPEED * 0.7f;
  }
  
  // Limitar paleta P2
  if (paddleP2_y < 0) paddleP2_y = 0;
  if (paddleP2_y > SCR_H - PADDLE_H) paddleP2_y = SCR_H - PADDLE_H;

  // 3. FÍSICAS DE LA PELOTA
  updatePhysics();

  // 4. RENDERIZADO
  
  // Red central punteada
  for (int i = 0; i < SCR_H; i += 4) {
    display.drawPixel(SCR_W / 2, i, SSD1306_WHITE);
  }

  // Paletas
  display.fillRect(10, (int)paddleP1_y, (int)PADDLE_W, (int)PADDLE_H, SSD1306_WHITE);
  display.fillRect(SCR_W - 10 - (int)PADDLE_W, (int)paddleP2_y, (int)PADDLE_W, (int)PADDLE_H, SSD1306_WHITE);

  // Pelota Soft-Body (Dibujar resortes estructurales)
  for (int i = 0; i < 4; i++) {
     int next = (i + 1) % 4;
     display.drawLine((int)nodes[i].x, (int)nodes[i].y, (int)nodes[next].x, (int)nodes[next].y, SSD1306_WHITE);
  }
  // Rellenar la pelota para que sea más visible
  display.fillTriangle((int)nodes[0].x, (int)nodes[0].y, (int)nodes[1].x, (int)nodes[1].y, (int)nodes[2].x, (int)nodes[2].y, SSD1306_WHITE);
  display.fillTriangle((int)nodes[0].x, (int)nodes[0].y, (int)nodes[2].x, (int)nodes[2].y, (int)nodes[3].x, (int)nodes[3].y, SSD1306_WHITE);

  // Marcador
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCR_W / 2 - 20, 2);
  display.print(scoreP1);
  display.setCursor(SCR_W / 2 + 15, 2);
  display.print(scoreP2);

  display.display();
  delay(16);
}
