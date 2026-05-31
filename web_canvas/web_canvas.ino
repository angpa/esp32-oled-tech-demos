// ============================================================
//  REAL-TIME WEB CANVAS (Wi-Fi Access Point)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Crea su propia red Wi-Fi "ESP32_OLED_CANVAS"
//  - Aloja una página web interactiva
//  - Dibuja en tu celular y los píxeles se envían por
//    paquetes HTTP POST al ESP32 para renderizado en vivo.
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

const char* ssid = "ESP32_OLED_CANVAS";
WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0">
  <title>OLED Canvas</title>
  <style>
    body { background-color: #111; color: #0f0; text-align: center; font-family: monospace; margin: 0; padding: 20px; }
    canvas { background-color: black; border: 2px solid #0f0; touch-action: none; width: 100%; max-width: 384px; image-rendering: pixelated; box-shadow: 0 0 15px #0f0; }
    button { background: #0f0; color: #000; border: none; padding: 10px 20px; font-size: 18px; font-family: monospace; font-weight: bold; cursor: pointer; margin-top: 20px; }
  </style>
</head>
<body>
  <h2>HACKER CANVAS</h2>
  <p>Dibuja aquí y mira el OLED</p>
  <canvas id="c" width="128" height="64"></canvas><br>
  <button onclick="clearOLED()">BORRAR PANTALLA</button>

  <script>
    var c = document.getElementById('c');
    var ctx = c.getContext('2d');
    var drawing = false;
    ctx.fillStyle = "white";
    
    let queue = [];
    
    function sendPixel(x, y) {
      queue.push(x + "," + y);
      ctx.fillRect(x, y, 1, 1);
    }
    
    function handle(e) {
      e.preventDefault();
      var rect = c.getBoundingClientRect();
      var touch = e.touches ? e.touches[0] : e;
      var x = Math.floor((touch.clientX - rect.left) / (rect.width / 128));
      var y = Math.floor((touch.clientY - rect.top) / (rect.height / 64));
      sendPixel(x, y);
    }
    
    c.onmousedown = function(e){ drawing = true; handle(e); };
    c.onmousemove = function(e){ if(drawing) handle(e); };
    c.onmouseup = function(){ drawing = false; };
    c.ontouchstart = function(e){ drawing = true; handle(e); };
    c.ontouchmove = function(e){ if(drawing) handle(e); };
    c.ontouchend = function(){ drawing = false; };
    
    function clearOLED() {
      ctx.clearRect(0, 0, 128, 64);
      fetch('/clear');
    }

    // Enviar lote de píxeles cada 50ms para no saturar el servidor
    setInterval(() => {
      if(queue.length > 0) {
        let data = queue.join(";");
        queue = [];
        fetch('/d', { method: 'POST', body: data });
      }
    }, 50);
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { while (1); }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Iniciando Wi-Fi..."));
  display.display();

  // Configurar ESP32 como Punto de Acceso (Access Point)
  WiFi.softAP(ssid);
  IPAddress IP = WiFi.softAPIP();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("WIFI AP CREADO!"));
  display.println(F("================="));
  display.print(F("RED: ")); display.println(ssid);
  display.print(F("IP:  ")); display.println(IP);
  display.println(F("================="));
  display.println(F("Conectate con tu cel"));
  display.println(F("y abri la IP en el"));
  display.println(F("navegador."));
  display.display();

  // Ruta principal: Enviar el HTML
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlPage);
  });

  // Ruta para borrar la pantalla
  server.on("/clear", HTTP_GET, []() {
    display.clearDisplay();
    display.display();
    server.send(200, "text/plain", "CLEARED");
  });

  // Ruta para recibir puntos: "x,y;x,y;x,y"
  server.on("/d", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String data = server.arg("plain");
      
      // Parsear la cadena de puntos
      int start = 0;
      int end = data.indexOf(';');
      while (end != -1) {
        String pair = data.substring(start, end);
        int comma = pair.indexOf(',');
        if(comma != -1) {
          int x = pair.substring(0, comma).toInt();
          int y = pair.substring(comma+1).toInt();
          display.drawPixel(x, y, SSD1306_WHITE);
        }
        start = end + 1;
        end = data.indexOf(';', start);
      }
      // Último par
      String pair = data.substring(start);
      int comma = pair.indexOf(',');
      if(comma != -1) {
        int x = pair.substring(0, comma).toInt();
        int y = pair.substring(comma+1).toInt();
        display.drawPixel(x, y, SSD1306_WHITE);
      }
      
      display.display();
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  server.handleClient();
}
