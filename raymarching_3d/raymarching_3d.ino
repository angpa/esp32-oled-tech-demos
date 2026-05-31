// ============================================================
//  RAYMARCHING 3D METABALLS SDF SHADER
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Motor de Raymarching 3D a pantalla completa corriendo en doble núcleo.
//  - Esferas metaballs líquidas que se atraen y fusionan (Smooth Min).
//  - Optimización Bounding Sphere: Omite el raymarching en fondo negro (60%+ CPU libre).
//  - Sombreado difuso por producto punto basado en el gradiente de normales.
//  - Dithering ordenado de 1 bit para simular volumen analógico.
//  - Sincronización FreeRTOS Task Notifications sin retardo de sondeo.
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

// Handles de tareas de FreeRTOS para sincronización de doble núcleo
TaskHandle_t mainTaskHandle;
TaskHandle_t core0TaskHandle;

// Posiciones globales de las metaballs (acceso compartido)
float s1x, s1y, s1z, r1;
float s2x, s2y, s2z, r2;

// Bounding Sphere de optimización
float bcx, bcy, bcz, br;

uint16_t frameCnt = 0;

// ============================================================
//  FUNCIONES DE DISTANCIA SIGNADA (SDF) Y BLEND
// ============================================================

// Suavizado mínimo exponencial (Metaballs blend)
float smin(float a, float b, float k) {
  float h = max(k - abs(a - b), 0.0f) / k;
  return min(a, b) - h * h * h * k * 0.166667f;
}

// Función de distancia de la escena
float getDistance(float px, float py, float pz) {
  // Distancia a Esfera 1
  float dx1 = px - s1x;
  float dy1 = py - s1y;
  float dz1 = pz - s1z;
  float d1 = sqrt(dx1*dx1 + dy1*dy1 + dz1*dz1) - r1;
  
  // Distancia a Esfera 2
  float dx2 = px - s2x;
  float dy2 = py - s2y;
  float dz2 = pz - s2z;
  float d2 = sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2) - r2;
  
  // Fusión suave de ambas distancias
  return smin(d1, d2, 1.1f);
}

// ============================================================
//  RENDERIZADO POR COLUMNAS (Raymarching + Bounding Sphere)
// ============================================================
void renderHalf(int startY, int endY) {
  // Origen del rayo (cámara fija)
  float ox = 0.0f;
  float oy = 0.0f;
  float oz = -3.5f;
  
  // Pre-calcular componentes del vector al centro de la Bounding Sphere
  float L_x = ox - bcx;
  float L_y = oy - bcy;
  float L_z = oz - bcz;
  float L2 = L_x*L_x + L_y*L_y + L_z*L_z;
  float c_param = L2 - br*br;
  
  // Dirección de la luz direccional (normalizada)
  float lx = 0.577f;
  float ly = -0.577f;
  float lz = -0.577f;
  
  for (int y = startY; y < endY; y++) {
    // Escala del viewport en Y
    float ry = (y - 32.0f) / 64.0f;
    
    for (int x = 0; x < SCR_W; x++) {
      // Escala del viewport en X
      float rx = (x - 64.0f) / 64.0f;
      
      // Dirección del rayo
      float rz = 1.2f; // Longitud focal
      float len = sqrt(rx*rx + ry*ry + rz*rz);
      float dx = rx / len;
      float dy = ry / len;
      float dz = rz / len;
      
      // 1. CHEQUEO DE OPTIMIZACIÓN: Intersección Rayo-Bounding Sphere
      float b_param = 2.0f * (dx * L_x + dy * L_y + dz * L_z);
      float disc = b_param*b_param - 4.0f * c_param;
      
      if (disc < 0.0f) {
        // El rayo no toca la esfera contenedora. Dibujar fondo negro y saltar Raymarching.
        display.drawPixel(x, y, SSD1306_BLACK);
        continue;
      }
      
      // 2. BUCLE DE RAYMARCHING
      float px = ox;
      float py = oy;
      float pz = oz;
      
      bool hit = false;
      float d_travel = 0.0f;
      
      for (int step = 0; step < 16; step++) {
        float d = getDistance(px, py, pz);
        
        if (d < 0.015f) {
          hit = true;
          break;
        }
        
        d_travel += d;
        if (d_travel > 6.0f) break; // Límite lejano
        
        px += dx * d;
        py += dy * d;
        pz += dz * d;
      }
      
      // 3. ILUMINACIÓN Y SHADING (Si el rayo golpea el volumen)
      if (hit) {
        // Calcular vector normal en la superficie (Gradiente de la SDF)
        float eps = 0.02f;
        float nx = getDistance(px + eps, py, pz) - getDistance(px - eps, py, pz);
        float ny = getDistance(px, py + eps, pz) - getDistance(px, py - eps, pz);
        float nz = getDistance(px, py, pz + eps) - getDistance(px, py, pz - eps);
        
        float nLen = sqrt(nx*nx + ny*ny + nz*nz);
        if (nLen > 0.0f) {
          nx /= nLen;
          ny /= nLen;
          nz /= nLen;
        }
        
        // Intensidad de iluminación difusa (Lambertian Shading)
        float diff = nx * lx + ny * ly + nz * lz;
        if (diff < 0.0f) diff = 0.0f;
        
        // Mapeo a Dithering de 1 bit para profundidad visual
        bool draw = false;
        if (diff > 0.75f) {
          draw = true; // Sólido blanco
        } else if (diff > 0.50f) {
          draw = ((x + y) % 2 == 0); // Trama al 50%
        } else if (diff > 0.20f) {
          draw = ((x + y) % 4 == 0) && (y % 2 == 0); // Trama al 25%
        }
        
        display.drawPixel(x, y, draw ? SSD1306_WHITE : SSD1306_BLACK);
      } else {
        display.drawPixel(x, y, SSD1306_BLACK);
      }
    }
  }
}

