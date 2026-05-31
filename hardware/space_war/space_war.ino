#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// SPRITES EN PIXEL ART (Alta calidad)
// ==========================================

// Nave del jugador (16x8) - Diseño estilo caza estelar
const unsigned char ship_bmp[] PROGMEM = {
  0x00, 0x80, // ........#.......
  0x01, 0xC0, // .......###......
  0x03, 0xE0, // ......#####.....
  0x1F, 0xFC, // ...#########....
  0x7F, 0xFE, // .##############.
  0xFF, 0xFF, // ################
  0x3F, 0xFC, // ..############..
  0x0C, 0x30, // ....##....##....
};

// Nave con propulsores encendidos (frame 2)
const unsigned char ship_thrust_bmp[] PROGMEM = {
  0x00, 0x80, // ........#.......
  0x01, 0xC0, // .......###......
  0x03, 0xE0, // ......#####.....
  0x1F, 0xFC, // ...#########....
  0x7F, 0xFE, // .##############.
  0xFF, 0xFF, // ################
  0x3F, 0xFC, // ..############..
  0x2C, 0x34, // ..#.##....##.#..
};

// Enemigo tipo 1: Nave alien (12x8)
const unsigned char enemy1_bmp[] PROGMEM = {
  0x1E, 0x00, // ...####.........
  0x7F, 0x80, // .########.......
  0xDB, 0x00, // ##.##.##........
  0xFF, 0x80, // #########.......
  0x7F, 0x00, // .#######........
  0xDB, 0x80, // ##.##.###.......
  0x42, 0x00, // .#....#.........
  0xA5, 0x00, // #.#..#.#........
};

// Enemigo tipo 2: Crucero pesado (14x8)
const unsigned char enemy2_bmp[] PROGMEM = {
  0x06, 0x00, // .....##.........
  0x0F, 0x00, // ....####........
  0x3F, 0xC0, // ..########......
  0xFF, 0xF0, // ############....
  0xFF, 0xF0, // ############....
  0x3F, 0xC0, // ..########......
  0x0F, 0x00, // ....####........
  0x06, 0x00, // .....##.........
};

// Enemigo tipo 3: Asteroide (8x8)
const unsigned char asteroid_bmp[] PROGMEM = {
  0x3C, // ..####..
  0x7E, // .######.
  0xFB, // #####.##
  0xFF, // ########
  0xFF, // ########
  0xDF, // ##.#####
  0x7E, // .######.
  0x3C, // ..####..
};

// Explosion frame 1 (8x8)
const unsigned char explosion1_bmp[] PROGMEM = {
  0x00, // ........
  0x24, // ..#..#..
  0x18, // ...##...
  0x7E, // .######.
  0x7E, // .######.
  0x18, // ...##...
  0x24, // ..#..#..
  0x00, // ........
};

// Explosion frame 2 (8x8)
const unsigned char explosion2_bmp[] PROGMEM = {
  0x42, // .#....#.
  0x24, // ..#..#..
  0x5A, // .#.##.#.
  0xBD, // #.####.#
  0xBD, // #.####.#
  0x5A, // .#.##.#.
  0x24, // ..#..#..
  0x42, // .#....#.
};

// Explosion frame 3 (8x8)
const unsigned char explosion3_bmp[] PROGMEM = {
  0x81, // #......#
  0x42, // .#....#.
  0x24, // ..#..#..
  0x18, // ...##...
  0x18, // ...##...
  0x24, // ..#..#..
  0x42, // .#....#.
  0x81, // #......#
};

// Powerup (escudo) (8x8)
const unsigned char powerup_bmp[] PROGMEM = {
  0x3C, // ..####..
  0x42, // .#....#.
  0x99, // #..##..#
  0xA5, // #.#..#.#
  0xA5, // #.#..#.#
  0x99, // #..##..#
  0x42, // .#....#.
  0x3C, // ..####..
};

// ==========================================
// ESTRELLAS DE FONDO (Parallax de 3 capas)
// ==========================================
#define NUM_STARS_FAR 12
#define NUM_STARS_MID 8
#define NUM_STARS_NEAR 5

