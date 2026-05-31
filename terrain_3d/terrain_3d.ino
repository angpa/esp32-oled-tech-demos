// ============================================================
//  SYNTHWAVE 3D TERRAIN FLIGHT SIMULATOR (Procedural Generation)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Generación de un terreno montañoso infinito (Wireframe Sólido).
//  - Usa algoritmo de ruido 2D para elevaciones.
//  - Proyección de perspectiva 3D realista corregida.
//  - Oclusión con el Algoritmo del Pintor (ocultación de líneas traseras).
//  - Dinámicas de vuelo: Oscilación de altura (bobbing) y alabeo (roll).
//  - Elementos estéticos: Sol retro sunset, estrellas y HUD de vuelo.
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
const int scl = 12;   // Tamaño de celda en la grilla
const int cols = 16;  // Columnas
const int rows = 12;  // Filas
const int w = cols * scl;
const int h = rows * scl;

float flying = 0.0;
float terrain[cols][rows];

// Configuración de cámara y proyección
const float fov = 55.0; // Campo de visión
float cam_h = 16.0;     // Altura de la cámara (bobbing dinámico)
float roll = 0.0;       // Ángulo de ladeo lateral (banking dinámico)
float theta = 0.22;     // Ángulo de inclinación hacia abajo (pitch fijo)

// Funciones trigonométricas de rotación
float cos_theta = cos(theta);
float sin_theta = sin(theta);
float cos_roll = 1.0;
float sin_roll = 0.0;

// Estrellas fijas en el firmamento
const int num_stars = 14;
float star_x[num_stars];
float star_z[num_stars];

