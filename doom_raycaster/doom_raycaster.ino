// ============================================================
//  RAYCASTER 3D ENGINE (Doom / Wolfenstein 3D Premium)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Motor de Raycasting 3D optimizado para el FPU del ESP32.
//  - Texturas procedimentales en paredes (ladrillos, metal, columnas).
//  - Niebla de distancia con dithering dinámico (fog/anti-aliasing).
//  - Escopeta HD (24x24 px) con retroceso, fogonazo y sacudida de pantalla.
//  - Rostro de Doomguy animado reactivo al ladeo y disparos.
//  - HUD premium con iconos de salud, munición, sistema de daño y respawn.
//  - Radar / Mini-mapa 2D en tiempo real (HUD táctico).
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
//  WORLD MAP (1: Bricks, 2: Metal Grate, 3: Wood Columns)
// ============================================================
#define MAP_W 12
#define MAP_H 12

const uint8_t PROGMEM worldMap[MAP_W][MAP_H] = {
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,2,0,0,0,0,0,0,1},
  {1,0,3,0,2,0,3,3,0,3,0,1},
  {1,0,3,0,0,0,0,0,0,3,0,1},
  {1,0,3,3,3,3,0,2,0,3,0,1},
  {1,0,0,0,0,2,0,2,0,0,0,1},
  {1,2,2,2,0,2,0,2,2,2,0,1},
  {1,0,0,0,0,0,0,0,0,2,0,1},
  {1,0,3,3,3,3,3,3,0,2,0,1},
  {1,0,0,0,0,0,0,3,0,2,0,1},
  {1,3,3,3,3,3,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1}
};

// ============================================================
//  CAMERA & PLAYER STATE
// ============================================================
float posX = 1.5f, posY = 1.5f;
float dirX = 1.0f, dirY = 0.0f;
float planeX = 0.0f, planeY = 0.8f; // FOV
uint16_t frameCnt = 0;

// Variables de combate y juego
int playerHP = 100;
int playerAmmo = 60;
int shootTimer = 0;
int shakeX = 0;
int shakeY = 0;
int damageFlash = 0;
int deathFlash = 0;

// ============================================================
//  BITMAPS & SPRITES (Almacenados en PROGMEM)
// ============================================================

// Icono Corazón HUD (8x8)
const unsigned char PROGMEM icon_heart[] = {
  0b01100110,
  0b11111111,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100,
  0b00011000,
  0b00000000
};

// Icono Bala HUD (8x8)
const unsigned char PROGMEM icon_bullet[] = {
  0b00011000,
  0b00011000,
  0b00111100,
  0b00111100,
  0b00111100,
  0b00111100,
  0b00111100,
  0b00000000
};

// Doomguy Rostro - Frente (16x16)
const unsigned char PROGMEM face_straight[] = {
  0x07, 0xe0, 0x18, 0x18, 0x20, 0x04, 0x40, 0x02,
  0x49, 0x92, 0x40, 0x02, 0x42, 0x42, 0x44, 0x22,
  0x43, 0xc2, 0x40, 0x02, 0x20, 0x04, 0x18, 0x18,
  0x0f, 0xf0, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00
};

// Doomguy Rostro - Mirar Izquierda (16x16)
const unsigned char PROGMEM face_left[] = {
  0x07, 0xe0, 0x18, 0x18, 0x20, 0x04, 0x40, 0x02,
  0x4c, 0x62, 0x40, 0x02, 0x42, 0x42, 0x44, 0x22,
  0x43, 0xc2, 0x40, 0x02, 0x20, 0x04, 0x18, 0x18,
  0x0f, 0xf0, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00
};

// Doomguy Rostro - Mirar Derecha (16x16)
const unsigned char PROGMEM face_right[] = {
  0x07, 0xe0, 0x18, 0x18, 0x20, 0x04, 0x40, 0x02,
  0x46, 0x32, 0x40, 0x02, 0x42, 0x42, 0x44, 0x22,
  0x43, 0xc2, 0x40, 0x02, 0x20, 0x04, 0x18, 0x18,
  0x0f, 0xf0, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00
};

