// ============================================================
//  CHIPTUNE SYNTH & 3D MUSIC-REACTIVE TERRAIN
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Sintetizador Chiptune de 3 canales a 16,000 Hz por Hardware (DAC Pin 25).
//  - Secuenciador integrado con pista de bajo, melodía arpegiada y percusión.
//  - Analizador de espectro FFT Cooley-Tukey en tiempo real (64 muestras).
//  - Terreno 3D reactivo a las frecuencias (Graves al centro, agudos a lados).
//  - Sol retro sunset pulsante en el horizonte al ritmo de los bajos.
//  - Cámara con rebote vertical (bobbing) físico activado por el volumen.
//  - Renderizado 3D sólido con oclusión completa (Painter's Algorithm).
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include "esp_timer.h"

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// Parámetros de la grilla 3D
const int scl = 12;   // Tamaño de celda en la grilla
const int cols = 16;  // Columnas (correspondientes a 16 bandas FFT)
const int rows = 12;  // Filas
float terrain[cols][rows];

// Configuración de proyección 3D
const float fov = 50.0;
float cam_h = 16.0;     // Altura de cámara (bobbing reactivo)
float theta = 0.22;     // Pitch hacia abajo (12.6 grados)
float cos_theta = cos(theta);
float sin_theta = sin(theta);

// Buffer circular de audio para la FFT
volatile int bufferWriteIdx = 0;
volatile float audioBuffer[64];

// ============================================================
//  MÚSICA Y SECUENCIADOR RETRO CHIPTUNE (Secuencias en PROGMEM)
// ============================================================
const uint32_t stepSamples = 1900; // Tempo (~118 BPM, 1900 muestras por paso a 16kHz)
uint32_t currentSample = 0;
int currentStep = 0;

// Notas de Melodía Arpegiada (MIDI)
const uint8_t PROGMEM melodyPattern[32] = {
  60, 64, 67, 72,  60, 64, 67, 72,  // Do Mayor arpegio
  62, 65, 69, 74,  62, 65, 69, 74,  // Re menor arpegio
  57, 60, 64, 69,  57, 60, 64, 69,  // La menor arpegio
  59, 62, 67, 71,  59, 62, 67, 71   // Sol Mayor arpegio
};

// Notas de Bajo Pulsante (MIDI)
const uint8_t PROGMEM bassPattern[32] = {
  36, 0, 36, 0,  38, 0, 38, 0,
  33, 0, 33, 0,  35, 0, 35, 0,
  36, 36, 0, 36, 38, 38, 0, 38,
  33, 33, 0, 33, 35, 35, 0, 35
};

// Percusiones (1: Bombo/Kick, 2: Redoblante/Snare, 3: Platillo/Hi-hat)
const uint8_t PROGMEM drumPattern[32] = {
  1, 0, 3, 0,  2, 0, 3, 0,
  1, 3, 0, 3,  2, 0, 3, 0,
  1, 0, 3, 0,  2, 0, 3, 0,
  1, 3, 1, 0,  2, 3, 2, 0
};

// Variables del sintetizador
volatile float freq1 = 0.0f;      // Frecuencia arpegio (lead)
volatile float freq2 = 0.0f;      // Frecuencia bajo
volatile float env1 = 0.0f;       // Envolvente lead
volatile float env2 = 0.0f;       // Envolvente bajo
volatile float envNoise = 0.0f;   // Envolvente percusión
volatile int noiseType = 0;       // Tipo de percusión activa
volatile float kickFreq = 0.0f;   // Barrido de frecuencia del bombo

float phase1 = 0.0f;
float phase2 = 0.0f;
uint16_t lfsr = 0xACE1u;          // Registro de ruido aleatorio

// Convertir MIDI a frecuencia
float noteToFreq(int note) {
  if (note == 0) return 0.0f;
  return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}

