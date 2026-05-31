#include "space_war_bg.h" // bitmap generado a partir de space_shooter1.png
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----------------------------------------------------------------
//  SISTEMAS DE ANIMACION SOBRE FONDO ESTATICO
// ----------------------------------------------------------------

// Particulas (propulsores y chispas)
#define MAX_PARTICLES 40
struct Particle {
  float x, y, vx, vy;
  int8_t life;
};
Particle particles[MAX_PARTICLES];

// Disparos láser (de la nave grande a la chica)
#define MAX_LASERS 5
struct Laser {
  float x, y, vx;
  int len;
  bool active;
};
Laser lasers[MAX_LASERS];

// Estrellas (para dar sensación de velocidad)
#define MAX_STARS 20
struct Star {
  float x, y, vx;
};
Star stars[MAX_STARS];

uint16_t frameCnt = 0;
uint16_t rng = 0xACE1;
uint16_t rnd() {
  rng ^= rng << 7;
  rng ^= rng >> 9;
  rng ^= rng << 8;
  return rng;
}

void spawnParticle(float x, float y, float vx, float vy, int8_t life) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life <= 0) {
      particles[i] = {x, y, vx, vy, life};
      break;
    }
  }
}

void spawnLaser(float x, float y, float vx, int len) {
  for (int i = 0; i < MAX_LASERS; i++) {
    if (!lasers[i].active) {
      lasers[i] = {x, y, vx, len, true};
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Fallo I2C"));
    while (1)
      ;
  }

  // Inicializar estrellas aleatorias
  for (int i = 0; i < MAX_STARS; i++) {
    stars[i].x = rnd() % 128;
    stars[i].y = 16 + (rnd() % 48);
    stars[i].vx = -0.5f - ((rnd() % 4) * 0.5f);
  }
}

void loop() {
  // ==========================================
  //  1. ACTUALIZAR LÓGICA
  // ==========================================

  // Mover estrellas (Parallax)
  for (int i = 0; i < MAX_STARS; i++) {
    stars[i].x += stars[i].vx;
    if (stars[i].x < 0) {
      stars[i].x = 128;
      stars[i].y = 16 + (rnd() % 48);
      stars[i].vx = -0.5f - ((rnd() % 4) * 0.5f);
    }
  }

  // Fuego de propulsor (nave pequeña a la izquierda, x=4, y=32 aprox)
  if (frameCnt % 2 == 0) {
    spawnParticle(4, 32 + (rnd() % 3) - 1, -((rnd() % 3) + 1),
                  ((rnd() % 3) - 1) * 0.3f, 4 + (rnd() % 4));
    spawnParticle(4, 34 + (rnd() % 3) - 1, -((rnd() % 3) + 1),
                  ((rnd() % 3) - 1) * 0.3f, 4 + (rnd() % 4));
  }

  // Nave grande dispara aleatoriamente
  if (rnd() % 25 == 0) {
    // Origen de cañones de la nave grande (derecha)
    float ly = (rnd() % 2 == 0) ? 38 : 46;
    spawnLaser(100, ly, -6.0f - (rnd() % 3), 8 + rnd() % 8);
  }

  // Mover láseres y detectar impacto
  for (int i = 0; i < MAX_LASERS; i++) {
    if (lasers[i].active) {
      lasers[i].x += lasers[i].vx;

      // Impacto contra los escudos de la nave pequeña (aprox x=28)
      if (lasers[i].x < 28) {
        lasers[i].active = false;
        // Generar explosión de chispas
        for (int p = 0; p < 10; p++) {
          float pvx = ((rnd() % 7) - 3) * 0.8f;
          float pvy = ((rnd() % 7) - 3) * 0.8f;
          spawnParticle(28, lasers[i].y, pvx, pvy, 8 + rnd() % 10);
        }
      }
    }
  }

  // Mover partículas
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;
      particles[i].life--;
    }
  }

  // ==========================================
  //  2. RENDER EN PANTALLA
  // ==========================================
  display.clearDisplay();

  // 1. Dibujar el fondo épico que ya tiene ambas naves (STATIC)
  display.drawBitmap(0, 0, bg_bitmap, 128, 64, SSD1306_WHITE);

  // 2. Dibujar Estrellas en movimiento usando INVERSE
  // Esto hace que si pasan por encima de una nave blanca, cambien de color,
  // pero en el espacio negro se verán blancas.
  for (int i = 0; i < MAX_STARS; i++) {
    display.drawPixel((int)stars[i].x, (int)stars[i].y, SSD1306_INVERSE);
    if (stars[i].vx < -1.5f) { // trail para estrellas rápidas
      display.drawPixel((int)stars[i].x + 1, (int)stars[i].y, SSD1306_INVERSE);
    }
  }

  // 3. Dibujar Láseres (disparos de la nave grande)
  for (int i = 0; i < MAX_LASERS; i++) {
    if (lasers[i].active) {
      int x = (int)lasers[i].x, y = (int)lasers[i].y;
      // Láser grueso para mayor impacto
      display.drawLine(x, y, x + lasers[i].len, y, SSD1306_WHITE);
      display.drawLine(x, y + 1, x + lasers[i].len, y + 1, SSD1306_WHITE);
    }
  }

  // 4. Dibujar Partículas (propulsores y chispas de impacto)
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      display.drawPixel((int)particles[i].x, (int)particles[i].y,
                        SSD1306_WHITE);
    }
  }

  display.display();
  delay(33);
  frameCnt++;
}