struct Star {
  int16_t x;
  int16_t y;
};

Star starsFar[NUM_STARS_FAR];
Star starsMid[NUM_STARS_MID];
Star starsNear[NUM_STARS_NEAR];

// ==========================================
// ENTIDADES DEL JUEGO
// ==========================================
#define MAX_BULLETS 6
#define MAX_ENEMIES 5
#define MAX_EXPLOSIONS 4
#define MAX_PARTICLES 10

struct Bullet {
  int16_t x, y;
  bool active;
};

struct Enemy {
  int16_t x, y;
  int8_t type;       // 0=alien, 1=crucero, 2=asteroide
  int8_t hp;
  int8_t movePattern; // patron de movimiento
  float phase;        // fase para mov sinusoidal
  bool active;
};

struct Explosion {
  int16_t x, y;
  int8_t frame;
  int8_t timer;
  bool active;
};

struct Particle {
  float x, y;
  float vx, vy;
  int8_t life;
  bool active;
};

// Nave del jugador
float shipY = 28.0;
float shipVelY = 0.0;
int shipX = 8;
bool shieldActive = false;
int shieldTimer = 0;

// Balas
Bullet bullets[MAX_BULLETS];

// Enemigos
Enemy enemies[MAX_ENEMIES];

// Explosiones
Explosion explosions[MAX_EXPLOSIONS];

// Particulas de exhaust
Particle particles[MAX_PARTICLES];

// Estado del juego
unsigned long frame = 0;
int score = 0;
int hull = 100;      // Vida del jugador
int bulletCooldown = 0;
int enemySpawnTimer = 0;
int difficulty = 1;
bool gameOver = false;
int dangerTimer = 0;

// Semilla de pseudo-random simple
uint16_t rngState = 12345;
uint16_t fastRandom() {
  rngState ^= rngState << 7;
  rngState ^= rngState >> 9;
  rngState ^= rngState << 8;
  return rngState;
}

// ==========================================
// INICIALIZACION
// ==========================================
void initStars() {
  for (int i = 0; i < NUM_STARS_FAR; i++) {
    starsFar[i].x = fastRandom() % SCREEN_WIDTH;
    starsFar[i].y = fastRandom() % SCREEN_HEIGHT;
  }
  for (int i = 0; i < NUM_STARS_MID; i++) {
    starsMid[i].x = fastRandom() % SCREEN_WIDTH;
    starsMid[i].y = fastRandom() % SCREEN_HEIGHT;
  }
  for (int i = 0; i < NUM_STARS_NEAR; i++) {
    starsNear[i].x = fastRandom() % SCREEN_WIDTH;
    starsNear[i].y = fastRandom() % SCREEN_HEIGHT;
  }
}

void initGame() {
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
  for (int i = 0; i < MAX_EXPLOSIONS; i++) explosions[i].active = false;
  for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;
  initStars();
}

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 fail"));
    for (;;);
  }

  initGame();
}

// ==========================================
// ACTUALIZACION DE ESTRELLAS (Parallax)
// ==========================================
void updateStars() {
  // Capa lejana: velocidad 1px cada 2 frames
  if (frame % 2 == 0) {
    for (int i = 0; i < NUM_STARS_FAR; i++) {
      starsFar[i].x--;
      if (starsFar[i].x < 0) {
        starsFar[i].x = SCREEN_WIDTH - 1;
        starsFar[i].y = fastRandom() % SCREEN_HEIGHT;
      }
    }
  }
  // Capa media: velocidad 1px por frame
  for (int i = 0; i < NUM_STARS_MID; i++) {
    starsMid[i].x--;
    if (starsMid[i].x < 0) {
      starsMid[i].x = SCREEN_WIDTH - 1;
      starsMid[i].y = fastRandom() % SCREEN_HEIGHT;
    }
  }
  // Capa cercana: velocidad 2px por frame
  for (int i = 0; i < NUM_STARS_NEAR; i++) {
    starsNear[i].x -= 2;
    if (starsNear[i].x < 0) {
      starsNear[i].x = SCREEN_WIDTH - 1;
      starsNear[i].y = fastRandom() % SCREEN_HEIGHT;
    }
  }
}

