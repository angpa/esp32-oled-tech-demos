// ============================================================
//  RAYCASTER 3D ENGINE (Doom / Wolfenstein 3D style)
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Raycasting engine using Floating Point (ESP32 FPU is fast!)
//  - Distance-based / Axis-based dithering for wall shading
//  - Auto-navigator (bounces around the maze)
//  - HUD in the yellow band (0-15px)
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
//  WORLD MAP
// ============================================================
#define MAP_W 12
#define MAP_H 12

const uint8_t PROGMEM worldMap[MAP_W][MAP_H] = {
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,1,0,0,0,0,0,0,1},
  {1,0,1,0,1,0,1,1,1,1,0,1},
  {1,0,1,0,0,0,0,0,0,1,0,1},
  {1,0,1,1,1,1,0,1,0,1,0,1},
  {1,0,0,0,0,1,0,1,0,0,0,1},
  {1,1,1,1,0,1,0,1,1,1,0,1},
  {1,0,0,0,0,0,0,0,0,1,0,1},
  {1,0,1,1,1,1,1,1,0,1,0,1},
  {1,0,0,0,0,0,0,1,0,1,0,1},
  {1,1,1,1,1,1,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1}
};

// ============================================================
//  CAMERA & PLAYER STATE
// ============================================================
float posX = 1.5f, posY = 1.5f;
float dirX = 1.0f, dirY = 0.0f;
float planeX = 0.0f, planeY = 0.8f; // FOV (approx 75 degrees)
uint16_t frameCnt = 0;

// Gun sprite (simple shotgun)
const unsigned char PROGMEM spr_gun[] = {
  0b00111100,
  0b00111100,
  0b00111100,
  0b00111100,
  0b01111110,
  0b11111111,
  0b11111111,
  0b11111111
};

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 fail"));
    while (1);
  }
}

void loop() {
  display.clearDisplay();

  // ==========================================
  //  1. RAYCASTING 3D RENDERING
  // ==========================================
  for (int x = 0; x < SCR_W; x++) {
    float cameraX = 2 * x / (float)SCR_W - 1; 
    float rayDirX = dirX + planeX * cameraX;
    float rayDirY = dirY + planeY * cameraX;

    int mapX = int(posX);
    int mapY = int(posY);

    float sideDistX, sideDistY;
    float deltaDistX = (rayDirX == 0) ? 1e30 : abs(1 / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30 : abs(1 / rayDirY);
    float perpWallDist;

    int stepX, stepY;
    int hit = 0;
    int side;

    if (rayDirX < 0) { stepX = -1; sideDistX = (posX - mapX) * deltaDistX; } 
    else             { stepX =  1; sideDistX = (mapX + 1.0f - posX) * deltaDistX; }
    
    if (rayDirY < 0) { stepY = -1; sideDistY = (posY - mapY) * deltaDistY; } 
    else             { stepY =  1; sideDistY = (mapY + 1.0f - posY) * deltaDistY; }

    // DDA (Digital Differential Analyzer)
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
      
      // Bounds check
      if(mapX >= 0 && mapX < MAP_W && mapY >= 0 && mapY < MAP_H) {
        if (pgm_read_byte(&worldMap[mapX][mapY]) > 0) hit = 1;
      } else {
        hit = 1; 
      }
    }

    if (side == 0) perpWallDist = (sideDistX - deltaDistX);
    else           perpWallDist = (sideDistY - deltaDistY);

    // Height of display area is 48px (y: 16 to 63)
    int h = 48;
    int lineHeight = (int)(h / perpWallDist);

    int drawStart = -lineHeight / 2 + h / 2;
    if (drawStart < 0) drawStart = 0;
    drawStart += 16; // Shift down below HUD

    int drawEnd = lineHeight / 2 + h / 2;
    if (drawEnd >= h) drawEnd = h - 1;
    drawEnd += 16;

    // Draw wall slice with shading
    if (side == 0) {
      // Light wall
      if (perpWallDist > 4.5) {
        // Dithered for distance
        for (int y = drawStart; y <= drawEnd; y++) {
          if ((x + y) % 2 == 0) display.drawPixel(x, y, SSD1306_WHITE);
        }
      } else {
        // Solid bright
        display.drawLine(x, drawStart, x, drawEnd, SSD1306_WHITE);
      }
    } else {
      // Dark wall (shaded axis)
      for (int y = drawStart; y <= drawEnd; y += 2) {
        display.drawPixel(x, y, SSD1306_WHITE);
      }
    }
    
    // Draw floor / ceiling (optional, maybe too messy, left black for contrast)
  }

  // ==========================================
  //  2. SPRITES (Gun & Muzzle Flash)
  // ==========================================
  // Gun bobs up and down while moving
  int gunY = 56 + (int)(sin(frameCnt * 0.4f) * 2.0f);
  display.drawBitmap(60, gunY, spr_gun, 8, 8, SSD1306_WHITE);

  // Random shooting
  if(frameCnt % 25 == 0) {
    // Muzzle flash
    display.fillCircle(64, gunY-2, 6, SSD1306_WHITE);
    display.fillCircle(64, gunY-2, 3, SSD1306_BLACK); // Hollow core
  }

  // ==========================================
  //  3. HUD (Top 16 pixels - Yellow band)
  // ==========================================
  display.fillRect(0, 0, SCR_W, 16, SSD1306_BLACK);
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setCursor(2, 4);
  display.print(F("HP:99"));
  
  display.setCursor(44, 4);
  display.print(F("AMMO:54"));

  // Doom-style mini face outline
  display.drawRect(108, 1, 14, 13, SSD1306_WHITE);
  display.drawPixel(112, 5, SSD1306_WHITE); // L eye
  display.drawPixel(118, 5, SSD1306_WHITE); // R eye
  display.drawLine(113, 10, 117, 10, SSD1306_WHITE); // Mouth

  display.display();

  // ==========================================
  //  4. AUTO-NAVIGATOR (AI movement)
  // ==========================================
  float moveSpeed = 0.1f;
  float rotSpeed = 0.08f;
  
  // Try to move forward
  float nextX = posX + dirX * moveSpeed;
  float nextY = posY + dirY * moveSpeed;
  
  bool hitWall = false;
  if (pgm_read_byte(&worldMap[int(nextX)][int(posY)]) == 0) {
    posX = nextX;
  } else hitWall = true;

  if (pgm_read_byte(&worldMap[int(posX)][int(nextY)]) == 0) {
    posY = nextY;
  } else hitWall = true;

  // Turn behavior
  if (hitWall) {
    // Force a large turn if stuck
    float oldDirX = dirX;
    dirX = dirX * cos(rotSpeed * 10) - dirY * sin(rotSpeed * 10);
    dirY = oldDirX * sin(rotSpeed * 10) + dirY * cos(rotSpeed * 10);
    float oldPlaneX = planeX;
    planeX = planeX * cos(rotSpeed * 10) - planeY * sin(rotSpeed * 10);
    planeY = oldPlaneX * sin(rotSpeed * 10) + planeY * cos(rotSpeed * 10);
  } else {
    // Gentle sway to make path look natural
    float sway = sin(frameCnt * 0.05f) * 0.03f;
    float oldDirX = dirX;
    dirX = dirX * cos(sway) - dirY * sin(sway);
    dirY = oldDirX * sin(sway) + dirY * cos(sway);
    float oldPlaneX = planeX;
    planeX = planeX * cos(sway) - planeY * sin(sway);
    planeY = oldPlaneX * sin(sway) + planeY * cos(sway);
  }

  frameCnt++;
}