// ============================================================
//  TAREA DE FREE RTOS (Ejecutada en el Núcleo 0)
// ============================================================
void core0_task(void * pvParameters) {
  while (true) {
    // Esperar señal del Núcleo 1
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    // Renderizar mitad superior de la pantalla (líneas 0 a 31)
    renderHalf(0, 32);
    
    // Notificar al Núcleo 1 que el cálculo de la mitad superior finalizó
    xTaskNotifyGive(mainTaskHandle);
  }
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
  
  // Capturar el handle del hilo principal (Core 1)
  mainTaskHandle = xTaskGetCurrentTaskHandle();
  
  // Crear la tarea para el Núcleo 0 (Core 0)
  xTaskCreatePinnedToCore(
    core0_task,       // Callback de la tarea
    "Core0Task",      // Nombre descriptivo
    4096,             // Tamaño de pila (stack size)
    NULL,             // Parámetro de entrada
    1,                // Prioridad
    &core0TaskHandle, // Handle de la tarea creada
    0                 // Núcleo 0
  );
}

// ============================================================
//  LOOP DE ARDUINO (Ejecutado en el Núcleo 1)
// ============================================================
void loop() {
  // 1. Actualizar posiciones de las metaballs en base al tiempo
  float t = frameCnt * 0.045f;
  
  s1x = sin(t) * 1.25f;
  s1y = cos(t * 0.7f) * 0.85f;
  s1z = sin(t * 0.35f) * 0.3f;
  r1 = 0.75f; // Radio
  
  s2x = cos(t * 1.1f) * 1.15f;
  s2y = sin(t * 0.6f) * 0.95f;
  s2z = cos(t * 0.3f) * 0.35f;
  r2 = 0.6f;  // Radio
  
  // 2. Calcular los límites de la Bounding Sphere común
  bcx = (s1x + s2x) * 0.5f;
  bcy = (s1y + s2y) * 0.5f;
  bcz = (s1z + s2z) * 0.5f;
  
  float dist = sqrt((s1x - s2x)*(s1x - s2x) + (s1y - s2y)*(s1y - s2y) + (s1z - s2z)*(s1z - s2z));
  br = dist * 0.5f + max(r1, r2) + 1.15f; // Diámetro medio + blend threshold (1.15f)

  // 3. Activar el Núcleo 0 para iniciar el cálculo de su mitad superior
  xTaskNotifyGive(core0TaskHandle);
  
  // 4. El Núcleo 1 renderiza inmediatamente la mitad inferior (líneas 32 a 63)
  renderHalf(32, 64);
  
  // 5. Bloquear y esperar a que el Núcleo 0 termine su parte
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  
  // 6. Dibujar overlay técnico de información en pantalla
  display.fillRect(0, 0, 36, 9, SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 1);
  display.print(F("SDF3D"));
  
  // Dibujar decorador oscilante de estado del buffer
  int indicatorX = 30 + (int)(sin(frameCnt * 0.2f) * 3.0f);
  display.drawPixel(indicatorX, 4, SSD1306_WHITE);

  // 7. Transferir buffer a pantalla
  display.display();
  
  frameCnt++;
}
