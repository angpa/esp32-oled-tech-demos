// ============================================================
//  SYNTHWAVE 3D TERRAIN (Procedural Generation)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Generación de un terreno montañoso infinito (Wireframe).
//  - Usa un algoritmo de ruido 2D (similar a Perlin Noise) 
//    para calcular las elevaciones de forma orgánica.
//  - Aplica proyección 3D con rotación de cámara en el eje X.
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

// Parámetros de la grilla 3D
const int scl = 12;   // Escala (tamaño de cada celda)
const int cols = 18;  // Columnas
const int rows = 14;  // Filas
const int w = cols * scl;
const int h = rows * scl;

float flying = 0;
float terrain[cols][rows];

// ============================================================
//  FUNCIÓN DE RUIDO 2D (Pseudo-Perlin)
// ============================================================
float randomNoise(int x, int y) {
  int n = x + y * 57;
  n = (n << 13) ^ n;
  return (1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

// Interpolación suave para crear colinas continuas
float smoothNoise(float x, float y) {
  int int_x = (int)x;
  float frac_x = x - int_x;
  int int_y = (int)y;
  float frac_y = y - int_y;

  float v1 = randomNoise(int_x, int_y);
  float v2 = randomNoise(int_x + 1, int_y);
  float v3 = randomNoise(int_x, int_y + 1);
  float v4 = randomNoise(int_x + 1, int_y + 1);

  // Interpolación coseno para bordes redondeados
  float f_x = (1.0 - cos(frac_x * 3.14159265)) * 0.5;
  float f_y = (1.0 - cos(frac_y * 3.14159265)) * 0.5;

  float i1 = v1 * (1.0 - f_x) + v2 * f_x;
  float i2 = v3 * (1.0 - f_x) + v4 * f_x;

  return i1 * (1.0 - f_y) + i2 * f_y;
}

// ============================================================
//  PROYECCIÓN 3D -> 2D
// ============================================================
const float angleX = 1.35; // Inclinación de la cámara mirando hacia abajo
const float fov = 70.0;    // Campo de visión
const float zOffset = 90.0;// Distancia a la cámara

void project3D(int gx, int gy, float gz, int &sx, int &sy) {
  // 1. Centrar la grilla alrededor del origen
  float px = (gx * scl) - w / 2;
  float py = (gy * scl) - h / 2 + 30; // +30 para desplazar la grilla hacia adelante
  
  // 2. Rotación en el eje X (Tilt de la cámara)
  float rpy = py * cos(angleX) - gz * sin(angleX);
  float rpz = py * sin(angleX) + gz * cos(angleX) + zOffset;
  
  // Evitar división por cero
  if (rpz < 1.0) rpz = 1.0;

  // 3. Proyección a 2D
  sx = (SCR_W / 2) + (int)(px * fov / rpz);
  sy = (SCR_H / 2) - 10 + (int)(rpy * fov / rpz);
}

// ============================================================
//  PROGRAMA PRINCIPAL
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
}

void loop() {
  // Desplazar el mapa hacia adelante
  flying -= 0.15;
  float yoff = flying;
  
  // Calcular elevaciones del terreno actual
  for (int y = 0; y < rows; y++) {
    float xoff = 0;
    for (int x = 0; x < cols; x++) {
      // El mapa central es más llano (valle) y los costados más altos (montañas)
      float distToCenter = abs((float)x - (cols/2.0)) / (cols/2.0);
      float heightMult = 15.0 + (distToCenter * 25.0); // Montañas a los lados
      
      terrain[x][y] = smoothNoise(xoff, yoff) * heightMult;
      xoff += 0.25;
    }
    yoff += 0.25;
  }

  // Dibujar
  display.clearDisplay();

  // Dibujar el HUD Synthwave
  display.fillRect(0, 0, SCR_W, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print(F("TERRAIN 3D ENGINE"));

  // Dibujar la grilla wireframe (líneas horizontales)
  for (int y = 0; y < rows - 1; y++) {
    for (int x = 0; x < cols - 1; x++) {
      int sx1, sy1, sx2, sy2, sx3, sy3;
      
      // Proyectar vértice actual y los dos adyacentes (derecha y abajo)
      project3D(x, y, terrain[x][y], sx1, sy1);
      project3D(x+1, y, terrain[x+1][y], sx2, sy2);
      project3D(x, y+1, terrain[x][y+1], sx3, sy3);
      
      // Solo dibujar líneas si están dentro de límites razonables de la pantalla
      if(sy1 > 11 && sy1 < 100) {
        // Línea horizontal (de x a x+1)
        if(sx1 > -20 && sx2 < SCR_W+20) {
          display.drawLine(sx1, sy1, sx2, sy2, SSD1306_WHITE);
        }
        // Línea vertical (de y a y+1)
        if(sx1 > -20 && sx1 < SCR_W+20 && sy3 > 11) {
          display.drawLine(sx1, sy1, sx3, sy3, SSD1306_WHITE);
        }
      }
    }
  }

  // Sol virtual en el horizonte
  display.drawCircle(SCR_W/2, 22, 10, SSD1306_WHITE);
  // Efecto de atardecer (líneas cortadas en el sol)
  display.drawLine(SCR_W/2 - 12, 28, SCR_W/2 + 12, 28, SSD1306_BLACK);
  display.drawLine(SCR_W/2 - 12, 30, SCR_W/2 + 12, 30, SSD1306_BLACK);

  display.display();
}
