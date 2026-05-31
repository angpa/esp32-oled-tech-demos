// ============================================================
//  FLOCKING BOIDS WITH QUADTREE SPATIAL PARTITION
//  ESP32 + SSD1306 128x64 OLED
// ============================================================
//  - Simulación colectiva de bandadas de boids autónomos (120 boids).
//  - Algoritmo de partición espacial Quadtree (Zero-Allocation) en RAM.
//  - Reducción del coste de colisión de O(N^2) a O(N log N).
//  - IA Depredador que persigue a los boids, forzando dispersión y pánico.
//  - Re-aparecido (respawn) de boids devorados en los bordes del mapa.
//  - Renderizado vectorial de orientación del vuelo a 50+ FPS.
//  - HUD de telemetría inferior mostrando FPS reales y nodos Quadtree.
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCR_W 128
#define SCR_H 64
#define OLED_RESET -1
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

#define MAX_BOIDS 120
#define MAX_QT_NODES 200 // Capacidad máxima del pool de nodos

// ============================================================
//  ESTRUCTURAS Y CLASES
// ============================================================

struct Boid {
  float x, y;
  float vx, vy;
};

struct Boundary {
  float x, y;    // Centro
  float hw, hh;  // Semi-ancho y semi-alto
  
  bool contains(float bx, float by) const {
    return (bx >= x - hw && bx <= x + hw && by >= y - hh && by <= y + hh);
  }
  
  bool intersects(float bx, float by, float r) const {
    // Intersección de círculo de consulta con la caja AABB
    float closestX = max(x - hw, min(bx, x + hw));
    float closestY = max(y - hh, min(by, y + hh));
    float distX = bx - closestX;
    float distY = by - closestY;
    return (distX * distX + distY * distY) < (r * r);
  }
};

struct QTNode {
  Boundary boundary;
  int boidIndices[4]; // Capacidad máxima de 4 boids por nodo
  int boidCount;
  bool divided;
  
  // Índices en el pool estático (-1 indica nulo)
  int nw, ne, sw, se;
};

// Variables globales de Boids y Depredador
Boid boids[MAX_BOIDS];
float predX = 64.0f, predY = 32.0f;
float predVx = 0.5f, predVy = 0.5f;

// Pool de memoria estática para el Quadtree
QTNode qtPool[MAX_QT_NODES];
int qtNodeCount = 0;

// Variables globales para la consulta de vecindad
int queryResults[48];
int queryCount = 0;

// Parámetros de la simulación
const float viewRadius = 12.0f;   // Rango de visión de boids
const float sepRadius = 4.5f;     // Rango de separación personal
const float maxSpeed = 1.3f;      // Velocidad máxima boids
const float maxForce = 0.04f;     // Fuerza de viraje máxima boids
const float predMaxSpeed = 1.6f;  // Velocidad máxima depredador

// Telemetría de FPS
uint32_t lastFPSUpdate = 0;
int fps = 0;
int frameCounter = 0;
int displayFps = 0;

// ============================================================
//  MÉTODOS DEL QUADTREE ESTÁTICO (Zero-Allocation)
// ============================================================

void clearQuadtree() {
  qtNodeCount = 0;
}

// Subdividir un nodo en 4 cuadrantes
void subdivide(int nodeIdx) {
  if (qtNodeCount >= MAX_QT_NODES - 4) return; // Pool lleno, abortar subdivisión
  
  Boundary b = qtPool[nodeIdx].boundary;
  float hx = b.hw * 0.5f;
  float hy = b.hh * 0.5f;
  
  int nw = qtNodeCount++;
  int ne = qtNodeCount++;
  int sw = qtNodeCount++;
  int se = qtNodeCount++;
  
  qtPool[nw] = { { b.x - hx, b.y - hy, hx, hy }, {}, 0, false, -1, -1, -1, -1 };
  qtPool[ne] = { { b.x + hx, b.y - hy, hx, hy }, {}, 0, false, -1, -1, -1, -1 };
  qtPool[sw] = { { b.x - hx, b.y + hy, hx, hy }, {}, 0, false, -1, -1, -1, -1 };
  qtPool[se] = { { b.x + hx, b.y + hy, hx, hy }, {}, 0, false, -1, -1, -1, -1 };
  
  qtPool[nodeIdx].nw = nw;
  qtPool[nodeIdx].ne = ne;
  qtPool[nodeIdx].sw = sw;
  qtPool[nodeIdx].se = se;
  
  qtPool[nodeIdx].divided = true;
}

