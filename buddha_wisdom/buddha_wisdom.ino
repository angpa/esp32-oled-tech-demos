// ============================================================
//  BUDDHA WISDOM (Avatamsaka Sutra - Expanded Edition)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - 20 Extractos del Avatamsaka Sutra.
//  - Caracteres ASCII puros para evitar errores en la fuente
//    (sin tildes ni la letra enie).
//  - Fade In y Fade Out mediante hardware (Contraste 0x81).
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// 20 Extractos del Avatamsaka Sutra sin tildes ni caracteres especiales
const char* sutras[] = {
  "Si deseas comprender\na todos los Budas,\ndebes contemplar que\ntodo es creado\nsolo por la mente.",
  "La mente es como\nun pintor habil,\nconstruyendo todos\nlos reinos mundanos.",
  "En una sola mota\nde polvo residen\nincontables mundos\ny Budas instruyendo.",
  "No hay diferencia\nentre la mente,\nlos seres sintientes\ny el Buda.",
  "Asi como el sol\nilumina el mundo,\nla sabiduria brilla\npara todos por igual.",
  "El cuerpo del Buda\nllena todo el cosmos,\napareciendo ante\ntodos los seres.",
  "El universo es\nuna red infinita\nde joyas, reflejando\nunas a las otras.",
  "La verdadera realidad\nno tiene forma,\nni nombre, ni\nprincipio ni final.",
  "El oceano del dolor\nes inmenso, pero al\ndar la vuelta esta\nla orilla de paz.",
  "Todos los fenomenos\nsurgen de las causas\ny se desvanecen\nen el gran vacio.",
  "La sabiduria del\nBuda es tan vasta\ncomo el oceano,\nsin limites.",
  "Como un espejo claro\nrefleja toda imagen,\nla mente pura refleja\ntodo el universo.",
  "La gran compasion\nes la raiz absoluta\ndel arbol de\nla iluminacion.",
  "Todo sufrimiento\nnace de la ignorancia\ny del aferramiento\nal falso yo.",
  "La naturaleza de\ntodas las cosas\nes vacia y pura\ndesde el principio.",
  "Buscar al Buda fuera\nde tu propia mente\nes como buscar peces\nen los arboles.",
  "El pasado, presente\ny futuro son solo\nuna ilusion creada\npor la mente.",
  "En un solo instante\nresiden incontables\neones de tiempo\neterno.",
  "Quien ve que todos\nlos fenomenos son\ncomo un sueño, ve\nal verdadero Buda.",
  "La luz infinita\ndispelara la\noscuridad en los\ncorazones de todos."
};
const int totalVerses = 20;

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
    
    for(int j=0; j<text.length(); j++) {
      if(text.charAt(j) == '\n' || j == text.length()-1) {
        if(j == text.length()-1) lines[lineCount++] = text.substring(start);
        else lines[lineCount++] = text.substring(start, j);
        start = j + 1;
        if(lineCount == 6) break;
      }
    }

    int totalHeight = lineCount * 12; // 8px font + 4px interlineado
    int currentY = (SCR_H - totalHeight) / 2 + 2; 
    
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
    delay(5500);
    
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