// ============================================================
//  FUNCIÓN DE RUIDO 2D (Pseudo-Perlin)
// ============================================================
float randomNoise(int x, int y) {
  int n = x + y * 57;
  n = (n << 13) ^ n;
  return (1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

float smoothNoise(float x, float y) {
  int int_x = (int)x;
  float frac_x = x - int_x;
  int int_y = (int)y;
  float frac_y = y - int_y;

  float v1 = randomNoise(int_x, int_y);
  float v2 = randomNoise(int_x + 1, int_y);
  float v3 = randomNoise(int_x, int_y + 1);
  float v4 = randomNoise(int_x + 1, int_y + 1);

  // Interpolación coseno
  float f_x = (1.0 - cos(frac_x * 3.14159265)) * 0.5;
  float f_y = (1.0 - cos(frac_y * 3.14159265)) * 0.5;

  float i1 = v1 * (1.0 - f_x) + v2 * f_x;
  float i2 = v3 * (1.0 - f_x) + v4 * f_x;

  return i1 * (1.0 - f_y) + i2 * f_y;
}

// ============================================================
//  PROYECCIÓN 3D -> 2D (Con Pitch y Roll)
// ============================================================
void project3D(float gx, float gy, float gz, int16_t &sx, int16_t &sy) {
  // 1. Coordenadas de mundo alineadas al centro de la cámara
  float px = (gx - cols / 2.0) * scl;
  float py = gy * scl + 24.0; // Distancia hacia adelante en el eje Y
  
  // 2. Rotación en el espacio de la cámara (inclinación hacia abajo)
  float y_cam = py * cos_theta - (gz - cam_h) * sin_theta;
  float z_cam = py * sin_theta + (gz - cam_h) * cos_theta;
  
  if (y_cam < 1.0) y_cam = 1.0; // Evitar división por cero / clipping trasero
  
  // 3. Proyección de perspectiva
  float proj_x = (px * fov / y_cam);
  float proj_y = -(z_cam * fov / y_cam) + 4.0; // Desplazamiento del centro
  
  // Seguridad anti-desborde para el rasterizador
  if (proj_x < -500.0) proj_x = -500.0;
  if (proj_x > 500.0) proj_x = 500.0;
  if (proj_y < -500.0) proj_y = -500.0;
  if (proj_y > 500.0) proj_y = 500.0;
  
  // 4. Rotación por el ángulo de ladeo (Roll)
  if (roll != 0.0) {
    float rx = proj_x * cos_roll - proj_y * sin_roll;
    float ry = proj_x * sin_roll + proj_y * cos_roll;
    proj_x = rx;
    proj_y = ry;
  }
  
  sx = (SCR_W / 2) + (int16_t)proj_x;
  sy = (SCR_H / 2) + (int16_t)proj_y;
}

// Proyección optimizada para estrellas y sol en el infinito
void projectInfinite(float sx_val, float sz_val, int16_t &sx, int16_t &sy) {
  float py = 350.0; // Distancia fija de proyección de horizonte
  float y_cam = py * cos_theta - (sz_val - cam_h) * sin_theta;
  float z_cam = py * sin_theta + (sz_val - cam_h) * cos_theta;
  
  if (y_cam < 1.0) y_cam = 1.0;
  
  float proj_x = (sx_val * fov / y_cam);
  float proj_y = -(z_cam * fov / y_cam) + 4.0;
  
  if (roll != 0.0) {
    float rx = proj_x * cos_roll - proj_y * sin_roll;
    float ry = proj_x * sin_roll + proj_y * cos_roll;
    proj_x = rx;
    proj_y = ry;
  }
  
  sx = (SCR_W / 2) + (int16_t)proj_x;
  sy = (SCR_H / 2) + (int16_t)proj_y;
}

// ============================================================
//  DIBUJO DE SOL RETRO CON LÍNEAS DE CORTE
// ============================================================
void drawRetroSun(int16_t cx, int16_t cy, int16_t r) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  
  // Genera las franjas de horizonte en la parte inferior del sol
  for (int16_t dy = 2; dy < r; dy += 3) {
    int16_t half_w = sqrt(r * r - dy * dy);
    
    // Rotar los extremos de la franja negra
    int16_t rx1 = cx + (int16_t)(-half_w * cos_roll - dy * sin_roll);
    int16_t ry1 = cy + (int16_t)(-half_w * sin_roll + dy * cos_roll);
    int16_t rx2 = cx + (int16_t)(half_w * cos_roll - dy * sin_roll);
    int16_t ry2 = cy + (int16_t)(half_w * sin_roll + dy * cos_roll);
    
    display.drawLine(rx1, ry1, rx2, ry2, SSD1306_BLACK);
  }
}

// ============================================================
//  SETUP Y INICIALIZACIÓN
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { 
    while (1); 
  }
  
  // Inicialización de estrellas aleatorias
  for (int i = 0; i < num_stars; i++) {
    star_x[i] = random(-140, 140);
    star_z[i] = random(18, 55); // Posicionadas en el plano del cielo
  }
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  // Velocidad de vuelo hacia adelante
  flying += 0.055;
  float frac_flying = flying - (int)flying;
  
  // Actualizar parámetros de movimiento de la cámara
  cam_h = 14.5 + sin(flying * 0.35) * 2.2;  // Turbulencia / oscilación vertical
  roll = sin(flying * 0.14) * 0.08;         // Ladeo lateral suave
  
  // Actualizar matrices trigonométricas
  cos_roll = cos(roll);
  sin_roll = sin(roll);
  
  // 1. Generar alturas del terreno con desvío en el medio para mantener un valle limpio
  for (int y = 0; y < rows; y++) {
    float y_noise = ((int)flying + y) * 0.16 + 1000.0;
    for (int x = 0; x < cols; x++) {
      float x_noise = x * 0.20 + 1000.0;
      
      // Proporción de distancia al centro (0 en el medio, 1 en los extremos)
      float distToCenter = abs((float)x - (cols / 2.0)) / (cols / 2.0);
      
      // La altura se incrementa exponencialmente en los extremos
      float heightMult = 8.0 + (distToCenter * distToCenter * 44.0);
      
      float noiseVal = smoothNoise(x_noise, y_noise);
      
      // Añadir colinas extra marcadas en los lados
      if (distToCenter > 0.45) {
        noiseVal += (distToCenter - 0.45) * 0.65;
      }
      
      terrain[x][y] = noiseVal * heightMult;
    }
  }

  // Limpiar pantalla y comenzar render
  display.clearDisplay();

  // 2. Dibujar cielo: estrellas rotando con el alabeo
  for (int i = 0; i < num_stars; i++) {
    int16_t sx, sy;
    projectInfinite(star_x[i], star_z[i], sx, sy);
    if (sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H) {
      display.drawPixel(sx, sy, SSD1306_WHITE);
    }
  }

  // 3. Dibujar Sol Retro en el centro del horizonte
  int16_t sun_x, sun_y;
  projectInfinite(0.0, 44.0, sun_x, sun_y);
  drawRetroSun(sun_x, sun_y, 13);

  // 4. Pre-calcular y almacenar todas las proyecciones de vértices
  int16_t sx[cols][rows];
  int16_t sy[cols][rows];
  
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      project3D(x, y - frac_flying, terrain[x][y], sx[x][y], sy[x][y]);
    }
  }

  // 5. Dibujar el terreno de atrás hacia adelante (Algoritmo del Pintor)
  for (int y = rows - 1; y > 0; y--) {
    for (int x = 0; x < cols - 1; x++) {
      int16_t x0 = sx[x][y];
      int16_t y0 = sy[x][y];
      int16_t x1 = sx[x+1][y];
      int16_t y1 = sy[x+1][y];
      int16_t x2 = sx[x+1][y-1];
      int16_t y2 = sy[x+1][y-1];
      int16_t x3 = sx[x][y-1];
      int16_t y3 = sy[x][y-1];
      
      // Rellenar la celda actual con negro para ocultar el fondo
      display.fillTriangle(x0, y0, x1, y1, x3, y3, SSD1306_BLACK);
      display.fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_BLACK);
      
      // Dibujar las líneas del wireframe
      display.drawLine(x0, y0, x1, y1, SSD1306_WHITE); // Línea horizontal (fondo)
      display.drawLine(x0, y0, x3, y3, SSD1306_WHITE); // Línea vertical (izquierda)
      
      // Si es el borde derecho de la grilla, dibujar el cierre derecho
      if (x == cols - 2) {
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
      }
      // Si es el frente más cercano, dibujar la línea del fondo
      if (y == 1) {
        display.drawLine(x3, y3, x2, y2, SSD1306_WHITE);
      }
    }
  }

  // 6. Dibujar HUD de Vuelo táctico en primer plano
  int16_t chx = SCR_W / 2;
  int16_t chy = SCR_H / 2;
  
  // Retícula central
  display.drawPixel(chx, chy, SSD1306_WHITE);
  display.drawFastHLine(chx - 8, chy, 4, SSD1306_WHITE);
  display.drawFastHLine(chx + 4, chy, 4, SSD1306_WHITE);
  display.drawFastVLine(chx, chy - 4, 2, SSD1306_WHITE);
  display.drawFastVLine(chx, chy + 2, 2, SSD1306_WHITE);
  
  // Corchetes de guiado lateral
  display.drawFastVLine(chx - 22, chy - 5, 11, SSD1306_WHITE);
  display.drawFastHLine(chx - 22, chy - 5, 4, SSD1306_WHITE);
  display.drawFastHLine(chx - 22, chy + 5, 4, SSD1306_WHITE);
  
  display.drawFastVLine(chx + 22, chy - 5, 11, SSD1306_WHITE);
  display.drawFastHLine(chx + 18, chy - 5, 4, SSD1306_WHITE);
  display.drawFastHLine(chx + 18, chy + 5, 4, SSD1306_WHITE);
  
  // Indicador de alabeo en la parte superior
  display.drawFastHLine(SCR_W/2 - 15, 0, 31, SSD1306_WHITE);
  int16_t tick_x = SCR_W/2 + (int16_t)(roll * 110.0);
  if (tick_x < SCR_W/2 - 15) tick_x = SCR_W/2 - 15;
  if (tick_x > SCR_W/2 + 15) tick_x = SCR_W/2 + 15;
  display.drawFastVLine(tick_x, 1, 3, SSD1306_WHITE);

  // Telemetría
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  // Altímetro (ALT)
  display.setCursor(SCR_W - 55, 2);
  display.print(F("ALT:"));
  int16_t alt_val = (int16_t)(cam_h * 50.0);
  display.print(alt_val);
  display.print(F("m"));
  
  // Velocímetro (SPD)
  display.setCursor(4, 2);
  display.print(F("SPD:180KT"));

  // Enviar buffer a la pantalla
  display.display();
  
  // Controlar fotogramas por segundo (~40 FPS)
  delay(25);
}
