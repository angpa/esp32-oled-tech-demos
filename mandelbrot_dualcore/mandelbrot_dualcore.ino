// ============================================================
//  DUAL-CORE MANDELBROT FRACTAL
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Calcula un fractal de Mandelbrot haciendo zoom infinito.
//  - Usa FreeRTOS para dividir el trabajo: 
//    Core 0 procesa la mitad superior de la pantalla.
//    Core 1 procesa la mitad inferior.
//  - Rinde casi el doble de rápido que un procesador común.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// Variables de la cámara fractal
float zoom = 1.0;
float moveX = -0.5, moveY = 0.0;
float zoomDir = 1.05;

// Sincronización entre núcleos
volatile bool core0_done = true;
volatile bool start_frame = false;

// Función matemática intensiva (Fractal de Mandelbrot)
// Se puede ejecutar desde cualquier núcleo pasando los límites de Y
void calcMandelbrot(int startY, int endY) {
  int maxIter = 45; // Aumentado para mayor detalle en bandas topográficas
  for (int x = 0; x < SCR_W; x++) {
    for (int y = startY; y < endY; y++) {
      float pr = 1.5 * (x - SCR_W / 2) / (0.5 * zoom * SCR_W) + moveX;
      float pi = (y - SCR_H / 2) / (0.5 * zoom * SCR_H) + moveY;
      float newRe, newIm, oldRe, oldIm;
      newRe = newIm = oldRe = oldIm = 0;
      int i;
      for (i = 0; i < maxIter; i++) {
        oldRe = newRe;
        oldIm = newIm;
        newRe = oldRe * oldRe - oldIm * oldIm + pr;
        newIm = 2 * oldRe * oldIm + pi;
        if ((newRe * newRe + newIm * newIm) > 4) break;
      }

      // Colorear: Bandas Topográficas (Zebra Stripes)
      // En lugar de manchas sólidas, alternamos blanco y negro según la paridad de la iteración.
      // Esto genera patrones psicodélicos infinitos y evita la "pantalla sólida".
      if (i == maxIter) {
        // Dentro del set de Mandelbrot: Vacio absoluto (Negro)
      } else {
        // Fuera del set: Bandas de cebra basadas en las iteraciones de escape
        if (i % 2 == 0) {
          display.drawPixel(x, y, SSD1306_WHITE);
        }
      }
    }
  }
}

// Tarea para el Núcleo 0 (PRO_CPU)
void core0Task(void * parameter) {
  while (true) {
    // Esperar señal del Núcleo 1 para empezar a calcular
    if (start_frame && !core0_done) {
      // Core 0 calcula la mitad superior (Y: 0 a 31)
      calcMandelbrot(0, 32);
      core0_done = true; // Avisar que terminó
    }
    // Pausa mínima para no bloquear el Watchdog del RTOS
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println(F("Iniciando Dual-Core"));
  display.display();
  delay(1000);

  // Crear la tarea en el Core 0
  xTaskCreatePinnedToCore(
    core0Task,    // Función
    "Core0Task",  // Nombre
    10000,        // Stack
    NULL,         // Parámetros
    1,            // Prioridad
    NULL,         // Handle
    0             // Núcleo 0
  );
}

// El loop de Arduino corre por defecto en el Núcleo 1 (APP_CPU)
void loop() {
  display.clearDisplay();
  
  // 1. Dar la orden de inicio al Core 0
  core0_done = false;
  start_frame = true;
  
  // 2. Mientras el Core 0 trabaja arriba, el Core 1 calcula la mitad inferior
  calcMandelbrot(32, 64);
  
  // 3. Esperar a que el Core 0 termine su parte (Sincronización)
  while (!core0_done) {
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  start_frame = false; // Bajar la bandera
  
  // Dibujar info del HUD
  display.fillRect(0, 0, SCR_W, 10, SSD1306_BLACK);
  display.setCursor(2, 1);
  display.print(F("CORE0+CORE1 Z:"));
  display.print(zoom, 1);

  // 4. Mandar todo el buffer completado por ambos núcleos al OLED
  display.display();
  
  // 5. Acercar la cámara (Zoom in)
  zoom *= zoomDir;
  
  // Navegar por áreas interesantes del fractal
  if (zoom > 500.0) {
    zoomDir = 0.95; // Empezar a alejar
  } else if (zoom < 1.0) {
    zoomDir = 1.05; // Empezar a acercar
    // Cambiar objetivo aleatoriamente
    moveX = -0.5 + (random(-20, 20) / 100.0);
    moveY = 0.0 + (random(-20, 20) / 100.0);
  }
}
