// ============================================================
//  LIVE CRYPTO TICKER (Wi-Fi + HTTP)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Se conecta a tu red Wi-Fi
//  - Hace una petición HTTPS a la API pública de Binance
//  - Grafica el historial del precio de Bitcoin en tiempo real
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// --- CAMBIA ESTO POR TU RED WIFI ---
const char* ssid = "TU_WIFI_AQUI";
const char* password = "TU_PASSWORD_AQUI";
// -----------------------------------

const char* apiBTC = "https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT";

float priceHistory[SCR_W];
int historyCount = 0;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Conectando Wi-Fi..."));
  display.print(ssid);
  display.display();

  WiFi.begin(ssid, password);
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED && dots < 30) {
    delay(500);
    display.print(".");
    display.display();
    dots++;
  }

  display.clearDisplay();
  if (WiFi.status() == WL_CONNECTED) {
    display.println(F("Wi-Fi OK!"));
  } else {
    display.println(F("ERROR Wi-Fi!"));
    display.println(F("Revisa las credenciales"));
    display.println(F("en el codigo."));
  }
  display.display();
  delay(2000);

  for(int i=0; i<SCR_W; i++) priceHistory[i] = 0;
}

float getBTCPrice() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiBTC);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      // Ejemplo de payload: {"symbol":"BTCUSDT","price":"68123.45"}
      int startIdx = payload.indexOf("\"price\":\"");
      if (startIdx != -1) {
        int endIdx = payload.indexOf("\"", startIdx + 9);
        String priceStr = payload.substring(startIdx + 9, endIdx);
        http.end();
        return priceStr.toFloat();
      }
    }
    http.end();
  }
  return 0;
}

void loop() {
  // Actualizar cada 5 segundos
  if (millis() - lastUpdate > 5000 || lastUpdate == 0) {
    float currentPrice = getBTCPrice();
    lastUpdate = millis();

    if (currentPrice > 0) {
      // Desplazar historial
      if (historyCount < SCR_W) {
        priceHistory[historyCount] = currentPrice;
        historyCount++;
      } else {
        for (int i = 0; i < SCR_W - 1; i++) {
          priceHistory[i] = priceHistory[i + 1];
        }
        priceHistory[SCR_W - 1] = currentPrice;
      }
    }

    // Dibujar pantalla
    display.clearDisplay();

    // 1. HUD Superior (Precio actual)
    display.fillRect(0, 0, SCR_W, 16, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(2, 4);
    display.print(F("BTC: $"));
    display.print(currentPrice, 2);

    // 2. Gráfico Inferior
    if (historyCount > 1) {
      // Buscar min y max para escalar el gráfico
      float minP = priceHistory[0];
      float maxP = priceHistory[0];
      for (int i = 0; i < historyCount; i++) {
        if (priceHistory[i] < minP) minP = priceHistory[i];
        if (priceHistory[i] > maxP) maxP = priceHistory[i];
      }
      
      // Margen para que no toque los bordes
      if(maxP - minP < 10) { minP -= 10; maxP += 10; } // Evitar division por cero
      
      // Trazar línea
      for (int i = 0; i < historyCount - 1; i++) {
        int x0 = i;
        int y0 = map(priceHistory[i], minP, maxP, 63, 18);
        int x1 = i + 1;
        int y1 = map(priceHistory[i+1], minP, maxP, 63, 18);
        display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
      }
    }

    display.display();
  }
}
