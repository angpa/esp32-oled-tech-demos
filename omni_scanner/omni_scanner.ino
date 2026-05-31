// ============================================================
//  OMNI SCANNER (BLE + Wi-Fi)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Escanea dispositivos Bluetooth y redes Wi-Fi (incluidas
//    las ocultas) alternando el uso de la radio en el Core 0.
//  - Muestra un Radar que fusiona ambos mundos.
//  - Presenta Créditos separados para BLE y Wi-Fi con todos
//    los metadatos interceptados (MAC, Canal, Señal, etc).
// ============================================================

#include <WiFi.h>
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
//  ESTRUCTURAS DE DATOS (Compartidas)
// ============================================================
#define MAX_BLE_DEVICES 60
#define MAX_WIFI_NETWORKS 40

struct BLERecord {
  char mac[18];
  char name[32];
  int rssi;
};
BLERecord bleDevices[MAX_BLE_DEVICES];
int totalBLE = 0;

struct WiFiRecord {
  char ssid[33];
  char bssid[18];
  int rssi;
  int channel;
  int enc;
};
WiFiRecord wifiNetworks[MAX_WIFI_NETWORKS];
int totalWiFi = 0;

float radarAngle = 0;

struct Blip {
  float angle;
  float dist;
  int life;
};
Blip blips[15];
int blipIdx = 0;

portMUX_TYPE deviceMux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================
//  TAREA CORE 0: ESCANEO DUAL (BLE + Wi-Fi)
// ============================================================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      int rssi = advertisedDevice.getRSSI();
      String macStr = advertisedDevice.getAddress().toString().c_str();
      
      bool found = false;
      portENTER_CRITICAL(&deviceMux);
      for(int i = 0; i < totalBLE; i++) {
        if(strcmp(bleDevices[i].mac, macStr.c_str()) == 0) {
          bleDevices[i].rssi = rssi;
          found = true;
          break;
        }
      }
      
      if(!found && totalBLE < MAX_BLE_DEVICES) {
        strncpy(bleDevices[totalBLE].mac, macStr.c_str(), 17);
        bleDevices[totalBLE].mac[17] = '\0';
        
        if(advertisedDevice.haveName()) {
           String n = advertisedDevice.getName().c_str();
           strncpy(bleDevices[totalBLE].name, n.c_str(), 31);
           bleDevices[totalBLE].name[31] = '\0';
        } else {
           bleDevices[totalBLE].name[0] = '\0';
        }
        bleDevices[totalBLE].rssi = rssi;
        totalBLE++;
      }
      portEXIT_CRITICAL(&deviceMux);

      // Blip Radar
      float dist = map(rssi, -100, -40, 30, 2); 
      if(dist < 2) dist = 2; if(dist > 30) dist = 30;
      blips[blipIdx].angle = radarAngle;
      blips[blipIdx].dist = dist;
      blips[blipIdx].life = 100;
      blipIdx = (blipIdx + 1) % 15;
    }
};

void radioScanTask(void * parameter) {
  // Init BLE
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  
  // Init Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  while(true) {
    // 1. Escanear BLE por 2 segundos
    pBLEScan->start(2, false); 
    pBLEScan->clearResults(); 
    
    vTaskDelay(50 / portTICK_PERIOD_MS); 

    // 2. Escanear Wi-Fi (incluyendo ocultas = true)
    int n = WiFi.scanNetworks(false, true, false, 120); 
    if (n > 0) {
      portENTER_CRITICAL(&deviceMux);
      totalWiFi = 0; // Refrescar lista de Wi-Fi en cada barrido
      for (int i = 0; i < n && i < MAX_WIFI_NETWORKS; ++i) {
        String ssidStr = WiFi.SSID(i);
        if(ssidStr.length() == 0) ssidStr = "<RED OCULTA>";
        
        strncpy(wifiNetworks[totalWiFi].ssid, ssidStr.c_str(), 32);
        wifiNetworks[totalWiFi].ssid[32] = '\0';
        
        String bssidStr = WiFi.BSSIDstr(i);
        strncpy(wifiNetworks[totalWiFi].bssid, bssidStr.c_str(), 17);
        wifiNetworks[totalWiFi].bssid[17] = '\0';
        
        wifiNetworks[totalWiFi].rssi = WiFi.RSSI(i);
        wifiNetworks[totalWiFi].channel = WiFi.channel(i);
        wifiNetworks[totalWiFi].enc = WiFi.encryptionType(i);
        totalWiFi++;
        
        // Agregar un blip de Wi-Fi desfasado aleatoriamente para el radar
        float dist = map(WiFi.RSSI(i), -100, -30, 30, 2);
        if(dist < 2) dist = 2; if(dist > 30) dist = 30;
        float rAngle = radarAngle + (random(-100, 100) / 100.0);
        blips[blipIdx].angle = rAngle;
        blips[blipIdx].dist = dist;
        blips[blipIdx].life = 100;
        blipIdx = (blipIdx + 1) % 15;
      }
      portEXIT_CRITICAL(&deviceMux);
      WiFi.scanDelete(); // Liberar memoria RAM interna del Wi-Fi
    }
    
    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}

