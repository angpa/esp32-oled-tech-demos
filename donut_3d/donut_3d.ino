// ============================================================
//  3D ROTATING DONUT (donut.c port)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  Un clásico de la demoscene. Renderizado 3D de un toroide
//  usando trigonometría, proyección ortográfica y cálculo de 
//  iluminación según la normal de cada cara.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C

Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

float A = 0; // Rotación Eje X
float B = 0; // Rotación Eje Z

// Parámetros del Toroide
const float R1 = 1.0f;  // Grosor del tubo
const float R2 = 2.0f;  // Radio total
const float K2 = 5.0f;  // Distancia a la cámara
const float K1 = SCR_W * K2 * 3 / (8 * (R1 + R2)); // Factor de escalado

float zbuffer[SCR_W][SCR_H]; // Movid a global para evitar Stack Overflow

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  display.clearDisplay();
  
  // Dibujar UI base
  display.fillRect(0, 0, SCR_W, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(8, 4);
  display.print(F("MATH RENDERING: 3D DONUT"));
  
  // Limpiar el Z-buffer en cada frame
  for(int x=0; x<SCR_W; x++) 
    for(int y=0; y<SCR_H; y++) 
      zbuffer[x][y] = 0;

  // Renderizado principal (basado en la matemática de Andy Sloane)
  float cosA = cos(A), sinA = sin(A);
  float cosB = cos(B), sinB = sin(B);

  // theta recorre la sección circular del tubo
  for (float theta = 0; theta < 6.28; theta += 0.2) {
    float costheta = cos(theta), sintheta = sin(theta);
    
    // phi recorre el centro de revolución del toroide
    for (float phi = 0; phi < 6.28; phi += 0.08) {
      float cosphi = cos(phi), sinphi = sin(phi);
      
      // Coordenadas x,y del círculo antes de rotar
      float circlex = R2 + R1 * costheta;
      float circley = R1 * sintheta;
      
      // Coordenada 3D final después de aplicar rotaciones A y B
      float x = circlex * (cosB * cosphi + sinA * sinB * sinphi) - circley * cosA * sinB;
      float y = circlex * (sinB * cosphi - sinA * cosB * sinphi) + circley * cosA * cosB;
      float z = K2 + cosA * circlex * sinphi + circley * sinA;
      float ooz = 1 / z; // Inverso de z
      
      // Proyección a coordenadas 2D de la pantalla
      int xp = (int) (SCR_W / 2 + K1 * ooz * x);
      int yp = (int) (SCR_H / 2 + 10 - K1 * ooz * y); // +10 offset para esquivar HUD
      
      // Cálculo de iluminación (L) midiendo el ángulo de la normal de la cara con la luz
      float L = cosphi * costheta * sinB - cosA * costheta * sinphi - sinA * sintheta + cosB * (cosA * sintheta - costheta * sinA * sinphi);
      
      // Dibujar si está dentro de la pantalla y si L > 0 (si la cara "apunta" hacia la luz)
      if (xp > 0 && xp < SCR_W && yp >= 16 && yp < SCR_H) {
        if (ooz > zbuffer[xp][yp]) {
          zbuffer[xp][yp] = ooz;
          if (L > 0) {
            // Mientras más luz (L), más denso el patrón (dithering)
            if (L > 0.8) {
              display.fillRect(xp, yp, 2, 2, SSD1306_WHITE); // Brillante
            } else if (L > 0.4) {
              display.drawPixel(xp, yp, SSD1306_WHITE); // Medio
            } else {
              if ((xp+yp)%2==0) display.drawPixel(xp, yp, SSD1306_WHITE); // Oscuro
            }
          } else {
            // Fondo / sombra (no pintar o pintar muy tenue)
          }
        }
      }
    }
  }

  display.display();
  
  A += 0.04;
  B += 0.02;
}