// ============================================================
//  RUTINA DE INTERRUPCIÓN (Síntesis de Audio a 16,000 Hz)
// ============================================================
void IRAM_ATTR onTimerCallback(void* arg) {
  // 1. Avanzar secuenciador
  currentSample++;
  if (currentSample >= stepSamples) {
    currentSample = 0;
    currentStep = (currentStep + 1) % 32;
    
    // Disparar nota de melodía
    uint8_t mNote = pgm_read_byte(&melodyPattern[currentStep]);
    if (mNote > 0) {
      freq1 = noteToFreq(mNote) / 16000.0f;
      env1 = 1.0f;
    }
    
    // Disparar nota de bajo
    uint8_t bNote = pgm_read_byte(&bassPattern[currentStep]);
    if (bNote > 0) {
      freq2 = noteToFreq(bNote) / 16000.0f;
      env2 = 1.0f;
    }
    
    // Disparar percusión
    uint8_t dEvent = pgm_read_byte(&drumPattern[currentStep]);
    if (dEvent == 1) { // Kick (Bombo)
      kickFreq = 150.0f / 16000.0f;
      noiseType = 3;
      envNoise = 1.0f;
    } else if (dEvent == 2) { // Snare (Redoblante)
      noiseType = 2;
      envNoise = 1.0f;
    } else if (dEvent == 3) { // Hi-hat (Platillo)
      noiseType = 1;
      envNoise = 0.5f;
    }
  }
  
  // 2. Caída de las envolventes analógicas
  env1 *= 0.9991f;      // Decaimiento medio para la melodía
  env2 *= 0.9982f;      // Decaimiento rápido para el bajo
  envNoise *= 0.9945f;  // Decaimiento súper rápido para percusión
  
  // 3. Canal 1: Onda Cuadrada (Melodía Lead)
  float out1 = 0.0f;
  if (freq1 > 0.0f && env1 > 0.01f) {
    phase1 += freq1;
    if (phase1 >= 1.0f) phase1 -= 1.0f;
    out1 = (phase1 < 0.25f) ? 1.0f : -1.0f; // Ciclo de trabajo del 25% para un tono nasal retro
    out1 *= env1 * 0.30f;
  }
  
  // 4. Canal 2: Onda Diente de Sierra (Bajos)
  float out2 = 0.0f;
  if (freq2 > 0.0f && env2 > 0.01f) {
    phase2 += freq2;
    if (phase2 >= 1.0f) phase2 -= 1.0f;
    out2 = (phase2 * 2.0f - 1.0f) * env2 * 0.38f;
  }
  
  // 5. Canal 3: Percusiones (LFSR Noise / Pitch Sweep)
  float outNoise = 0.0f;
  if (envNoise > 0.01f) {
    // Algoritmo LFSR para generar ruido blanco
    uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
    lfsr = (lfsr >> 1) | (bit << 15);
    float noiseVal = (lfsr & 1) ? 1.0f : -1.0f;
    
    if (noiseType == 1) {
      // Hi-hat: Ruido filtrado paso alto sencillo
      static float lastNoise = 0.0f;
      outNoise = (noiseVal - lastNoise) * envNoise * 0.15f;
      lastNoise = noiseVal;
    } 
    else if (noiseType == 2) {
      // Snare: Ráfaga de ruido blanco pura
      outNoise = noiseVal * envNoise * 0.28f;
    } 
    else if (noiseType == 3) {
      // Kick: Barrido de frecuencia descendente con onda senoidal
      static float kickPhase = 0.0f;
      kickPhase += kickFreq;
      if (kickPhase >= 1.0f) kickPhase -= 1.0f;
      outNoise = sin(kickPhase * 2.0f * 3.14159265f) * envNoise * 0.55f;
      kickFreq *= 0.994f; // Caída exponencial del tono
    }
  }
  
  // 6. Mezclar canales, limitar rango y escribir al DAC de 8 bits
  float mixed = out1 + out2 + outNoise;
  if (mixed < -1.0f) mixed = -1.0f;
  if (mixed > 1.0f) mixed = 1.0f;
  
  uint8_t dacSample = (uint8_t)((mixed + 1.0f) * 127.0f);
  dacWrite(25, dacSample); // Salida al pin GPIO 25
  
  // Guardar muestra normalizada en el buffer de la FFT
  audioBuffer[bufferWriteIdx] = mixed;
  bufferWriteIdx = (bufferWriteIdx + 1) % 64;
}

