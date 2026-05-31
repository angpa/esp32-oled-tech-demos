// ============================================================
//  BLE RADAR SCANNER (Dual Core)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Escanea dispositivos Bluetooth (Smartphones, Tags, etc.)
//  - Usa FreeRTOS para escanear en Core 0 sin trabar la pantalla
//  - La animación del radar corre a 60 FPS fijos en Core 1
// ============================================================

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// ============================================================
//  DATOS COMPARTIDOS ENTRE NÚCLEOS
// ============================================================
String lastDeviceName = "SCANNING...";
int lastDeviceRSSI = 0;
float radarAngle = 0;

struct Blip {
  float angle;
  float dist;
  int life;
};
Blip blips[10];
int blipIdx = 0;

// ============================================================
//  TAREA CORE 0: ESCANEO BLUETOOTH
// ============================================================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      int rssi = advertisedDevice.getRSSI();
      
      // Mapear intensidad de señal a distancia en el radar (radio máx 30)
      float dist = map(rssi, -100, -40, 30, 2); 
      if(dist < 2) dist = 2;
      if(dist > 30) dist = 30;
      
      // Generar un "blip" exactamente donde apunta la línea del radar ahora
      blips[blipIdx].angle = radarAngle;
      blips[blipIdx].dist = dist;
      blips[blipIdx].life = 100; // frames de duración
      blipIdx = (blipIdx + 1) % 10;
      
      // Guardar nombre para mostrar en pantalla
      if(advertisedDevice.haveName()) {
         lastDeviceName = advertisedDevice.getName().c_str();
      } else {
         lastDeviceName = advertisedDevice.getAddress().toString().c_str();
      }
      lastDeviceRSSI = rssi;
    }
};

void bleScanTask(void * parameter) {
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  while(true) {
    pBLEScan->start(2, false); // Escaneo de 2 segundos
    pBLEScan->clearResults(); 
    vTaskDelay(10 / portTICK_PERIOD_MS); // Pequeña pausa
  }
}

// ============================================================
//  TAREA CORE 1: ANIMACIÓN DEL RADAR (loop principal)
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
  
  // Limpiar blips
  for(int i=0; i<10; i++) blips[i].life = 0;

  // Iniciar la tarea de Bluetooth en el Núcleo 0 (PRO_CPU)
  xTaskCreatePinnedToCore(
    bleScanTask,    // Función
    "BLE_Task",     // Nombre
    10000,          // Tamaño de pila (stack)
    NULL,           // Parámetros
    1,              // Prioridad
    NULL,           // Handler
    0               // Núcleo 0
  );
  
  // El loop() normal de Arduino corre por defecto en el Núcleo 1 (APP_CPU)
}

void loop() {
  display.clearDisplay();
  
  // 1. Dibujar UI HUD
  display.fillRect(0, 0, SCR_W, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 4);
  display.print(F("BLE RADAR / CORE 0"));

  display.setTextColor(SSD1306_WHITE);
  
  // Mostrar último dispositivo detectado
  display.setCursor(2, 18);
  display.print(F("TARGET: "));
  display.print(lastDeviceName);
  
  display.setCursor(2, 28);
  display.print(F("PWR: "));
  display.print(lastDeviceRSSI);
  display.print(F(" dBm"));

  // 2. Dibujar estructura del radar (derecha de la pantalla)
  int cx = 94, cy = 40, r = 22;
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.drawCircle(cx, cy, r/2, SSD1306_WHITE);
  display.drawLine(cx-r, cy, cx+r, cy, SSD1306_WHITE);
  display.drawLine(cx, cy-r, cx, cy+r, SSD1306_WHITE);

  // 3. Línea barredora
  int lx = cx + cos(radarAngle) * r;
  int ly = cy + sin(radarAngle) * r;
  display.drawLine(cx, cy, lx, ly, SSD1306_WHITE);

  // 4. Dibujar blips (objetivos detectados)
  for(int i=0; i<10; i++) {
    if(blips[i].life > 0) {
      int bx = cx + cos(blips[i].angle) * blips[i].dist;
      int by = cy + sin(blips[i].angle) * blips[i].dist;
      
      // Parpadeo atenuante según la vida restante
      if(blips[i].life > 50 || (blips[i].life % 4 < 2)) {
        display.fillCircle(bx, by, 2, SSD1306_WHITE);
      }
      
      blips[i].life--;
    }
  }

  display.display();
  
  // Rotar barredora
  radarAngle += 0.05;
  if(radarAngle > 6.2831) radarAngle -= 6.2831;

  delay(20); // ~50 FPS
}