// Doomguy Rostro - Sonrisa de Combate (16x16)
const unsigned char PROGMEM face_grin[] = {
  0x07, 0xe0, 0x18, 0x18, 0x20, 0x04, 0x40, 0x02,
  0x49, 0x92, 0x40, 0x02, 0x44, 0x22, 0x48, 0x12,
  0x47, 0xe2, 0x40, 0x02, 0x20, 0x04, 0x18, 0x18,
  0x0f, 0xf0, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00
};

// Escopeta de Doble Cañón HD (24x24)
const unsigned char PROGMEM spr_shotgun[] = {
  0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x3c, 0x00,
  0x00, 0x3c, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x3c, 0x00, 0x00, 0xff, 0x00,
  0x00, 0xff, 0x00, 0x01, 0xff, 0x80, 0x03, 0xff, 0xc0, 0x07, 0xff, 0xe0,
  0x0f, 0xe7, 0xf0, 0x1f, 0x81, 0xf8, 0x3f, 0x00, 0xfc, 0x3f, 0x00, 0xfc,
  0x7e, 0x00, 0x7e, 0x7e, 0x00, 0x7e, 0x7c, 0x00, 0x3e, 0xfc, 0x00, 0x3f,
  0xf8, 0x00, 0x1f, 0xf8, 0x00, 0x1f, 0xf0, 0x00, 0x0f, 0xe0, 0x00, 0x07
};