// ============================================================
//  ALGORITMO FFT (Cooley-Tukey Radix-2 de 64 muestras)
// ============================================================
void computeFFT(float* rex, float* imx, int n) {
  int i, j, k, l, m, le, le1, ip;
  float tr, ti, ur, ui, wr, wi, sr, si;

  // Bit Reversal
  j = n / 2;
  for (i = 1; i < n - 1; i++) {
    if (i < j) {
      tr = rex[j]; rex[j] = rex[i]; rex[i] = tr;
      ti = imx[j]; imx[j] = imx[i]; imx[i] = ti;
    }
    k = n / 2;
    while (k <= j) {
      j -= k;
      k /= 2;
    }
    j += k;
  }

  // Mariposas FFT
  m = (int)(log(n) / log(2) + 0.5);
  for (l = 1; l <= m; l++) {
    le = 1 << l;
    le1 = le / 2;
    ur = 1.0f;
    ui = 0.0f;
    float angle = -3.14159265f / le1;
    wr = cos(angle);
    wi = sin(angle);
    for (j = 0; j < le1; j++) {
      for (i = j; i < n; i += le) {
        ip = i + le1;
        tr = rex[ip] * ur - imx[ip] * ui;
        ti = rex[ip] * ui + imx[ip] * ur;
        rex[ip] = rex[i] - tr;
        imx[ip] = imx[i] - ti;
        rex[i] += tr;
        imx[i] += ti;
      }
      sr = ur * wr - ui * wi;
      si = ur * wi + ui * wr;
      ur = sr;
      ui = si;
    }
  }
}

// ============================================================
//  PROYECCIÓN DE PERSPECTIVA TERRENO 3D
// ============================================================
void project3D(float gx, float gy, float gz, int16_t &sx, int16_t &sy) {
  float px = (gx - cols / 2.0f) * scl;
  float py = gy * scl + 24.0f; // Distancia focal inicial
  
  // Rotación espacial de cámara (mirar abajo)
  float y_cam = py * cos_theta - (gz - cam_h) * sin_theta;
  float z_cam = py * sin_theta + (gz - cam_h) * cos_theta;
  
  if (y_cam < 1.0f) y_cam = 1.0f;
  
  // Proyección de perspectiva y centrado
  float temp_x = (px * fov / y_cam);
  float temp_y = -(z_cam * fov / y_cam) + 6.0f;
  
  // Clamping de seguridad
  if (temp_x < -200.0f) temp_x = -200.0f;
  if (temp_x > 200.0f) temp_x = 200.0f;
  if (temp_y < -200.0f) temp_y = -200.0f;
  if (temp_y > 200.0f) temp_y = 200.0f;
  
  sx = (SCR_W / 2) + (int16_t)temp_x;
  sy = (SCR_H / 2) + (int16_t)temp_y;
}

// Dibujar sol retro sunset
void drawRetroSun(int16_t cx, int16_t cy, int16_t r) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  // Líneas de corte horizontales
  for (int16_t dy = 2; dy < r; dy += 3) {
    int16_t half_w = sqrt(r * r - dy * dy);
    display.drawFastHLine(cx - half_w, cy + dy, half_w * 2, SSD1306_BLACK);
  }
}

// ============================================================
//  INICIALIZACIÓN SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }
  display.clearDisplay();

  // Limpiar terreno
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      terrain[x][y] = 0.0f;
    }
  }

  // Inicializar temporizador de alta frecuencia ESP_TIMER (16,000 Hz)
  const esp_timer_create_args_t timer_args = {
    .callback = &onTimerCallback,
    .name = "chiptune_timer"
  };
  esp_timer_handle_t synth_timer;
  esp_timer_create(&timer_args, &synth_timer);
  esp_timer_start_periodic(synth_timer, 62); // 62.5 microsegundos = ~16kHz
}

