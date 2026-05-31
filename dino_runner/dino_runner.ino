// ============================================================
//  SUPER DINO RUNNER
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Clon del clásico juego de Chrome con scroll parallax.
//  Controles capacitivos por hardware:
//    GPIO 4  (Touch 0): SALTAR (Esquivar cactus)
//    GPIO 15 (Touch 3): AGACHARSE (Esquivar pterodáctilos)
//
//  Características técnicas:
//    - Parallax scrolling en fondo (nubes, montañas, suelo).
//    - Generación procedural de obstáculos.
//    - Aceleración progresiva (aumenta la dificultad).
//    - Hitboxes diferenciados para estar de pie o agachado.
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

// ── Estado del Juego ───────────────────────────────
float globalSpeed = 3.0f;
float distance = 0.0f;
bool gameOver = false;
uint32_t fCnt = 0;

// ── Dinosaurio ─────────────────────────────────────
float dY = 0.0f;
float dVy = 0.0f;
bool isJumping = false;
bool isDucking = false;
const float gravity = 0.6f;
const float jumpForce = -6.5f;

// ── Obstáculos ─────────────────────────────────────
#define MAX_OBSTACLES 3
struct Obstacle {
  float x;
  int type; // 0: Cactus pequeño, 1: Cactus grande, 2: Pterodáctilo alto, 3: Pterodáctilo bajo
  bool active;
};
Obstacle obs[MAX_OBSTACLES];

// ── Parallax ───────────────────────────────────────
float cloudX = 0;
float mtnX = 0;

// ============================================================
//  BITMAPS Y DIBUJOS
// ============================================================
void drawDino(int x, int y, bool ducking, int frame) {
  if (ducking) {
    // Cuerpo agachado
    display.fillRect(x, y - 10, 24, 10, SSD1306_WHITE);
    // Cabeza
    display.fillRect(x + 20, y - 8, 12, 6, SSD1306_WHITE);
    display.drawPixel(x + 28, y - 6, SSD1306_BLACK); // Ojo
    // Piernas animadas
    if (frame % 8 < 4) {
      display.drawFastVLine(x + 4, y, 4, SSD1306_WHITE);
      display.drawFastVLine(x + 18, y, 2, SSD1306_WHITE);
    } else {
      display.drawFastVLine(x + 4, y, 2, SSD1306_WHITE);
      display.drawFastVLine(x + 18, y, 4, SSD1306_WHITE);
    }
  } else {
    // Cuerpo normal
    display.fillRect(x + 6, y - 18, 10, 14, SSD1306_WHITE); // Torso
    display.fillRect(x + 12, y - 24, 12, 10, SSD1306_WHITE); // Cabeza
    display.fillRect(x, y - 12, 8, 6, SSD1306_WHITE); // Cola
    display.drawPixel(x + 18, y - 22, SSD1306_BLACK); // Ojo
    display.fillRect(x + 18, y - 12, 6, 2, SSD1306_WHITE); // Brazo
    
    // Piernas animadas
    if (isJumping) {
       display.drawFastVLine(x + 8, y - 4, 4, SSD1306_WHITE);
       display.drawFastVLine(x + 14, y - 4, 4, SSD1306_WHITE);
    } else {
       if (frame % 8 < 4) {
         display.drawFastVLine(x + 8, y - 4, 6, SSD1306_WHITE);
         display.drawFastVLine(x + 14, y - 4, 3, SSD1306_WHITE);
       } else {
         display.drawFastVLine(x + 8, y - 4, 3, SSD1306_WHITE);
         display.drawFastVLine(x + 14, y - 4, 6, SSD1306_WHITE);
       }
    }
  }
}

void drawObstacle(Obstacle o, int frame) {
  if (o.type == 0) { // Cactus pequeño
    display.fillRect((int)o.x, 50 - 12, 6, 12, SSD1306_WHITE);
    display.fillRect((int)o.x - 4, 50 - 8, 4, 6, SSD1306_WHITE);
    display.fillRect((int)o.x + 6, 50 - 10, 4, 6, SSD1306_WHITE);
  } else if (o.type == 1) { // Cactus grande
    display.fillRect((int)o.x, 50 - 18, 8, 18, SSD1306_WHITE);
    display.fillRect((int)o.x - 6, 50 - 12, 6, 8, SSD1306_WHITE);
    display.fillRect((int)o.x + 8, 50 - 14, 6, 8, SSD1306_WHITE);
  } else { // Pterodáctilo
    int yBase = (o.type == 2) ? 20 : 35; // Alto o bajo
    if (frame % 16 < 8) {
      // Alas arriba
      display.fillRect((int)o.x, yBase, 16, 4, SSD1306_WHITE); // Cuerpo
      display.drawLine((int)o.x + 6, yBase, (int)o.x + 12, yBase - 8, SSD1306_WHITE); // Ala
      display.drawLine((int)o.x + 7, yBase, (int)o.x + 13, yBase - 8, SSD1306_WHITE); 
    } else {
      // Alas abajo
      display.fillRect((int)o.x, yBase, 16, 4, SSD1306_WHITE); // Cuerpo
      display.drawLine((int)o.x + 6, yBase + 4, (int)o.x + 12, yBase + 12, SSD1306_WHITE); // Ala
      display.drawLine((int)o.x + 7, yBase + 4, (int)o.x + 13, yBase + 12, SSD1306_WHITE); 
    }
  }
}