// ============================================================
//  MÁQUINA DE ESTADOS (Core 1)
// ============================================================
enum State { STATE_RADAR, STATE_CREDITS_BLE, STATE_CREDITS_WIFI };
State currentState = STATE_RADAR;
unsigned long stateStartTime = 0;
float scrollY = 64.0;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
  for(int i=0; i<15; i++) blips[i].life = 0;

  xTaskCreatePinnedToCore(
    radioScanTask, "RadioTask", 10000, NULL, 1, NULL, 0
  );
  stateStartTime = millis();
}

void loop() {
  display.clearDisplay();
  
  if (currentState == STATE_RADAR) {
    // ----------------------------------------------------
    //  FASE 1: RADAR (12 segundos)
    // ----------------------------------------------------
    display.fillRect(0, 0, SCR_W, 16, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(2, 4);
    display.print(F("OMNI SCANNER / C0"));

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, 24);
    display.print(F("WIFI: ")); display.print(totalWiFi);
    display.setCursor(2, 38);
    display.print(F("BLE:  ")); display.print(totalBLE);

    // Dibujar radar
    int cx = 94, cy = 40, r = 22;
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    display.drawCircle(cx, cy, r/2, SSD1306_WHITE);
    display.drawLine(cx-r, cy, cx+r, cy, SSD1306_WHITE);
    display.drawLine(cx, cy-r, cx, cy+r, SSD1306_WHITE);

    int lx = cx + cos(radarAngle) * r;
    int ly = cy + sin(radarAngle) * r;
    display.drawLine(cx, cy, lx, ly, SSD1306_WHITE);

    for(int i=0; i<15; i++) {
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

    // Cambiar de estado
    if (millis() - stateStartTime > 12000) {
      if (totalBLE > 0) {
        currentState = STATE_CREDITS_BLE;
        scrollY = 64.0;
      } else if (totalWiFi > 0) {
        currentState = STATE_CREDITS_WIFI;
        scrollY = 64.0;
      } else {
        stateStartTime = millis();
      }
    }
  } 
  else if (currentState == STATE_CREDITS_BLE) {
    // ----------------------------------------------------
    //  FASE 2: CRÉDITOS BLE
    // ----------------------------------------------------
    display.setTextColor(SSD1306_WHITE);
    int currentY = (int)scrollY;
    
    if (currentY > -20 && currentY < 64) {
      display.setCursor(4, currentY);
      display.print(F("--- BLUETOOTH LE ---"));
    }
    currentY += 16;
    
    for(int i = 0; i < totalBLE; i++) {
      if (currentY > -20 && currentY < 64) {
        display.setCursor(0, currentY);
        if(strlen(bleDevices[i].name) > 0) {
          char shortName[16];
          strncpy(shortName, bleDevices[i].name, 15);
          shortName[15] = '\0';
          display.print(shortName);
        } else {
          display.print(bleDevices[i].mac);
        }
        display.setCursor(104, currentY);
        display.print(bleDevices[i].rssi);
      }
      currentY += 12;
    }

    scrollY -= 0.6; 
    
    if (currentY < 0) {
      if (totalWiFi > 0) {
        currentState = STATE_CREDITS_WIFI;
        scrollY = 64.0;
      } else {
        // Limpiar
        portENTER_CRITICAL(&deviceMux);
        totalBLE = 0;
        portEXIT_CRITICAL(&deviceMux);
        currentState = STATE_RADAR;
        stateStartTime = millis();
      }
    }
  }
  else if (currentState == STATE_CREDITS_WIFI) {
    // ----------------------------------------------------
    //  FASE 3: CRÉDITOS WI-FI
    // ----------------------------------------------------
    display.setTextColor(SSD1306_WHITE);
    int currentY = (int)scrollY;
    
    if (currentY > -20 && currentY < 64) {
      display.setCursor(4, currentY);
      display.print(F("--- REDES WI-FI ---"));
    }
    currentY += 16;
    
    for(int i = 0; i < totalWiFi; i++) {
      if (currentY > -28 && currentY < 64) {
        // Línea 1: SSID o MAC si está oculto
        display.setCursor(0, currentY);
        char shortName[16];
        strncpy(shortName, wifiNetworks[i].ssid, 15);
        shortName[15] = '\0';
        display.print(shortName);
        
        display.setCursor(104, currentY);
        display.print(wifiNetworks[i].rssi);
        
        // Línea 2: Metadatos (CH y Encriptación básica / BSSID truncado)
        display.setCursor(0, currentY + 10);
        display.print(F("CH:")); display.print(wifiNetworks[i].channel);
        display.print(F(" MAC:"));
        char shortMac[9];
        strncpy(shortMac, wifiNetworks[i].bssid + 9, 8); // Mostrar ultima parte de MAC
        shortMac[8] = '\0';
        display.print(shortMac);
      }
      currentY += 24; // Ocupa doble espacio
    }

    scrollY -= 0.6; 
    
    if (currentY < 0) {
      // Limpiar memoria
      portENTER_CRITICAL(&deviceMux);
      totalBLE = 0;
      // WiFi se limpia solo en cada barrido
      portEXIT_CRITICAL(&deviceMux);
      
      currentState = STATE_RADAR;
      stateStartTime = millis();
    }
  }

  display.display();
  delay(20);
}
