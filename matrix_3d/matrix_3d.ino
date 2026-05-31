// ============================================================
//  3D PERSPECTIVE MATRIX TUNNEL
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Simula caracteres de código moviéndose en un cilindro 3D
//  hacia la cámara, dando efecto de viajar por la Matrix.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C

Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

#define NUM_RUNES 60

struct Rune {
  float x, y, z;
  char c;
  int speed;
};

Rune runes[NUM_RUNES];
uint16_t frameCnt = 0;

void resetRune(int i, bool farZone) {
  // Posición circular en 2D
  float angle = random(0, 628) / 100.0f;
  float radius = random(30, 45); // Tubo de radio 30 a 45
  
  runes[i].x = cos(angle) * radius;
  runes[i].y = sin(angle) * radius;
  
  if (farZone) {
    runes[i].z = random(150, 400);
  } else {
    runes[i].z = random(20, 400); // Dispersión inicial
  }
  
  // Símbolos estilo matrix (0, 1, y letras random)
  int type = random(0, 10);
  if (type < 4) runes[i].c = '0';
  else if (type < 8) runes[i].c = '1';
  else runes[i].c = (char)random(65, 90); // A-Z
  
  runes[i].speed = random(2, 6);
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }

  for (int i = 0; i < NUM_RUNES; i++) {
    resetRune(i, false);
  }
}

void loop() {
  display.clearDisplay();

  // Rotación global de la cámara para efecto de barril
  float camRoll = frameCnt * 0.02f;
  float cosRoll = cos(camRoll);
  float sinRoll = sin(camRoll);

  // Interfaz tipo "Terminal"
  display.fillRect(0, 0, SCR_W, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(4, 4);
  display.print(F("UPLINK: SECURE TUNNEL"));
  
  display.setTextColor(SSD1306_WHITE);

  for (int i = 0; i < NUM_RUNES; i++) {
    // Mover hacia la cámara (eje Z decreciente)
    runes[i].z -= runes[i].speed;
    
    if (runes[i].z <= 1.0f) {
      resetRune(i, true); // Relanzar al fondo
    }

    // Rotación de cámara
    float rx = runes[i].x * cosRoll - runes[i].y * sinRoll;
    float ry = runes[i].x * sinRoll + runes[i].y * cosRoll;

    // Proyección de 3D a 2D de pantalla (Perspectiva matemática: px = x/z)
    int px = (int)(SCR_W / 2 + (rx * 100.0f) / runes[i].z);
    int py = (int)(SCR_H / 2 + 8 + (ry * 100.0f) / runes[i].z); // +8 offset por HUD

    // Solo dibujar si cae dentro de la pantalla visible
    if (px > -10 && px < SCR_W && py > 14 && py < SCR_H) {
      // Modificar tamaño de texto basado en profundidad Z
      if (runes[i].z < 40.0f) {
        display.setTextSize(2);
      } else {
        display.setTextSize(1);
      }
      
      // Cambio ocasional de caracter (glitch)
      if (random(0, 100) < 2) {
        runes[i].c = random(0, 2) ? '0' : '1';
      }

      display.setCursor(px, py);
      
      // Atenuación de distancia (si está muy lejos, pintar oscuro, simulado por líneas en oled)
      // En un OLED bicolor es difícil, así que lo mostramos normal, pero si está muy lejos omitimos el texto
      if (runes[i].z > 300.0f) {
        display.drawPixel(px, py, SSD1306_WHITE);
      } else {
        display.print(runes[i].c);
      }
    }
  }

  // Centro convergente falso (efecto hiperespacio remoto)
  display.drawPixel(SCR_W/2, SCR_H/2 + 8, SSD1306_WHITE);

  display.display();
  frameCnt++;
}
