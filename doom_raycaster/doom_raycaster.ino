// ============================================================
//  LABYRINTH EXPLORER 3D (Expert Graphics Version)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Motor de Raycasting 3D con perspectiva ampliada (54px).
//  - Rejilla 3D Holodeck en Suelo y Techo: Líneas horizontales de
//    avance y radiales de inclinación que rotan con la cámara.
//  - Oclusión total: Muros sólidos se dibujan encima de la rejilla.
//  - Texturizado HD procedimental con efectos avanzados:
//    - Ladrillos con rugosidad de piedra (dithering interno).
//    - Ventana con reflejo de cristal diagonal (vidrio reflectivo).
//    - Columnas con veteado de madera y puertas con remaches metálicos.
//  - Autopiloto predictivo inteligente que detecta esquinas.
//  - Telemetría inferior con osciloscopio animado en tiempo real.
//  - Radar mini-mapa con vector de mira/dirección del jugador.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C

Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// ============================================================
//  WORLD MAP (1: Bricks, 2: Window, 3: Columns, 4: Door)
// ============================================================
#define MAP_W 12
#define MAP_H 12

const uint8_t PROGMEM worldMap[MAP_W][MAP_H] = {
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,3,0,0,0,0,0,0,1},
  {1,0,2,0,3,0,2,2,0,2,0,1},
  {1,0,2,0,0,0,0,0,0,2,0,1},
  {1,0,4,4,4,4,0,3,0,2,0,1},
  {1,0,0,0,0,4,0,3,0,0,0,1},
  {1,3,3,3,0,4,0,3,3,3,0,1},
  {1,0,0,0,0,0,0,0,0,3,0,1},
  {1,0,2,2,2,2,2,2,0,4,0,1},
  {1,0,0,0,0,0,0,2,0,4,0,1},
  {1,2,2,2,2,2,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1}
};

// ============================================================
//  CAMERA & PLAYER STATE
// ============================================================
float posX = 1.5f, posY = 1.5f;
float dirX = 1.0f, dirY = 0.0f;
float planeX = 0.0f, planeY = 0.8f; // FOV
uint16_t frameCnt = 0;

// Configuración del firmamento estrellado
const int num_stars = 20;
float star_x[num_stars];
float star_y[num_stars];

// ============================================================
//  TEXTURIZADO PROCEDIMENTAL DE PAREDES (Alta Fidelidad)
// ============================================================
bool getWallPixel(int wallType, int texX, int texY, int side) {
  // Sombreado direccional para el eje Y (muros oscurecidos con tramado)
  if (side == 1) {
    if (texY % 2 == 0 && texX % 2 == 0) return false;
  }
  
  if (wallType == 1) {
    // 1. TEXTURA DE LADRILLO CON RUGOSIDAD
    if (texY == 0 || texY == 8) return true; // Juntas horizontales
    if (texY < 8) {
      if (texX == 0 || texX == 8) return true; // Juntas verticales fila superior
    } else {
      if (texX == 4 || texX == 12) return true; // Juntas verticales fila inferior
    }
    // Rugosidad de la piedra (tramado interno)
    if ((texX + texY) % 4 == 0) return true;
    return false;
  } 
  else if (wallType == 2) {
    // 2. MARCO DE VENTANA Y REFLEJO DIAGONAL
    if (texX == 0 || texX == 15 || texY == 0 || texY == 15) return true; // Marco exterior
    if (texX == 8 || texY == 8) return true; // Cruceta central
    // Reflejo diagonal de cristal punteado
    if (texX == texY && (texX % 3 == 0)) return true;
    return false;
  } 
  else if (wallType == 3) {
    // 3. COLUMNAS DE MADERA CON VETEADO
    if (texY == 0 || texY == 15) return true;
    if (texX == 0 || texX == 4 || texX == 11 || texX == 15) return true; // Ranuras de soporte
    // Vetado horizontal de madera
    if (texY % 3 == 0 && (texX % 2 == 0)) return true;
    return false;
  }
  else if (wallType == 4) {
    // 4. PUERTA SCI-FI CON REMACHES Y LECTOR
    if (texX <= 1 || texX >= 14 || texY <= 1 || texY >= 14) {
      // Remaches/tornillos de esquina
      if ((texX == 1 || texX == 14) && (texY % 4 == 0)) return true;
      return true; // Marco grueso
    }
    if (texX == 7 || texX == 8) return true; // Junta central de apertura
    if ((texX == 11 || texX == 12) && texY == 9) return true; // Picaporte
    // Lector de tarjetas de acceso (izquierda)
    if (texX == 4 && (texY == 5 || texY == 6)) return true;
    return false;
  }
  return true;
}