void drawStars() {
  for (int i = 0; i < NUM_STARS_FAR; i++) {
    display.drawPixel(starsFar[i].x, starsFar[i].y, SSD1306_WHITE);
  }
  for (int i = 0; i < NUM_STARS_MID; i++) {
    display.drawPixel(starsMid[i].x, starsMid[i].y, SSD1306_WHITE);
    // Estrella ligeramente mas grande
    if (i % 3 == 0) display.drawPixel(starsMid[i].x + 1, starsMid[i].y, SSD1306_WHITE);
  }
  for (int i = 0; i < NUM_STARS_NEAR; i++) {
    // Estrella cercana = linea corta para dar sensacion de velocidad
    display.drawLine(starsNear[i].x, starsNear[i].y, starsNear[i].x + 2, starsNear[i].y, SSD1306_WHITE);
  }
}

// ==========================================
// MOVIMIENTO DE LA NAVE (Autopiloto cinematico)
// ==========================================
void updateShip() {
  // Movimiento sinusoidal suave con variacion
  float targetY = 28.0 + sin(frame * 0.04) * 12.0 + cos(frame * 0.027) * 8.0;
  
  // Suavizado tipo "easing" para movimiento natural
  shipVelY = (targetY - shipY) * 0.08;
  shipY += shipVelY;
  
  // Mantener dentro de pantalla (zona azul: 16-56)
  if (shipY < 18) shipY = 18;
  if (shipY > 52) shipY = 52;
}

void drawShip() {
  // Alternar sprite de propulsores cada 3 frames
  if ((frame / 3) % 2 == 0) {
    display.drawBitmap(shipX, (int)shipY, ship_bmp, 16, 8, SSD1306_WHITE);
  } else {
    display.drawBitmap(shipX, (int)shipY, ship_thrust_bmp, 16, 8, SSD1306_WHITE);
  }
  
  // Escudo visual
  if (shieldActive) {
    display.drawCircle(shipX + 8, (int)shipY + 4, 10, SSD1306_WHITE);
    shieldTimer--;
    if (shieldTimer <= 0) shieldActive = false;
  }
}

// ==========================================
// PARTICULAS DE ESCAPE DEL MOTOR
// ==========================================
void spawnExhaustParticle() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) {
      particles[i].active = true;
      particles[i].x = shipX - 1;
      particles[i].y = shipY + 4 + ((fastRandom() % 3) - 1);
      particles[i].vx = -1.0 - (fastRandom() % 100) / 100.0;
      particles[i].vy = ((fastRandom() % 100) - 50) / 100.0;
      particles[i].life = 4 + (fastRandom() % 4);
      break;
    }
  }
}

void updateParticles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;
      particles[i].life--;
      if (particles[i].life <= 0 || particles[i].x < 0) {
        particles[i].active = false;
      }
    }
  }
}

void drawParticles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      display.drawPixel((int)particles[i].x, (int)particles[i].y, SSD1306_WHITE);
    }
  }
}

// ==========================================
// DISPAROS
// ==========================================
void fireBullet() {
  if (bulletCooldown > 0) return;
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].active = true;
      bullets[i].x = shipX + 16;
      bullets[i].y = (int)shipY + 3;
      bulletCooldown = 8;
      break;
    }
  }
}

void updateBullets() {
  if (bulletCooldown > 0) bulletCooldown--;
  
  // Disparar automaticamente cada cierto tiempo
  if (frame % 12 == 0) fireBullet();
  
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      bullets[i].x += 4; // Velocidad del laser
      if (bullets[i].x > SCREEN_WIDTH) {
        bullets[i].active = false;
      }
    }
  }
}

void drawBullets() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      // Laser con brillo: linea doble
      display.drawLine(bullets[i].x, bullets[i].y, bullets[i].x + 5, bullets[i].y, SSD1306_WHITE);
      display.drawLine(bullets[i].x + 1, bullets[i].y + 1, bullets[i].x + 4, bullets[i].y + 1, SSD1306_WHITE);
    }
  }
}