// ============================================================
//  LOOP PRINCIPAL (Cálculo de FFT y Renderizado)
// ============================================================
void loop() {
  // 1. Obtener una captura instantánea (snapshot) del buffer de audio de forma segura
  float rex[64];
  float imx[64];
  int readIdx = bufferWriteIdx;
  for (int i = 0; i < 64; i++) {
    rex[i] = audioBuffer[(readIdx + i) % 64];
    imx[i] = 0.0f;
  }

  // 2. Procesar Transformada Rápida de Fourier
  computeFFT(rex, imx, 64);

  // Calcular las magnitudes de las primeras 16 bandas (rango de 0 a 4,000 Hz)
  float magnitude[16];
  float totalVolume = 0.0f;
  for (int i = 0; i < 16; i++) {
    float mag = sqrt(rex[i] * rex[i] + imx[i] * imx[i]);
    
    // Boost logarítmico/escala para balancear frecuencias altas y agudas
    float boost = 1.0f + i * 0.18f;
    magnitude[i] = mag * boost;
    
    // Suma de volumen de bajos (primeras 6 bandas) para reactividad física
    if (i < 6) {
      totalVolume += mag;
    }
  }

  // 3. Reactividad física: altura de cámara (bobbing) y tamaño de sol según volumen de graves
  if (totalVolume > 4.0f) totalVolume = 4.0f;
  cam_h = 13.5f + totalVolume * 3.5f; // La cámara sube y rebota ante golpes de bajo
  int16_t sunRadius = 10 + (int16_t)(totalVolume * 3.0f); // El sol pulsa rítmicamente

  // 4. Desplazar el terreno 3D hacia adelante (Efecto Waterfall del espectro)
  for (int y = rows - 1; y > 0; y--) {
    for (int x = 0; x < cols; x++) {
      terrain[x][y] = terrain[x][y - 1];
    }
  }

  // Cargar las nuevas frecuencias calculadas en la fila del horizonte (y = 0)
  for (int x = 0; x < cols; x++) {
    // Mapear frecuencia a altura
    float val = magnitude[x] * 12.0f;
    if (val > 45.0f) val = 45.0f;
    
    // Suavizado temporal de las crestas
    terrain[x][0] = terrain[x][0] * 0.25f + val * 0.75f;
  }

  // Comenzar dibujo
  display.clearDisplay();

  // 5. Dibujar Fondo de Estrellas y Sol Retro Sunset Pulsante
  drawRetroSun(64, 25, sunRadius);
  
  // Línea del horizonte divisoria
  display.drawFastHLine(0, 27, SCR_W, SSD1306_WHITE);

  // 6. Pre-calcular las coordenadas proyectadas de todos los nodos de la malla
  int16_t sx[cols][rows];
  int16_t sy[cols][rows];
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      project3D(x, y, terrain[x][y], sx[x][y], sy[x][y]);
    }
  }

  // 7. Renderizar terreno de atrás hacia adelante (Algoritmo del Pintor)
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

      // Oclusión de caras traseras pintando el fondo de negro
      display.fillTriangle(x0, y0, x1, y1, x3, y3, SSD1306_BLACK);
      display.fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_BLACK);

      // Trazar bordes de rejilla blanca
      display.drawLine(x0, y0, x1, y1, SSD1306_WHITE); // Línea horizontal trasera
      display.drawLine(x0, y0, x3, y3, SSD1306_WHITE); // Línea longitudinal izquierda

      if (x == cols - 2) {
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE); // Cierre borde derecho
      }
      if (y == 1) {
        display.drawLine(x3, y3, x2, y2, SSD1306_WHITE); // Borde frontal más cercano
      }
    }
  }

  // 8. Dibujar barra de información decorativa superior
  display.fillRect(0, 0, SCR_W, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print(F("AUDIO SPECTRO 3D"));
  
  // Dibujar indicador animado de tempo en la esquina del banner
  if (currentStep % 4 == 0) {
    display.fillRect(SCR_W - 12, 2, 7, 7, SSD1306_BLACK);
  } else {
    display.drawRect(SCR_W - 12, 2, 7, 7, SSD1306_BLACK);
  }

  display.display();
  
  // Capping de framerate estable (~40 FPS)
  delay(25);
}
