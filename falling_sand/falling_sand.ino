// ============================================================
//  FALLING SAND / CELLULAR AUTOMATA
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Simulación física 2D de partículas en tiempo real.
//  El "emisor" deja caer granos de arena que reaccionan a la
//  gravedad y al terreno, apilándose y resbalando en cascada.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C

Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// La grilla física: 0=vacío, 1=arena, 2=pared estática
uint8_t grid[SCR_W][SCR_H];

float emitterX = 64;
float emitterDir = 1.0f;
uint16_t frameCnt = 0;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }

  // Inicializar grilla con piso y plataformas
  for (int x = 0; x < SCR_W; x++) {
    for (int y = 0; y < SCR_H; y++) {
      grid[x][y] = 0;
      if (y == SCR_H - 1) grid[x][y] = 2; // Piso sólido
      // Bordes
      if (x == 0 || x == SCR_W - 1) grid[x][y] = 2;
    }
  }

  // Dibujar obstáculos molinetes en el medio
  for (int x = 30; x < 50; x++) grid[x][30] = 2;
  for (int x = 80; x < 100; x++) grid[x][40] = 2;
  for (int x = 50; x < 70; x++) grid[x][50] = 2;
}

void loop() {
  // Movimiento del emisor (péndulo)
  emitterX += emitterDir * 1.5f;
  if (emitterX > 110 || emitterX < 18) emitterDir *= -1;

  // Generar arena aleatoriamente
  if (frameCnt % 2 == 0) {
    for(int i=-1; i<=1; i++) {
      int ex = (int)emitterX + i;
      if (ex > 0 && ex < SCR_W-1 && grid[ex][18] == 0) {
        grid[ex][18] = 1; // Generar grano
      }
    }
  }

  // Actualizar la grilla (reglas de autómatas celulares)
  // IMPORTANTE: Recorrer de abajo hacia arriba para que la arena no caiga múltiples píxeles por frame
  for (int y = SCR_H - 2; y >= 16; y--) {
    // Alternar dirección de barrido en X evita que las dunas se inclinen siempre hacia el mismo lado
    bool leftToRight = (frameCnt % 2 == 0);
    
    for (int i = 1; i < SCR_W - 1; i++) {
      int x = leftToRight ? i : (SCR_W - 1 - i);

      if (grid[x][y] == 1) { // Si hay arena
        // Regla 1: Intentar caer recto
        if (grid[x][y + 1] == 0) {
          grid[x][y] = 0;
          grid[x][y + 1] = 1;
        } 
        // Regla 2: Intentar resbalar en diagonal izquierda
        else if (grid[x - 1][y + 1] == 0) {
          grid[x][y] = 0;
          grid[x - 1][y + 1] = 1;
        } 
        // Regla 3: Intentar resbalar en diagonal derecha
        else if (grid[x + 1][y + 1] == 0) {
          grid[x][y] = 0;
          grid[x + 1][y + 1] = 1;
        }
      }
    }
  }

  // Limpiar pantalla
  display.clearDisplay();

  // Dibujar UI base
  display.fillRect(0, 0, SCR_W, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(4, 4);
  display.print(F("PHYSICS ENGINE: SAND"));

  // Dibujar tubo emisor
  display.fillRect((int)emitterX - 4, 16, 9, 3, SSD1306_WHITE);

  // Dibujar la grilla a la pantalla OLED
  for (int x = 0; x < SCR_W; x++) {
    for (int y = 16; y < SCR_H; y++) {
      if (grid[x][y] == 1) {
        // La arena se pinta como píxeles sólidos
        display.drawPixel(x, y, SSD1306_WHITE);
      } else if (grid[x][y] == 2) {
        // Los muros se pintan con textura dither
        if ((x + y) % 2 == 0) display.drawPixel(x, y, SSD1306_WHITE);
      }
    }
  }

  display.display();
  frameCnt++;
}