void spawnObstacle() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obs[i].active) {
      obs[i].active = true;
      obs[i].x = SCR_W + random(20, 80);
      
      // Decidir tipo
      int r = random(100);
      if (globalSpeed < 4.0f) {
        obs[i].type = (r < 70) ? 0 : 1; // Solo cactus al principio
      } else {
        if (r < 40) obs[i].type = 0;
        else if (r < 70) obs[i].type = 1;
        else if (r < 85) obs[i].type = 2; // Ptero alto
        else obs[i].type = 3;             // Ptero bajo (requiere agacharse)
      }
      break;
    }
  }
}

void resetGame() {
  globalSpeed = 3.5f;
  distance = 0.0f;
  dY = 0.0f; dVy = 0.0f;
  isJumping = false;
  gameOver = false;
  for (int i = 0; i < MAX_OBSTACLES; i++) obs[i].active = false;
  spawnObstacle();
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
  
  bool btnJump = (t4  < t4Base  * 0.65f);
  bool btnDuck = (t15 < t15Base * 0.65f);

  if (gameOver) {
    display.setCursor(35, 20);
    display.setTextColor(SSD1306_WHITE);
    display.print(F("GAME OVER"));
    display.setCursor(15, 40);
    display.print(F("Touch to restart"));
    display.display();
    if (btnJump || btnDuck) {
       resetGame();
       delay(200);
    }
    return;
  }

  // 2. LÓGICA DEL DINOSAURIO
  if (btnJump && !isJumping && !isDucking) {
    dVy = jumpForce;
    isJumping = true;
  }
  
  // Caída rápida si se agacha en el aire
  if (btnDuck && isJumping) {
    dVy += 1.5f; 
  }

  dY += dVy;
  if (dY < 0.0f) {
    dVy += gravity;
  } else {
    dY = 0.0f;
    dVy = 0.0f;
    isJumping = false;
  }

  isDucking = (btnDuck && !isJumping);

  // 3. ACTUALIZAR MUNDO
  distance += globalSpeed * 0.1f;
  globalSpeed += 0.001f; // Aumentar dificultad gradualmente
  if (globalSpeed > 8.0f) globalSpeed = 8.0f;

  cloudX -= globalSpeed * 0.2f;
  mtnX -= globalSpeed * 0.5f;
  if (cloudX < -64) cloudX += SCR_W + 64;
  if (mtnX < -SCR_W) mtnX += SCR_W;

  // 4. LÓGICA DE OBSTÁCULOS Y COLISIONES
  int activeCount = 0;
  bool collision = false;
  
  // Hitbox del dino
  int dx = 20, dy = 50 + (int)dY;
  int dw = isDucking ? 32 : 20;
  int dh = isDucking ? 10 : 20;
  int dHitY = isDucking ? dy - 10 : dy - 20;

  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obs[i].active) {
      obs[i].x -= globalSpeed;
      if (obs[i].x < -20) {
        obs[i].active = false;
      } else {
        activeCount++;
        // Hitbox del obstáculo
        int ox = (int)obs[i].x;
        int oy, ow, oh;
        
        if (obs[i].type == 0)      { oy = 38; ow = 10; oh = 12; }
        else if (obs[i].type == 1) { oy = 32; ow = 14; oh = 18; }
        else if (obs[i].type == 2) { oy = 20; ow = 16; oh = 8; }
        else                       { oy = 35; ow = 16; oh = 8; }

        // Colisión AABB simple
        if (dx < ox + ow && dx + dw > ox &&
            dHitY < oy + oh && dHitY + dh > oy) {
            // Hitbox más indulgente
            if (dx + 4 < ox + ow - 2 && dx + dw - 4 > ox + 2 &&
                dHitY + 4 < oy + oh - 2 && dHitY + dh - 4 > oy + 2) {
              collision = true;
            }
        }
      }
    }
  }

  if (collision) {
    gameOver = true;
  }

  // Generar nuevo obstáculo si hay pocos y están lejos
  if (activeCount < MAX_OBSTACLES) {
    bool canSpawn = true;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (obs[i].active && obs[i].x > SCR_W - 60) canSpawn = false;
    }
    if (canSpawn && random(100) < 5) {
      spawnObstacle();
    }
  }

  // 5. RENDERIZADO
  
  // Nubes Parallax
  display.fillRect((int)cloudX, 8, 12, 4, SSD1306_WHITE);
  display.fillRect((int)cloudX + 40, 15, 16, 5, SSD1306_WHITE);

  // Montañas Parallax
  for (int i = 0; i < 3; i++) {
    int mx = (int)mtnX + i * 64;
    display.drawLine(mx, 45, mx + 20, 25, SSD1306_WHITE);
    display.drawLine(mx + 20, 25, mx + 40, 45, SSD1306_WHITE);
  }

  // Línea de suelo
  display.drawFastHLine(0, 50, SCR_W, SSD1306_WHITE);
  // Puntos de suelo
  for (int i = 0; i < SCR_W; i += 12) {
    display.drawPixel((i + (int)(distance * 10)) % SCR_W, 52, SSD1306_WHITE);
    display.drawPixel((i + 5 + (int)(distance * 10)) % SCR_W, 53, SSD1306_WHITE);
  }

  // Obstáculos
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obs[i].active) drawObstacle(obs[i], fCnt);
  }

  // Dinosaurio
  drawDino(dx, 50 + (int)dY, isDucking, fCnt);

  // Score
  display.setCursor(SCR_W - 40, 0);
  display.setTextColor(SSD1306_WHITE);
  display.print((int)distance);

  display.display();
  fCnt++;
  delay(16);
}
