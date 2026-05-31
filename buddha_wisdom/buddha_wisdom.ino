// ============================================================
//  BUDDHA WISDOM (Perfection of Wisdom)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Muestra extractos del Canon Tibetano (84000.co)
//  - Implementa "Fade In" y "Fade Out" alterando el voltaje
//    de contraste del chip SSD1306 por hardware.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// Extractos seleccionados de la Perfeccion de la Sabiduria (Prajnaparamita)
// del Heart Sutra (Toh 21) y el Diamond Sutra (Toh 16).
const char* sutras[] = {
  "Form is empty;\nemptiness is form.",
  "Emptiness is not\nother than form;",
  "form is not\nother than emptiness.",
  "So you should view\nthis fleeting world:",
  "A star at dawn,\na bubble in a stream,",
  "A flash of lightning\nin a summer cloud.",
  "Gate gate,\nparagate,",
  "parasamgate,\nbodhi svaha."
};
const int totalVerses = 8;

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
  setContrast(0); // Empezar con el hardware oscurecido
  display.clearDisplay();
  display.display();
}

void loop() {
  for (int i = 0; i < totalVerses; i++) {
    
    // Preparar el buffer de la pantalla en total oscuridad
    display.clearDisplay();
    setContrast(0);
    
    // Calcular el centrado del texto
    String text = sutras[i];
    int breakIdx = text.indexOf('\n');
    String line1 = text;
    String line2 = "";
    
    if (breakIdx != -1) {
      line1 = text.substring(0, breakIdx);
      line2 = text.substring(breakIdx + 1);
    }

    int16_t x1, y1; uint16_t w1, h1;
    display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    // Centrar en el eje Y dependiendo de si hay una o dos lineas
    display.setCursor((SCR_W - w1) / 2, (SCR_H / 2) - (line2.length() > 0 ? 8 : 4));
    display.print(line1);

    if (line2.length() > 0) {
      int16_t x2, y2; uint16_t w2, h2;
      display.getTextBounds(line2, 0, 0, &x2, &y2, &w2, &h2);
      display.setCursor((SCR_W - w2) / 2, (SCR_H / 2) + 4);
      display.print(line2);
    }
    
    // Enviar a la RAM (Aun no se ve nada porque el contraste es 0)
    display.display();

    // ----------------------------------------------------
    //  EFECTO: Fade-In (Aparicion meditativa por voltaje)
    // ----------------------------------------------------
    for (int c = 0; c <= 255; c += 2) {
      setContrast(c);
      delay(15); // Transicion muy suave
    }
    setContrast(255);
    
    // Tiempo de lectura
    delay(4500);
    
    // ----------------------------------------------------
    //  EFECTO: Fade-Out (Desvanecimiento en el vacio)
    // ----------------------------------------------------
    for (int c = 255; c >= 0; c -= 2) {
      setContrast(c);
      delay(15);
    }
    setContrast(0);
    
    // Limpiar RAM fisica para asegurar negro total
    display.clearDisplay();
    display.display();
    
    // Pausa en la nada absoluta
    delay(1500);
  }
}
