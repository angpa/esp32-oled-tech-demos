// ============================================================
//  CLASSIC PONG
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  El Pong original, rápido, nítido y claro.
//  Controles capacitivos por hardware:
//    GPIO 4  (Touch 0): Mover Paleta ARRIBA
//    GPIO 15 (Touch 3): Mover Paleta ABAJO
//
//  Características técnicas:
//    - Física simple de rebotes inelásticos contra la paleta.
//    - Aceleración progresiva de la pelota tras cada rebote.
//    - IA simple pero reactiva para el oponente.
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
const float PADDLE_SPEED = 3.0f;

// Pelota
float ballX = 64.0f;
float ballY = 32.0f;
float ballVx = 2.0f;
float ballVy = 1.0f;
const float BALL_SIZE = 3.0f; // Cuadrado de 3x3

void resetBall() {
  ballX = SCR_W / 2;
  ballY = SCR_H / 2;
  // Dirección aleatoria
  ballVx = (random(2) == 0 ? -1.0f : 1.0f) * 1.5f;
  ballVy = (random(2) == 0 ? -1.0f : 1.0f) * (0.5f + random(10) * 0.1f);
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
  if (ballY < paddleP2_y + PADDLE_H / 2 - 4) {
    paddleP2_y -= PADDLE_SPEED * 0.65f; // Velocidad de la IA
  } else if (ballY > paddleP2_y + PADDLE_H / 2 + 4) {
    paddleP2_y += PADDLE_SPEED * 0.65f;
  }
  
  // Limitar paleta P2
  if (paddleP2_y < 0) paddleP2_y = 0;
  if (paddleP2_y > SCR_H - PADDLE_H) paddleP2_y = SCR_H - PADDLE_H;

  // 3. FÍSICAS DE LA PELOTA
  ballX += ballVx;
  ballY += ballVy;

  // Rebote superior e inferior
  if (ballY < 0) {
    ballY = 0;
    ballVy = -ballVy;
  } else if (ballY > SCR_H - BALL_SIZE) {
    ballY = SCR_H - BALL_SIZE;
    ballVy = -ballVy;
  }

  // Rebote Paleta P1 (Izquierda)
  if (ballX < 10 + PADDLE_W && ballX + BALL_SIZE > 10) {
    if (ballY + BALL_SIZE > paddleP1_y && ballY < paddleP1_y + PADDLE_H) {
      ballX = 10 + PADDLE_W;
      ballVx = -ballVx * 1.05f; // Aumentar velocidad al golpear
      // Efecto inglés (spin)
      float hitPoint = (ballY + BALL_SIZE/2) - (paddleP1_y + PADDLE_H/2);
      ballVy += hitPoint * 0.1f;
    }
  }

  // Rebote Paleta P2 (Derecha)
  if (ballX + BALL_SIZE > SCR_W - 10 - PADDLE_W && ballX < SCR_W - 10) {
    if (ballY + BALL_SIZE > paddleP2_y && ballY < paddleP2_y + PADDLE_H) {
      ballX = SCR_W - 10 - PADDLE_W - BALL_SIZE;
      ballVx = -ballVx * 1.05f;
      float hitPoint = (ballY + BALL_SIZE/2) - (paddleP2_y + PADDLE_H/2);
      ballVy += hitPoint * 0.1f;
    }
  }

  // Límite de velocidad
  if (ballVx > 5.0f) ballVx = 5.0f;
  if (ballVx < -5.0f) ballVx = -5.0f;

  // 4. CHEQUEO DE GOLES
  if (ballX < 0) {
    scoreP2++;
    resetBall();
    delay(500); // Pausa dramática
  } else if (ballX > SCR_W) {
    scoreP1++;
    resetBall();
    delay(500);
  }

  // 5. RENDERIZADO
  
  // Red central punteada
  for (int i = 0; i < SCR_H; i += 6) {
    display.drawFastVLine(SCR_W / 2, i, 3, SSD1306_WHITE);
  }

  // Paletas
  display.fillRect(10, (int)paddleP1_y, (int)PADDLE_W, (int)PADDLE_H, SSD1306_WHITE);
  display.fillRect(SCR_W - 10 - (int)PADDLE_W, (int)paddleP2_y, (int)PADDLE_W, (int)PADDLE_H, SSD1306_WHITE);

  // Pelota
  display.fillRect((int)ballX, (int)ballY, (int)BALL_SIZE, (int)BALL_SIZE, SSD1306_WHITE);

  // Marcador
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  // Score P1
  if (scoreP1 < 10) display.setCursor(SCR_W / 2 - 25, 5);
  else display.setCursor(SCR_W / 2 - 35, 5);
  display.print(scoreP1);
  
  // Score P2
  display.setCursor(SCR_W / 2 + 15, 5);
  display.print(scoreP2);

  display.display();
  delay(16);
}
