// ============================================================
//  BLE RADAR SCANNER v2 (Cinematic Credits)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Escanea dispositivos Bluetooth Low Energy en el Core 0.
//  - Almacena dispositivos únicos sin duplicados.
//  - Alterna entre una interfaz de Radar (10s) y una pantalla
//    de créditos rodantes que muestra todo lo detectado.
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
#define MAX_DEVICES 60

struct DeviceRecord {
  char mac[18];
  char name[32];
  int rssi;
};
DeviceRecord foundDevices[MAX_DEVICES];
int totalDevices = 0;

float radarAngle = 0;

struct Blip {
  float angle;
  float dist;
  int life;
};
Blip blips[10];
int blipIdx = 0;

// Mutex para operaciones seguras entre núcleos
portMUX_TYPE deviceMux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================
//  TAREA CORE 0: ESCANEO BLUETOOTH
// ============================================================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      int rssi = advertisedDevice.getRSSI();
      String macStr = advertisedDevice.getAddress().toString().c_str();
      
      // Chequear si el dispositivo ya existe (para evitar duplicados)
      bool found = false;
      portENTER_CRITICAL(&deviceMux);
      for(int i = 0; i < totalDevices; i++) {
        if(strcmp(foundDevices[i].mac, macStr.c_str()) == 0) {
          foundDevices[i].rssi = rssi; // Actualizar potencia de señal
          found = true;
          break;
        }
      }
      
      // Si es nuevo y hay espacio en la memoria
      if(!found && totalDevices < MAX_DEVICES) {
        strncpy(foundDevices[totalDevices].mac, macStr.c_str(), 17);
        foundDevices[totalDevices].mac[17] = '\0';
        
        if(advertisedDevice.haveName()) {
           String n = advertisedDevice.getName().c_str();
           strncpy(foundDevices[totalDevices].name, n.c_str(), 31);
           foundDevices[totalDevices].name[31] = '\0';
        } else {
           foundDevices[totalDevices].name[0] = '\0'; // Sin nombre
        }
        foundDevices[totalDevices].rssi = rssi;
        totalDevices++;
      }
      portEXIT_CRITICAL(&deviceMux);

      // Efecto del Blip visual en el radar
      float dist = map(rssi, -100, -40, 30, 2); 
      if(dist < 2) dist = 2;
      if(dist > 30) dist = 30;
      
      blips[blipIdx].angle = radarAngle;
      blips[blipIdx].dist = dist;
      blips[blipIdx].life = 100;
      blipIdx = (blipIdx + 1) % 10;
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
    pBLEScan->start(2, false); 
    pBLEScan->clearResults(); 
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

// ============================================================
//  MÁQUINA DE ESTADOS (Core 1)
// ============================================================
enum State { STATE_RADAR, STATE_CREDITS };
State currentState = STATE_RADAR;
unsigned long stateStartTime = 0;
float scrollY = 64.0;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
  
  for(int i=0; i<10; i++) blips[i].life = 0;

  xTaskCreatePinnedToCore(
    bleScanTask, "BLE_Task", 10000, NULL, 1, NULL, 0
  );
  
  stateStartTime = millis();
}

void loop() {
  display.clearDisplay();
  
  if (currentState == STATE_RADAR) {
    // ----------------------------------------------------
    //  FASE 1: RADAR ACTIVO (10 segundos)
    // ----------------------------------------------------
    display.fillRect(0, 0, SCR_W, 16, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(2, 4);
    display.print(F("BLE RADAR / CORE 0"));

    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(2, 28);
    display.print(F("OBJETIVOS:"));
    display.setCursor(2, 40);
    display.setTextSize(2);
    display.print(totalDevices);
    display.setTextSize(1);

    // Dibujar radar
    int cx = 94, cy = 40, r = 22;
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    display.drawCircle(cx, cy, r/2, SSD1306_WHITE);
    display.drawLine(cx-r, cy, cx+r, cy, SSD1306_WHITE);
    display.drawLine(cx, cy-r, cx, cy+r, SSD1306_WHITE);

    // Línea barredora
    int lx = cx + cos(radarAngle) * r;
    int ly = cy + sin(radarAngle) * r;
    display.drawLine(cx, cy, lx, ly, SSD1306_WHITE);

    // Dibujar blips
    for(int i=0; i<10; i++) {
      if(blips[i].life > 0) {
        int bx = cx + cos(blips[i].angle) * blips[i].dist;
        int by = cy + sin(blips[i].angle) * blips[i].dist;
        if(blips[i].life > 50 || (blips[i].life % 4 < 2)) {
          display.fillCircle(bx, by, 2, SSD1306_WHITE);
        }
        blips[i].life--;
      }
    }

    radarAngle += 0.05;
    if(radarAngle > 6.2831) radarAngle -= 6.2831;

    // Cambiar de estado después de 10 segundos
    if (millis() - stateStartTime > 10000) {
      if (totalDevices > 0) {
        currentState = STATE_CREDITS;
        scrollY = 64.0; // Iniciar créditos desde el fondo
      } else {
        stateStartTime = millis(); // Resetear temporizador si no hay nada
      }
    }
  } 
  else if (currentState == STATE_CREDITS) {
    // ----------------------------------------------------
    //  FASE 2: CRÉDITOS RODANTES (Lista de dispositivos)
    // ----------------------------------------------------
    display.setTextColor(SSD1306_WHITE);
    int currentY = (int)scrollY;
    
    // Título de la lista
    if (currentY > -20 && currentY < 64) {
      display.setCursor(8, currentY);
      display.print(F("--- DETECTADOS: "));
      display.print(totalDevices);
      display.print(F(" ---"));
    }
    currentY += 16;
    
    // Dibujar elementos de la lista
    for(int i = 0; i < totalDevices; i++) {
      if (currentY > -20 && currentY < 64) { // Solo dibujar si es visible
        display.setCursor(0, currentY);
        
        // Mostrar nombre si existe, sino MAC
        if(strlen(foundDevices[i].name) > 0) {
          // Truncar nombre si es muy largo
          char shortName[16];
          strncpy(shortName, foundDevices[i].name, 15);
          shortName[15] = '\0';
          display.print(shortName);
        } else {
          display.print(foundDevices[i].mac);
        }
        
        // Mostrar RSSI alineado a la derecha
        display.setCursor(104, currentY);
        display.print(foundDevices[i].rssi);
      }
      currentY += 12; // Espaciado entre líneas
    }

    // Scroll vertical
    scrollY -= 0.6; // Velocidad de los créditos
    
    // Si el último elemento desapareció por arriba, reiniciar ciclo
    if (currentY < 0) {
      // Limpiar memoria para no acumular basura de gente que ya se fue
      portENTER_CRITICAL(&deviceMux);
      totalDevices = 0;
      portEXIT_CRITICAL(&deviceMux);
      
      currentState = STATE_RADAR;
      stateStartTime = millis();
    }
  }

  display.display();
  delay(20);
}
