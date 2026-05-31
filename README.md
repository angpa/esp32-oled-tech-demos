# ESP32 OLED Tech Demos

Este repositorio contiene una colección de proyectos avanzados y demostraciones técnicas diseñadas para exprimir al máximo las capacidades de un microcontrolador **ESP32** junto con una pantalla **OLED I2C de 128x64 píxeles** (bicolor).

## 🚀 Proyectos Incluidos

### Juegos y Animación
- 🌌 **space_shooter**: El clásico juego de navecitas espaciales con partículas dinámicas y fondos optimizados.
- 🥷 **stick_fight**: Motor de animación por keyframes de peleas de muñecos de palo (estilo Xiaoxiao) con coreografía programada.

### Gráficos 3D y Demoscene
- 🔫 **doom_raycaster**: Motor 3D en primera persona usando la técnica matemática de Raycasting (estilo Wolfenstein 3D).
- 🍩 **donut_3d**: Renderizado matemático en tiempo real de un toroide giratorio en 3D (`donut.c`) con cálculo de falsas sombras.
- ⛰️ **terrain_3d**: Vuelo infinito sobre un terreno de montañas estilo Synthwave, generado proceduralmente con Ruido de Perlin.
- 💻 **matrix_3d**: Generación de un túnel cilíndrico en 3D en perspectiva con los caracteres descendentes de The Matrix.

### Física y Simulaciones
- ⏳ **falling_sand**: Simulación de física de partículas 2D de arena cayendo y reaccionando a colisiones (Autómatas Celulares).
- 🧠 **mandelbrot_dualcore**: Renderizador del fractal de Mandelbrot que exprime **ambos núcleos** del ESP32 simultáneamente mediante FreeRTOS, duplicando la velocidad.

### Redes e IoT (Wi-Fi y Bluetooth)
- 📡 **radar_ble**: Escáner de dispositivos Bluetooth Low Energy (BLE) utilizando doble núcleo para escanear y dibujar el radar en tiempo real.
- 🌐 **web_canvas**: El ESP32 funciona como Servidor Web y Punto de Acceso (AP). Dibuja desde el navegador de tu celular y míralo aparecer en el OLED al instante.
- 📈 **crypto_ticker**: Monitor del precio de Bitcoin en vivo consumiendo la API JSON pública de Binance vía cliente Wi-Fi.

## 🛠️ Hardware Necesario
- Placa de desarrollo **ESP32** (NodeMCU, WROOM, etc).
- Pantalla **OLED SSD1306 (128x64)** conectada por I2C (SDA, SCL).

## 📥 Instalación
Abre cualquiera de las carpetas y carga el archivo `.ino` correspondiente utilizando el **Arduino IDE**. Asegúrate de tener instaladas las siguientes librerías desde el Gestor de Librerías:
- `Adafruit_SSD1306`
- `Adafruit_GFX`