// ==========================================
// ENEMIGOS
// ==========================================
void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].active = true;
      enemies[i].x = SCREEN_WIDTH + 5;
      enemies[i].y = 18 + (fastRandom() % 34);
      enemies[i].type = fastRandom() % 3;
      enemies[i].hp = (enemies[i].type == 1) ? 3 : (enemies[i].type == 2) ? 2 : 1;
      enemies[i].movePattern = fastRandom() % 3;
      enemies[i].phase = (fastRandom() % 628) / 100.0;
      break;
    }
  }
}

void updateEnemies() {
  enemySpawnTimer++;
  int spawnRate = max(20, 60 - difficulty * 5);
  if (enemySpawnTimer >= spawnRate) {
    spawnEnemy();
    enemySpawnTimer = 0;
  }
  
  // Aumentar dificultad cada 300 frames
  if (frame % 300 == 0 && difficulty < 10) difficulty++;
  
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    
    // Velocidad base segun tipo
    int speed = (enemies[i].type == 2) ? 1 : 2; // Asteroides mas lentos
    enemies[i].x -= speed;
    
    // Patron de movimiento
    enemies[i].phase += 0.08;
    switch (enemies[i].movePattern) {
      case 0: // Recto
        break;
      case 1: // Sinusoidal
        enemies[i].y += sin(enemies[i].phase) * 1.5;
        break;
      case 2: // Zigzag
        enemies[i].y += (((int)(enemies[i].phase * 2) % 2) == 0) ? 1 : -1;
        break;
    }
    
    // Mantener en pantalla
    if (enemies[i].y < 17) enemies[i].y = 17;
    if (enemies[i].y > 54) enemies[i].y = 54;
    
    // Fuera de pantalla
    if (enemies[i].x < -16) {
      enemies[i].active = false;
    }
  }
}

void drawEnemies() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    switch (enemies[i].type) {
      case 0:
        display.drawBitmap(enemies[i].x, enemies[i].y, enemy1_bmp, 12, 8, SSD1306_WHITE);
        break;
      case 1:
        display.drawBitmap(enemies[i].x, enemies[i].y, enemy2_bmp, 14, 8, SSD1306_WHITE);
        break;
      case 2:
        display.drawBitmap(enemies[i].x, enemies[i].y, asteroid_bmp, 8, 8, SSD1306_WHITE);
        break;
    }
  }
}

// ==========================================
// EXPLOSIONES
// ==========================================
void spawnExplosion(int16_t x, int16_t y) {
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!explosions[i].active) {
      explosions[i].active = true;
      explosions[i].x = x;
      explosions[i].y = y;
      explosions[i].frame = 0;
      explosions[i].timer = 0;
      break;
    }
  }
}

void updateExplosions() {
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!explosions[i].active) {
      continue;
    }
    explosions[i].timer++;
    if (explosions[i].timer >= 4) {
      explosions[i].timer = 0;
      explosions[i].frame++;
      if (explosions[i].frame >= 4) {
        explosions[i].active = false;
      }
    }
  }
}

void drawExplosions() {
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!explosions[i].active) continue;
    switch (explosions[i].frame) {
      case 0:
        display.drawBitmap(explosions[i].x, explosions[i].y, explosion1_bmp, 8, 8, SSD1306_WHITE);
        break;
      case 1:
        display.drawBitmap(explosions[i].x, explosions[i].y, explosion2_bmp, 8, 8, SSD1306_WHITE);
        break;
      case 2:
        display.drawBitmap(explosions[i].x, explosions[i].y, explosion3_bmp, 8, 8, SSD1306_WHITE);
        break;
      case 3:
        display.drawBitmap(explosions[i].x, explosions[i].y, explosion1_bmp, 8, 8, SSD1306_WHITE);
        break;
    }
  }
}