// ============================================================
//  TEXTURIZADO PROCEDIMENTAL DE PAREDES
// ============================================================
bool getWallPixel(int wallType, int texX, int texY, int side) {
  // Las paredes del eje Y son más oscuras (sombra direccional 3D)
  if (side == 1) {
    if (texY % 2 == 0 && texX % 2 == 0) return false;
  }
  
  if (wallType == 1) {
    // 1. TEXTURA DE LADRILLO
    if (texY == 0 || texY == 8) return true; // Juntas horizontales
    if (texY < 8) {
      if (texX == 0 || texX == 8) return true; // Juntas verticales fila superior
    } else {
      if (texX == 4 || texX == 12) return true; // Juntas verticales fila inferior
    }
    return false;
  } 
  else if (wallType == 2) {
    // 2. TEXTURA DE REJILLA METÁLICA
    if (texX == 0 || texX == 15 || texY == 0 || texY == 15) return true; // Bordes
    if (texX == texY || texX == 15 - texY) return true; // Rejilla cruzada
    return false;
  } 
  else if (wallType == 3) {
    // 3. COLUMNAS / FRANZAS DE MADERA
    if (texY == 0 || texY == 15) return true; // Secciones
    if (texX == 0 || texX == 8 || texX == 15) return true; // Ranuras verticales
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
  display.clearDisplay();
}

// ============================================================
//  LOOP DE RENDERIZADO Y LÓGICA
// ============================================================
void loop() {
  display.clearDisplay();

  // Controladores de sacudida y parpadeo por disparo
  if (shootTimer > 0) {
    shootTimer--;
    if (shootTimer == 0) {
      shakeX = 0;
      shakeY = 0;
    } else {
      shakeX = random(-2, 3);
      shakeY = random(-2, 3);
    }
  }

  // ==========================================
  //  1. MOTOR RAYCASTING 3D CON TEXTURAS
  // ==========================================
  for (int x = 0; x < SCR_W; x++) {
    float cameraX = 2 * x / (float)SCR_W - 1; 
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

    // Altura del viewport es de 48 píxeles (y: 16 a 63)
    int h = 48;
    int lineHeight = (int)(h / perpWallDist);

    int drawStart = -lineHeight / 2 + h / 2;
    if (drawStart < 0) drawStart = 0;
    drawStart += 16; // Desplazar abajo del HUD

    int drawEnd = lineHeight / 2 + h / 2;
    if (drawEnd >= h) drawEnd = h - 1;
    drawEnd += 16;

    // Obtener textura y tipo de pared golpeada
    int wallType = pgm_read_byte(&worldMap[mapX][mapY]);
    if (wallType == 0) wallType = 1; // Seguridad

    float wallHitX;
    if (side == 0) wallHitX = posY + perpWallDist * rayDirY;
    else           wallHitX = posX + perpWallDist * rayDirX;
    wallHitX -= floor(wallHitX);

    int texX = (int)(wallHitX * 16.0f);
    if (side == 0 && rayDirX > 0) texX = 15 - texX;
    if (side == 1 && rayDirY < 0) texX = 15 - texX;

    // Aplicar sacudida de pantalla en X
    int sx = x + shakeX;
    if (sx >= 0 && sx < SCR_W) {
      int dStart = drawStart + shakeY;
      int dEnd = drawEnd + shakeY;
      
      if (dStart < 16) dStart = 16;
      if (dEnd >= SCR_H) dEnd = SCR_H - 1;

      // Dibujar columna de píxeles texturizados
      for (int y = dStart; y <= dEnd; y++) {
        float wallHitY = (float)(y - dStart) / (dEnd - dStart + 1);
        int texY = (int)(wallHitY * 16.0f);
        if (texY < 0) texY = 0;
        if (texY > 15) texY = 15;

        bool drawPix = false;

        // Niebla/Dithering por distancia
        if (perpWallDist > 6.0f) {
          drawPix = ((sx + y) % 4 == 0); // Muy lejano: degradado genérico
        } else if (perpWallDist > 4.2f) {
          bool rawPix = getWallPixel(wallType, texX, texY, side);
          drawPix = rawPix && ((sx + y) % 2 == 0); // Mediano: textura tramada
        } else {
          drawPix = getWallPixel(wallType, texX, texY, side); // Cercano: textura sólida
        }

        if (drawPix) {
          display.drawPixel(sx, y, SSD1306_WHITE);
        }
      }
    }
  }

  // ==========================================
  //  2. SPRITES (Escopeta HD y Fogonazo)
  // ==========================================
  int gunX = 52; // Centrado exacto: (128/2) - (24/2)
  int gunY = 40 + (int)(sin(frameCnt * 0.35f) * 1.5f); // Bobbing vertical
  if (shootTimer > 0) gunY += 5; // Retroceso del arma

  display.drawBitmap(gunX, gunY, spr_shotgun, 24, 24, SSD1306_WHITE);

  // Fogonazo procedimental en el cañón
  if (shootTimer >= 2) {
    int fx = 64;
    int fy = gunY - 2;
    display.fillCircle(fx, fy, 8, SSD1306_WHITE);
    display.fillCircle(fx, fy, 3, SSD1306_BLACK); // Núcleo hueco
    // Destellos radiales
    display.drawLine(fx - 14, fy, fx + 14, fy, SSD1306_WHITE);
    display.drawLine(fx, fy - 14, fx, fy + 8, SSD1306_WHITE);
    display.drawLine(fx - 10, fy - 10, fx + 10, fy + 10, SSD1306_WHITE);
    display.drawLine(fx - 10, fy + 10, fx + 10, fy - 10, SSD1306_WHITE);
  }

  // ==========================================
  //  3. AUTO-PILOTO Y LÓGICA DE MOVIMIENTO
  // ==========================================
  float moveSpeed = 0.05f;
  float rotSpeed = 0.06f;
  
  float nextX = posX + dirX * moveSpeed;
  float nextY = posY + dirY * moveSpeed;
  
  bool hitWall = false;
  
  // Colisiones físicas
  int ipX = int(nextX);
  int ipY = int(posY);
  if (ipX >= 0 && ipX < MAP_W && ipY >= 0 && ipY < MAP_H && pgm_read_byte(&worldMap[ipX][ipY]) == 0) {
    posX = nextX;
  } else hitWall = true;
  
  int ipX2 = int(posX);
  int ipY2 = int(nextY);
  if (ipX2 >= 0 && ipX2 < MAP_W && ipY2 >= 0 && ipY2 < MAP_H && pgm_read_byte(&worldMap[ipX2][ipY2]) == 0) {
    posY = nextY;
  } else hitWall = true;
  
  int turnState = 0; // 0: Centro, 1: Izq, 2: Der
  
  if (hitWall) {
    // Impacto: Reducir salud y encender indicador de daño
    if (frameCnt % 8 == 0 && playerHP > 0) {
      playerHP -= random(5, 12);
      damageFlash = 3; // HUD flash por 3 cuadros
    }
    
    // Girar bruscamente al chocar
    float oldDirX = dirX;
    dirX = dirX * cos(rotSpeed * 8) - dirY * sin(rotSpeed * 8);
    dirY = oldDirX * sin(rotSpeed * 8) + dirY * cos(rotSpeed * 8);
    float oldPlaneX = planeX;
    planeX = planeX * cos(rotSpeed * 8) - planeY * sin(rotSpeed * 8);
    planeY = oldPlaneX * sin(rotSpeed * 8) + planeY * cos(rotSpeed * 8);
    
    turnState = (frameCnt % 12 < 6) ? 1 : 2; // Ojos moviéndose
  } else {
    // Movimiento serpenteante natural
    float sway = sin(frameCnt * 0.05f) * 0.025f;
    float oldDirX = dirX;
    dirX = dirX * cos(sway) - dirY * sin(sway);
    dirY = oldDirX * sin(sway) + dirY * cos(sway);
    float oldPlaneX = planeX;
    planeX = planeX * cos(sway) - planeY * sin(sway);
    planeY = oldPlaneX * sin(sway) + planeY * cos(sway);
    
    if (sway > 0.007f) turnState = 1;
    else if (sway < -0.007f) turnState = 2;
  }

  // Disparo automático cada cierto intervalo
  if (frameCnt % 32 == 0 && shootTimer == 0) {
    shootTimer = 4;
    playerAmmo--;
    if (playerAmmo < 0) playerAmmo = 60; // Recarga
  }

  // Sistema de Muerte y Regeneración
  if (playerHP <= 0) {
    deathFlash = 12; // Destello de muerte de la pantalla
    playerHP = 100;
    playerAmmo = 60;
    posX = 1.5f;
    posY = 1.5f;
    dirX = 1.0f;
    dirY = 0.0f;
    planeX = 0.0f;
    planeY = 0.8f;
  }

  // ==========================================
  //  4. RADAR MINI-MAPA OVERLAY
  // ==========================================
  int mapStartX = SCR_W - 27;
  int mapStartY = 17;
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
  
  // Dibujar posición del jugador parpadeando en el radar
  if (frameCnt % 8 < 4) {
    int px = mapStartX + (int)(posX * 2.0f);
    int py = mapStartY + (int)(posY * 2.0f);
    if (px >= mapStartX && px < mapStartX + 24 && py >= mapStartY && py < mapStartY + 24) {
      display.drawPixel(px, py, SSD1306_WHITE);
      display.drawPixel(px + 1, py, SSD1306_WHITE);
      display.drawPixel(px, py + 1, SSD1306_WHITE);
      display.drawPixel(px + 1, py + 1, SSD1306_WHITE);
    }
  }

  // ==========================================
  //  5. HUD SUPERIOR PREMIUM
  // ==========================================
  // Fondo del HUD con indicador de daño parpadeante
  display.fillRect(0, 0, SCR_W, 16, (damageFlash > 0) ? SSD1306_WHITE : SSD1306_BLACK);
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);
  if (damageFlash > 0) damageFlash--;

  uint16_t hudCol = (damageFlash > 0) ? SSD1306_BLACK : SSD1306_WHITE;
  display.setTextColor(hudCol);
  display.setTextSize(1);

  // Icono y valor de Salud
  display.drawBitmap(2, 4, icon_heart, 8, 8, hudCol);
  display.setCursor(12, 4);
  display.print(playerHP);

  // Icono y valor de Munición
  display.drawBitmap(48, 4, icon_bullet, 8, 8, hudCol);
  display.setCursor(58, 4);
  display.print(playerAmmo);

  // Dibujar Rostro de Doomguy
  const unsigned char* currentFace = face_straight;
  if (shootTimer > 0) {
    currentFace = face_grin;
  } else {
    if (turnState == 1) currentFace = face_left;
    else if (turnState == 2) currentFace = face_right;
  }
  display.drawBitmap(108, 0, currentFace, 16, 16, hudCol);

  // ==========================================
  //  6. CONTROL DE DESTALLO DE MUERTE
  // ==========================================
  if (deathFlash > 0) {
    deathFlash--;
    display.invertDisplay(frameCnt % 2 == 0); // Efecto estroboscópico de muerte
  } else {
    display.invertDisplay(false);
  }

  display.display();
  frameCnt++;
}