// ============================================================
//  SETUP PRINCIPAL
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }
  
  // Inicialización del cielo estrellado
  for (int i = 0; i < num_stars; i++) {
    star_x[i] = random(0, 128);
    star_y[i] = random(2, 24);
  }
  display.clearDisplay();
}

// ============================================================
//  LOOP DE RENDERIZADO Y LÓGICA
// ============================================================
void loop() {
  display.clearDisplay();

  // Ángulo de la cámara y desplazamiento del cielo
  float playerAngle = atan2(dirY, dirX);
  int skyOffset = (int)((playerAngle / (2.0f * 3.14159265f)) * 128.0f);

  // 1. Dibujar Cielo Estrellado Giratorio
  for (int i = 0; i < num_stars; i++) {
    int sx = (int)(star_x[i]) + skyOffset;
    sx = (sx % SCR_W + SCR_W) % SCR_W; // Wrap horizontal
    int sy = (int)(star_y[i]);
    
    if (sy >= 0 && sy < 27) {
      display.drawPixel(sx, sy, SSD1306_WHITE);
    }
  }

  // 2. Dibujar línea del horizonte punteada (visible a través de ventanas)
  for (int x = 0; x < SCR_W; x += 2) {
    display.drawPixel(x, 27, SSD1306_WHITE);
  }

  // ==========================================
  //  REJILLA 3D HOLODECK (Suelo y Techo Vectorial)
  // ==========================================
  // A. Líneas Horizontales de Scroll (Avanzan al caminar)
  float travel_dist = posX * dirX + posY * dirY;
  float frac_travel = travel_dist - floor(travel_dist);
  if (frac_travel < 0.0f) frac_travel += 1.0f;
  
  for (int j = 1; j <= 5; j++) {
    float d = j - frac_travel;
    if (d < 0.1f) d = 0.1f;
    
    int dy = (int)(27.0f / d); // Distancia proyectada desde el horizonte (27)
    
    int y_floor = 27 + dy;
    int y_ceil = 27 - dy;
    
    // Dibujar líneas horizontales punteadas
    if (y_floor < 54) {
      for (int x = 0; x < SCR_W; x += 4) {
        display.drawPixel(x, y_floor, SSD1306_WHITE);
      }
    }
    if (y_ceil >= 0) {
      for (int x = 0; x < SCR_W; x += 4) {
        display.drawPixel(x, y_ceil, SSD1306_WHITE);
      }
    }
  }

  // B. Líneas Radiales de Perspectiva (Fugantes giratorias)
  int spacing = 24;
  int radialOffset = skyOffset % spacing;
  
  for (int x_edge = -spacing; x_edge < SCR_W + spacing; x_edge += spacing) {
    int px_bottom = x_edge - radialOffset;
    int px_top = x_edge - radialOffset;
    
    // Trazar radial del suelo (punteada)
    for (int y = 28; y < 54; y += 2) {
      float factor = (y - 27) / 26.0f;
      int lx = SCR_W / 2 + (int)(factor * (px_bottom - SCR_W / 2));
      if (lx >= 0 && lx < SCR_W) {
        display.drawPixel(lx, y, SSD1306_WHITE);
      }
    }
    // Trazar radial del techo (punteada)
    for (int y = 0; y < 27; y += 2) {
      float factor = (27 - y) / 27.0f;
      int lx = SCR_W / 2 + (int)(factor * (px_top - SCR_W / 2));
      if (lx >= 0 && lx < SCR_W) {
        display.drawPixel(lx, y, SSD1306_WHITE);
      }
    }
  }

  // ==========================================
  //  3. MOTOR RAYCASTING 3D CON OCLUSIÓN
  // ==========================================
  for (int x = 0; x < SCR_W; x++) {
    float cameraX = 2.0f * x / (float)SCR_W - 1.0f; 
    float rayDirX = dirX + planeX * cameraX;
    float rayDirY = dirY + planeY * cameraX;

    int mapX = int(posX);
    int mapY = int(posY);

    float sideDistX, sideDistY;
    float deltaDistX = (rayDirX == 0) ? 1e30 : abs(1.0f / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30 : abs(1.0f / rayDirY);
    float perpWallDist;

    int stepX, stepY;
    int hit = 0;
    int side;

    if (rayDirX < 0) { stepX = -1; sideDistX = (posX - mapX) * deltaDistX; } 
    else             { stepX =  1; sideDistX = (mapX + 1.0f - posX) * deltaDistX; }
    
    if (rayDirY < 0) { stepY = -1; sideDistY = (posY - mapY) * deltaDistY; } 
    else             { stepY =  1; sideDistY = (mapY + 1.0f - posY) * deltaDistY; }

    // Algoritmo DDA
    while (hit == 0) {
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = 0;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = 1;
      }
      
      if (mapX >= 0 && mapX < MAP_W && mapY >= 0 && mapY < MAP_H) {
        if (pgm_read_byte(&worldMap[mapX][mapY]) > 0) hit = 1;
      } else {
        hit = 1; 
      }
    }

    if (side == 0) perpWallDist = (sideDistX - deltaDistX);
    else           perpWallDist = (sideDistY - deltaDistY);

    if (perpWallDist < 0.1f) perpWallDist = 0.1f;

    // Viewport de 3D es de 54px (Y: 0 a 53)
    int h = 54;
    int lineHeight = (int)(h / perpWallDist);

    int drawStart = -lineHeight / 2 + h / 2;
    if (drawStart < 0) drawStart = 0;

    int drawEnd = lineHeight / 2 + h / 2;
    if (drawEnd >= h) drawEnd = h - 1;

    // Mapeo de textura procedimental
    int wallType = pgm_read_byte(&worldMap[mapX][mapY]);
    
    float wallHitX;
    if (side == 0) wallHitX = posY + perpWallDist * rayDirY;
    else           wallHitX = posX + perpWallDist * rayDirX;
    wallHitX -= floor(wallHitX);

    int texX = (int)(wallHitX * 16.0f);
    if (side == 0 && rayDirX > 0) texX = 15 - texX;
    if (side == 1 && rayDirY < 0) texX = 15 - texX;

    // Dibujar rebanada de pared
    for (int y = drawStart; y <= drawEnd; y++) {
      float wallHitY = (float)(y - drawStart) / (drawEnd - drawStart + 1);
      int texY = (int)(wallHitY * 16.0f);
      if (texY < 0) texY = 0;
      if (texY > 15) texY = 15;

      // Determinación de transparencia de la ventana (para ver estrellas y rejilla)
      bool isWindowPane = (wallType == 2) && 
                          !(texX == 0 || texX == 15 || texY == 0 || texY == 15 || 
                            texX == 8 || texY == 8 || (texX == texY && (texX % 3 == 0)));

      bool isWallPixel = false;
      
      if (!isWindowPane) {
        if (perpWallDist > 6.0f) {
          isWallPixel = ((x + y) % 4 == 0); // Dithering lejano
        } else if (perpWallDist > 4.2f) {
          bool rawPix = getWallPixel(wallType, texX, texY, side);
          isWallPixel = rawPix && ((x + y) % 2 == 0); // Dithering medio
        } else {
          isWallPixel = getWallPixel(wallType, texX, texY, side); // Cercano sólido
        }
      }

      // Oclusión dinámica: los píxeles de pared pisan la rejilla del fondo
      if (isWindowPane) {
        // En los paneles de ventana transparentes, eliminamos la rejilla para ver el cielo limpio
        if (y != 27) {
          display.drawPixel(x, y, SSD1306_BLACK);
        }
      } else {
        // Dibujamos el píxel de la pared o negro para tapar la rejilla
        display.drawPixel(x, y, isWallPixel ? SSD1306_WHITE : SSD1306_BLACK);
      }
    }
  }

  // ==========================================
  //  4. AUTOPILOTO INTELIGENTE (Navegación Suave)
  // ==========================================
  float moveSpeed = 0.040f;
  float rotSpeed = 0.038f;
  
  // Raycast de predicción frontal (Look-Ahead)
  float distToWall = 99.0f;
  for (float d = 0.1f; d < 2.5f; d += 0.1f) {
    int cx = (int)(posX + dirX * d);
    int cy = (int)(posY + dirY * d);
    if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H) {
      if (pgm_read_byte(&worldMap[cx][cy]) > 0) {
        distToWall = d;
        break;
      }
    }
  }
  
  static float currentTurnRate = 0.0f;
  static bool turningMode = false;
  
  if (distToWall < 1.2f) {
    turningMode = true;
    if (currentTurnRate == 0.0f) {
      // Evalúa lateralmente a distancia de 1 bloque para decidir giro inteligente
      float leftX = posX - dirY * 1.0f;
      float leftY = posY + dirX * 1.0f;
      int ilx = (int)leftX;
      int ily = (int)leftY;
      
      if (ilx >= 0 && ilx < MAP_W && ily >= 0 && ily < MAP_H && pgm_read_byte(&worldMap[ilx][ily]) == 0) {
        currentTurnRate = -rotSpeed; // Gira a la izquierda
      } else {
        currentTurnRate = rotSpeed;  // Gira a la derecha
      }
    }
  } else if (distToWall > 1.8f) {
    turningMode = false;
    currentTurnRate = 0.0f;
  }
  
  if (turningMode) {
    // Giro sobre su eje
    float oldDirX = dirX;
    dirX = dirX * cos(currentTurnRate) - dirY * sin(currentTurnRate);
    dirY = oldDirX * sin(currentTurnRate) + dirY * cos(currentTurnRate);
    float oldPlaneX = planeX;
    planeX = planeX * cos(currentTurnRate) - planeY * sin(currentTurnRate);
    planeY = oldPlaneX * sin(currentTurnRate) + planeY * cos(currentTurnRate);
  } else {
    // Avance serpenteante natural de vuelo
    float sway = sin(frameCnt * 0.045f) * 0.012f;
    float oldDirX = dirX;
    dirX = dirX * cos(sway) - dirY * sin(sway);
    dirY = oldDirX * sin(sway) + dirY * cos(sway);
    float oldPlaneX = planeX;
    planeX = planeX * cos(sway) - planeY * sin(sway);
    planeY = oldPlaneX * sin(sway) + planeY * cos(sway);
    
    // Movimiento
    float nextX = posX + dirX * moveSpeed;
    float nextY = posY + dirY * moveSpeed;
    
    // Desplazamiento por colisión de seguridad
    if (pgm_read_byte(&worldMap[(int)nextX][(int)posY]) == 0) posX = nextX;
    if (pgm_read_byte(&worldMap[(int)posX][(int)nextY]) == 0) posY = nextY;
  }

  // ==========================================
  //  5. RADAR MINI-MAPA CON VECTOR DE MIRA
  // ==========================================
  int mapStartX = SCR_W - 27;
  int mapStartY = 2;
  display.fillRect(mapStartX - 1, mapStartY - 1, 26, 26, SSD1306_BLACK);
  display.drawRect(mapStartX - 1, mapStartY - 1, 26, 26, SSD1306_WHITE);
  
  for (int my = 0; my < MAP_H; my++) {
    for (int mx = 0; mx < MAP_W; mx++) {
      if (pgm_read_byte(&worldMap[mx][my]) > 0) {
        display.drawPixel(mapStartX + mx * 2, mapStartY + my * 2, SSD1306_WHITE);
        display.drawPixel(mapStartX + mx * 2 + 1, mapStartY + my * 2, SSD1306_WHITE);
        display.drawPixel(mapStartX + mx * 2, mapStartY + my * 2 + 1, SSD1306_WHITE);
        display.drawPixel(mapStartX + mx * 2 + 1, mapStartY + my * 2 + 1, SSD1306_WHITE);
      }
    }
  }
  
  // Dibujar posición del jugador y vector de dirección parpadeando
  if (frameCnt % 8 < 4) {
    int px = mapStartX + (int)(posX * 2.0f);
    int py = mapStartY + (int)(posY * 2.0f);
    if (px >= mapStartX && px < mapStartX + 24 && py >= mapStartY && py < mapStartY + 24) {
      display.drawPixel(px, py, SSD1306_WHITE);
      display.drawPixel(px + 1, py, SSD1306_WHITE);
      display.drawPixel(px, py + 1, SSD1306_WHITE);
      display.drawPixel(px + 1, py + 1, SSD1306_WHITE);
      
      // Vector de dirección (mira hacia adelante)
      int ex = px + (int)(dirX * 3.5f);
      int ey = py + (int)(dirY * 3.5f);
      display.drawLine(px, py, ex, ey, SSD1306_WHITE);
    }
  }

  // ==========================================
  //  6. TELEMETRÍA Y OSCILOSCOPIO ANIMADO
  // ==========================================
  // Línea divisoria
  display.drawFastHLine(0, 54, SCR_W, SSD1306_WHITE);
  display.fillRect(0, 55, SCR_W, 9, SSD1306_BLACK);
  
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  // Coordenadas POS
  display.setCursor(2, 56);
  display.print(F("POS:"));
  display.print(posX, 1);
  display.print(F(","));
  display.print(posY, 1);
  
  // Osciloscopio animado en el centro del HUD
  int waveCenterY = 59;
  for (int dx = 0; dx < 12; dx++) {
    int dy = (int)(sin(frameCnt * 0.35f + dx * 0.7f) * 2.2f);
    display.drawPixel(53 + dx, waveCenterY + dy, SSD1306_WHITE);
  }
  
  // Rumbo HDG en grados
  display.setCursor(68, 56);
  display.print(F("HDG:"));
  int deg = (int)(playerAngle * 180.0f / 3.14159265f);
  if (deg < 0) deg += 360;
  if (deg < 100) display.print(F("0"));
  if (deg < 10) display.print(F("0"));
  display.print(deg);
  display.print(F("deg"));

  display.display();
  frameCnt++;
  
  // Control de FPS estable (~40 FPS)
  delay(25);
}
