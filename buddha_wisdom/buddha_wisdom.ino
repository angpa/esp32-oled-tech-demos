// ============================================================
//  BUDDHA WISDOM (Avatamsaka Sutra - Español)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Extractos del Avatamsaka Sutra (Vol 1-3).
//  - Fade In y Fade Out mediante hardware (Contraste 0x81).
//  - Soporta múltiples líneas perfectamente centradas.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// Extractos seleccionados del Avatamsaka Sutra traducidos al español
const char* sutras[] = {
  "Si deseas comprender\na todos los Budas,\ndebes contemplar que\ntodo es creado\nsolo por la mente.",
  "La mente es como\nun pintor habil,\nconstruyendo todos\nlos reinos mundanos.",
  "En cada atomo,\nse encuentran\nincontables mundos\ny Budas enseñando.",
  "No hay diferencia\nentre la mente,\nlos seres sintientes\ny el Buda.",
  "Asi como el sol\nilumina el mundo,\nla sabiduria brilla\npara todos por igual"
};
const int totalVerses = 5;

void setContrast(uint8_t contrast) {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  setContrast(0);
  display.clearDisplay();
  display.display();
}

void loop() {
  for (int i = 0; i < totalVerses; i++) {
    
    display.clearDisplay();
    setContrast(0);
    
    String text = sutras[i];
    String lines[6];
    int lineCount = 0;
    int start = 0;
    
    // Separar el string por saltos de linea
    for(int j=0; j<text.length(); j++) {
      if(text.charAt(j) == '\n' || j == text.length()-1) {
        if(j == text.length()-1) lines[lineCount++] = text.substring(start);
        else lines[lineCount++] = text.substring(start, j);
        start = j + 1;
        if(lineCount == 6) break;
      }
    }

    // Dibujar cada linea centrada
    int totalHeight = lineCount * 12; // 8px font + 4px interlineado
    int currentY = (SCR_H - totalHeight) / 2 + 2; // +2 compensacion optica
    
    for(int j=0; j<lineCount; j++) {
      int16_t x, y; uint16_t w, h;
      display.getTextBounds(lines[j], 0, 0, &x, &y, &w, &h);
      display.setCursor((SCR_W - w) / 2, currentY);
      display.print(lines[j]);
      currentY += 12;
    }
    
    display.display();

    // Fade-In de hardware
    for (int c = 0; c <= 255; c += 2) {
      setContrast(c);
      delay(15);
    }
    setContrast(255);
    
    // Tiempo de meditacion
    delay(5000);
    
    // Fade-Out de hardware
    for (int c = 255; c >= 0; c -= 2) {
      setContrast(c);
      delay(15);
    }
    setContrast(0);
    
    display.clearDisplay();
    display.display();
    delay(1500);
  }
}