// Insertar un boid en el nodo correspondiente
bool insertBoid(int nodeIdx, int boidIdx) {
  float bx = boids[boidIdx].x;
  float by = boids[boidIdx].y;
  
  if (!qtPool[nodeIdx].boundary.contains(bx, by)) {
    return false;
  }
  
  // Si hay espacio y no está dividido, insertar aquí
  if (qtPool[nodeIdx].boidCount < 4 && !qtPool[nodeIdx].divided) {
    qtPool[nodeIdx].boidIndices[qtPool[nodeIdx].boidCount++] = boidIdx;
    return true;
  }
  
  // Subdividir si aún no se ha hecho
  if (!qtPool[nodeIdx].divided) {
    subdivide(nodeIdx);
    
    // Redistribuir los boids locales a los hijos
    for (int i = 0; i < qtPool[nodeIdx].boidCount; i++) {
      int idx = qtPool[nodeIdx].boidIndices[i];
      insertBoid(qtPool[nodeIdx].nw, idx);
      insertBoid(qtPool[nodeIdx].ne, idx);
      insertBoid(qtPool[nodeIdx].sw, idx);
      insertBoid(qtPool[nodeIdx].se, idx);
    }
    qtPool[nodeIdx].boidCount = 0;
  }
  
  // Tratar de insertar en los hijos
  if (insertBoid(qtPool[nodeIdx].nw, boidIdx)) return true;
  if (insertBoid(qtPool[nodeIdx].ne, boidIdx)) return true;
  if (insertBoid(qtPool[nodeIdx].sw, boidIdx)) return true;
  if (insertBoid(qtPool[nodeIdx].se, boidIdx)) return true;
  
  return false;
}

// Consultar boids en un radio circular
void queryQuadtree(int nodeIdx, float qx, float qy, float r) {
  if (!qtPool[nodeIdx].boundary.intersects(qx, qy, r)) {
    return; // Sin intersección con el nodo
  }
  
  if (qtPool[nodeIdx].divided) {
    queryQuadtree(qtPool[nodeIdx].nw, qx, qy, r);
    queryQuadtree(qtPool[nodeIdx].ne, qx, qy, r);
    queryQuadtree(qtPool[nodeIdx].sw, qx, qy, r);
    queryQuadtree(qtPool[nodeIdx].se, qx, qy, r);
  } else {
    for (int i = 0; i < qtPool[nodeIdx].boidCount; i++) {
      int idx = qtPool[nodeIdx].boidIndices[i];
      float dx = boids[idx].x - qx;
      float dy = boids[idx].y - qy;
      if (dx * dx + dy * dy < r * r) {
        if (queryCount < 48) {
          queryResults[queryCount++] = idx;
        }
      }
    }
  }
}