// ==========================================
// COLISIONES
// ==========================================
void checkCollisions() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    for (int j = 0; j < MAX_ENEMIES; j++) {
      if (!enemies[j].active) continue;
      
      int ew = (enemies[j].type == 2) ? 8 : 12;
      
      if (bullets[i].x + 5 >= enemies[j].x &&
          bullets[i].x <= enemies[j].x + ew &&
          bullets[i].y + 1 >= enemies[j].y &&
          bullets[i].y <= enemies[j].y + 8) {
        // Impacto!
        bullets[i].active = false;
        enemies[j].hp--;
        
        if (enemies[j].hp <= 0) {
          spawnExplosion(enemies[j].x, enemies[j].y);
          enemies[j].active = false;
          score += (enemies[j].type + 1) * 10;
        }
        break;
      }
    }
  }
  
  // Colision nave-enemigo
  for (int j = 0; j < MAX_ENEMIES; j++) {
    if (!enemies[j].active) continue;
    
    int ew = (enemies[j].type == 2) ? 8 : 12;
    
    if (shipX + 16 >= enemies[j].x &&
        shipX <= enemies[j].x + ew &&
        (int)shipY + 8 >= enemies[j].y &&
        (int)shipY <= enemies[j].y + 8) {
      
      if (!shieldActive) {
        hull -= 20;
        dangerTimer = 30;
      }
      spawnExplosion(enemies[j].x, enemies[j].y);
      enemies[j].active = false;
      score += 5;
    }
  }
}

// ==========================================
// HUD (Zona amarilla: 0-15px)
// ==========================================
void drawHUD() {
  // Linea divisoria
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);
  
  // HULL (barra de vida)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("HULL:"));
  
  // Barra de vida
  int barWidth = map(hull, 0, 100, 0, 30);
  display.drawRect(30, 0, 32, 6, SSD1306_WHITE);
  display.fillRect(31, 1, barWidth, 4, SSD1306_WHITE);
  
  // Score
  display.setCursor(70, 0);
  display.print(F("SC:"));
  display.print(score);
  
  // DANGER!! parpadeante
  if (dangerTimer > 0) {
    dangerTimer--;
    if ((frame / 4) % 2 == 0) {
      display.setCursor(80, 8);
      display.print(F("DANGER!!"));
    }
  }
  
  // Nivel de dificultad
  display.setCursor(0, 8);
  display.print(F("LV:"));
  display.print(difficulty);
}

// ==========================================
// GAME OVER
// ==========================================
void drawGameOver() {
  display.clearDisplay();
  
  // Fondo de estrellas sigue moviéndose
  updateStars();
  drawStars();
  
  // Caja centrada con borde doble
  display.drawRect(20, 10, 88, 44, SSD1306_WHITE);
  display.drawRect(22, 12, 84, 40, SSD1306_WHITE);
  
  // Limpiar interior
  display.fillRect(23, 13, 82, 38, SSD1306_BLACK);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Titulo parpadeante
  if ((frame / 15) % 2 == 0) {
    display.setCursor(32, 18);
    display.print(F("GAME OVER"));
  }
  
  display.setCursor(30, 30);
  display.print(F("SCORE: "));
  display.print(score);
  
  display.setCursor(30, 40);
  display.print(F("LEVEL: "));
  display.print(difficulty);
  
  display.display();
  
  // Reiniciar despues de 5 segundos
  if (frame % 150 == 0) {
    score = 0;
    hull = 100;
    difficulty = 1;
    gameOver = false;
    dangerTimer = 0;
    shipY = 28.0;
    initGame();
  }
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================
void loop() {
  frame++;
  
  if (hull <= 0 && !gameOver) {
    gameOver = true;
    spawnExplosion(shipX, (int)shipY);
  }
  
  if (gameOver) {
    drawGameOver();
    return;
  }
  
  display.clearDisplay();
  
  // 1. Actualizar todo
  updateStars();
  updateShip();
  updateBullets();
  updateEnemies();
  updateExplosions();
  updateParticles();
  checkCollisions();
  
  // Generar particulas de motor cada 2 frames
  if (frame % 2 == 0) spawnExhaustParticle();
  
  // 2. Dibujar todo (orden de capas: fondo -> objetos -> HUD)
  drawStars();
  drawParticles();
  drawBullets();
  drawEnemies();
  drawExplosions();
  drawShip();
  drawHUD();
  
  // 3. Enviar al display
  display.display();
  
  // ~30 FPS
  delay(33);
}