// ============================================================
//  SETUP Y INICIALIZACIÓN
// ============================================================
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }
  display.clearDisplay();
  
  // Inicialización aleatoria de boids
  for (int i = 0; i < MAX_BOIDS; i++) {
    boids[i].x = random(2, 126);
    boids[i].y = random(2, 52); // Espacio libre arriba del HUD
    
    float angle = random(0, 360) * 3.14159f / 180.0f;
    float spd = random(8, 14) * 0.1f;
    boids[i].vx = cos(angle) * spd;
    boids[i].vy = sin(angle) * spd;
  }
  
  // Depredador inicial
  predX = 64.0f;
  predY = 27.0f;
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  display.clearDisplay();
  
  // 1. Reconstruir el Quadtree estático
  clearQuadtree();
  int rootIdx = qtNodeCount++;
  // El límite superior de y es 54 debido al HUD
  qtPool[rootIdx] = { { 64.0f, 27.0f, 64.0f, 27.0f }, {}, 0, false, -1, -1, -1, -1 };
  
  for (int i = 0; i < MAX_BOIDS; i++) {
    insertBoid(rootIdx, i);
  }

  // 2. IA del Depredador (Encuentra al boid más cercano y lo caza)
  float minDistSq = 99999.0f;
  int targetIdx = -1;
  for (int i = 0; i < MAX_BOIDS; i++) {
    float dx = boids[i].x - predX;
    float dy = boids[i].y - predY;
    float dSq = dx*dx + dy*dy;
    if (dSq < minDistSq) {
      minDistSq = dSq;
      targetIdx = i;
    }
  }
  
  if (targetIdx != -1) {
    float targetDx = boids[targetIdx].x - predX;
    float targetDy = boids[targetIdx].y - predY;
    float dist = sqrt(targetDx*targetDx + targetDy*targetDy);
    
    if (dist > 0.1f) {
      targetDx /= dist;
      targetDy /= dist;
    }
    
    // Fuerza de viraje hacia el objetivo
    float steerX = targetDx * predMaxSpeed - predVx;
    float steerY = targetDy * predMaxSpeed - predVy;
    
    // Limitar fuerza
    float steerLen = sqrt(steerX*steerX + steerY*steerY);
    if (steerLen > 0.08f) {
      steerX = (steerX / steerLen) * 0.08f;
      steerY = (steerY / steerLen) * 0.08f;
    }
    
    predVx += steerX;
    predVy += steerY;
  }
  
  // Avanzar depredador y limitar su velocidad
  float pSpd = sqrt(predVx*predVx + predVy*predVy);
  if (pSpd > predMaxSpeed) {
    predVx = (predVx / pSpd) * predMaxSpeed;
    predVy = (predVy / pSpd) * predMaxSpeed;
  }
  predX += predVx;
  predY += predVy;
  
  // Límites del mapa para el depredador (rebote suave)
  if (predX < 2.0f) { predX = 2.0f; predVx *= -1; }
  if (predX > 125.0f) { predX = 125.0f; predVx *= -1; }
  if (predY < 2.0f) { predY = 2.0f; predVy *= -1; }
  if (predY > 52.0f) { predY = 52.0f; predVy *= -1; }

  // Detectar si el depredador se come a su objetivo
  if (minDistSq < 5.0f && targetIdx != -1) {
    // Dibujar destello de impacto en el radar
    display.drawCircle((int16_t)predX, (int16_t)predY, 4, SSD1306_WHITE);
    
    // Reposicionar el boid devorado en un borde del mapa (respawn)
    if (random(0, 2) == 0) {
      boids[targetIdx].x = (random(0, 2) == 0) ? 2.0f : 125.0f;
      boids[targetIdx].y = random(2, 52);
    } else {
      boids[targetIdx].x = random(2, 126);
      boids[targetIdx].y = (random(0, 2) == 0) ? 2.0f : 52.0f;
    }
    float ang = random(0, 360) * 3.14159f / 180.0f;
    float sp = random(8, 13) * 0.1f;
    boids[targetIdx].vx = cos(ang) * sp;
    boids[targetIdx].vy = sin(ang) * sp;
  }

  // 3. Actualizar físicas de cada boid utilizando el Quadtree
  for (int i = 0; i < MAX_BOIDS; i++) {
    // Limpiar resultados anteriores
    queryCount = 0;
    
    // Consultar vecinos a través del Quadtree (súper veloz!)
    queryQuadtree(rootIdx, boids[i].x, boids[i].y, viewRadius);
    
    float avgVx = 0.0f, avgVy = 0.0f; // Alineación
    float avgPx = 0.0f, avgPy = 0.0f; // Cohesión
    float avoidX = 0.0f, avoidY = 0.0f; // Separación
    
    int alignNeighbors = 0;
    int cohNeighbors = 0;
    int sepNeighbors = 0;
    
    for (int k = 0; k < queryCount; k++) {
      int neighborIdx = queryResults[k];
      if (neighborIdx == i) continue;
      
      float dx = boids[i].x - boids[neighborIdx].x;
      float dy = boids[i].y - boids[neighborIdx].y;
      float distSq = dx*dx + dy*dy;
      
      if (distSq > 0.0f) {
        float dist = sqrt(distSq);
        
        // Separación (pesada inversamente a la distancia)
        if (dist < sepRadius) {
          avoidX += dx / dist;
          avoidY += dy / dist;
          sepNeighbors++;
        }
        
        // Cohesión
        avgPx += boids[neighborIdx].x;
        avgPy += boids[neighborIdx].y;
        cohNeighbors++;
        
        // Alineación
        avgVx += boids[neighborIdx].vx;
        avgVy += boids[neighborIdx].vy;
        alignNeighbors++;
      }
    }
    
    // Calcular vectores de dirección (Steering forces)
    float steerSepX = 0.0f, steerSepY = 0.0f;
    float steerCohX = 0.0f, steerCohY = 0.0f;
    float steerAliX = 0.0f, steerAliY = 0.0f;
    
    if (sepNeighbors > 0) {
      avoidX /= sepNeighbors;
      avoidY /= sepNeighbors;
      float avoidLen = sqrt(avoidX*avoidX + avoidY*avoidY);
      if (avoidLen > 0.0f) {
        avoidX = (avoidX / avoidLen) * maxSpeed;
        avoidY = (avoidY / avoidLen) * maxSpeed;
      }
      steerSepX = avoidX - boids[i].vx;
      steerSepY = avoidY - boids[i].vy;
    }
    
    if (cohNeighbors > 0) {
      avgPx /= cohNeighbors;
      avgPy /= cohNeighbors;
      float targetDx = avgPx - boids[i].x;
      float targetDy = avgPy - boids[i].y;
      float targetLen = sqrt(targetDx*targetDx + targetDy*targetDy);
      if (targetLen > 0.0f) {
        targetDx = (targetDx / targetLen) * maxSpeed;
        targetDy = (targetDy / targetLen) * maxSpeed;
      }
      steerCohX = targetDx - boids[i].vx;
      steerCohY = targetDy - boids[i].vy;
    }
    
    if (alignNeighbors > 0) {
      avgVx /= alignNeighbors;
      avgVy /= alignNeighbors;
      float alignLen = sqrt(avgVx*avgVx + avgVy*avgVy);
      if (alignLen > 0.0f) {
        avgVx = (avgVx / alignLen) * maxSpeed;
        avgVy = (avgVy / alignLen) * maxSpeed;
      }
      steerAliX = avgVx - boids[i].vx;
      steerAliY = avgVy - boids[i].vy;
    }
    
    // Fuerza de evasión del depredador (Pánico de escape)
    float steerEscX = 0.0f, steerEscY = 0.0f;
    float dxPred = boids[i].x - predX;
    float dyPred = boids[i].y - predY;
    float distPredSq = dxPred*dxPred + dyPred*dyPred;
    
    if (distPredSq < 484.0f) { // Dentro de 22 píxeles de distancia
      float distPred = sqrt(distPredSq);
      if (distPred > 0.1f) {
        // Empuje fuerte en dirección contraria
        float factor = (22.0f - distPred) * 0.12f;
        steerEscX = (dxPred / distPred) * maxSpeed * factor;
        steerEscY = (dyPred / distPred) * maxSpeed * factor;
      }
    }
    
    // Mezcla ponderada de las fuerzas
    float finalSteerX = steerSepX * 1.6f + steerCohX * 1.0f + steerAliX * 1.1f + steerEscX * 2.2f;
    float finalSteerY = steerSepY * 1.6f + steerCohY * 1.0f + steerAliY * 1.1f + steerEscY * 2.2f;
    
    // Limitar fuerza física de viraje
    float forceLen = sqrt(finalSteerX*finalSteerX + finalSteerY*finalSteerY);
    if (forceLen > maxForce) {
      finalSteerX = (finalSteerX / forceLen) * maxForce;
      finalSteerY = (finalSteerY / forceLen) * maxForce;
    }
    
    // Aplicar fuerza a la velocidad
    boids[i].vx += finalSteerX;
    boids[i].vy += finalSteerY;
    
    // Limitar velocidad del boid
    float speed = sqrt(boids[i].vx*boids[i].vx + boids[i].vy*boids[i].vy);
    if (speed > maxSpeed) {
      boids[i].vx = (boids[i].vx / speed) * maxSpeed;
      boids[i].vy = (boids[i].vy / speed) * maxSpeed;
    }
    
    // Actualizar posición
    boids[i].x += boids[i].vx;
    boids[i].y += boids[i].vy;
    
    // Envolver límites de la pantalla (Screen wrapping)
    if (boids[i].x < 0) boids[i].x += SCR_W;
    if (boids[i].x >= SCR_W) boids[i].x -= SCR_W;
    if (boids[i].y < 0) boids[i].y += 54.0f; // no envolver en el HUD
    if (boids[i].y >= 54.0f) boids[i].y -= 54.0f;
  }

  // ==========================================
  //  4. RENDERIZADO VECTORIAL
  // ==========================================
  
  // Dibujar boids (triángulos isósceles que apuntan a su dirección)
  for (int i = 0; i < MAX_BOIDS; i++) {
    float angle = atan2(boids[i].vy, boids[i].vx);
    
    // Vértice frontal (Punta)
    int16_t x0 = (int16_t)(boids[i].x + cos(angle) * 3.5f);
    int16_t y0 = (int16_t)(boids[i].y + sin(angle) * 3.5f);
    
    // Vértice trasero izquierdo
    int16_t x1 = (int16_t)(boids[i].x + cos(angle + 2.4f) * 2.5f);
    int16_t y1 = (int16_t)(boids[i].y + sin(angle + 2.4f) * 2.5f);
    
    // Vértice trasero derecho
    int16_t x2 = (int16_t)(boids[i].x + cos(angle - 2.4f) * 2.5f);
    int16_t y2 = (int16_t)(boids[i].y + sin(angle - 2.4f) * 2.5f);
    
    display.drawTriangle(x0, y0, x1, y1, x2, y2, SSD1306_WHITE);
  }

  // Dibujar Depredador (Triángulo relleno y más grande)
  float pAngle = atan2(predVy, predVx);
  int16_t px0 = (int16_t)(predX + cos(pAngle) * 6.5f);
  int16_t py0 = (int16_t)(predY + sin(pAngle) * 6.5f);
  int16_t px1 = (int16_t)(predX + cos(pAngle + 2.5f) * 4.5f);
  int16_t py1 = (int16_t)(predY + sin(pAngle + 2.5f) * 4.5f);
  int16_t px2 = (int16_t)(predX + cos(pAngle - 2.5f) * 4.5f);
  int16_t py2 = (int16_t)(predY + sin(pAngle - 2.5f) * 4.5f);
  
  display.fillTriangle(px0, py0, px1, py1, px2, py2, SSD1306_WHITE);

  // ==========================================
  //  5. BARRA DE RENDIMIENTO Y TELEMETRÍA (HUD)
  // ==========================================
  
  // Línea divisoria
  display.drawFastHLine(0, 54, SCR_W, SSD1306_WHITE);
  display.fillRect(0, 55, SCR_W, 9, SSD1306_BLACK);
  
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  // Mostrar FPS reales
  display.setCursor(2, 56);
  display.print(F("FPS:"));
  display.print(displayFps);
  
  // Mostrar cantidad de boids
  display.setCursor(46, 56);
  display.print(F("N:"));
  display.print(MAX_BOIDS);
  
  // Mostrar cantidad de nodos Quadtree generados
  display.setCursor(78, 56);
  display.print(F("QT_NODES:"));
  display.print(qtNodeCount);

  display.display();
  
  // Cálculo de FPS
  frameCounter++;
  uint32_t now = millis();
  if (now - lastFPSUpdate >= 1000) {
    displayFps = frameCounter;
    frameCounter = 0;
    lastFPSUpdate = now;
  }
}
