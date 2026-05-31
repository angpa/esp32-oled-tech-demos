# ESP32 OLED Tech Demos

Este repositorio contiene una colección de proyectos avanzados y demostraciones técnicas diseñadas para exprimir al máximo las capacidades de un microcontrolador **ESP32** junto con una pantalla **OLED I2C de 128x64 píxeles** (bicolor).

## 🚀 Proyectos Incluidos

### Juegos y Animación
- 🌌 **space_shooter**: El clásico juego de navecitas espaciales con partículas dinámicas y fondos optimizados.
- 🥷 **stick_fight**: Motor de animación por keyframes de peleas de muñecos de palo (estilo Xiaoxiao) con coreografía programada.
- 🏎️ **retro_racer**: Juego de carreras pseudo-3D estilo OutRun/Pole Position con pista procedural (curvas, colinas), rumble strips, tráfico, árboles laterales y controles por sensor táctil capacitivo (GPIO 4 y 15).

### Gráficos 3D y Demoscene
- 🧭 **doom_raycaster**: Labyrinth Explorer. Motor 3D en primera persona con rejilla de perspectiva 3D Holodeck, skybox rotatorio, ventanas transparentes y autopiloto predictivo inteligente.
- 🍩 **donut_3d**: Renderizado matemático en tiempo real de un toroide giratorio en 3D (`donut.c`) con cálculo de falsas sombras.
- ⛰️ **terrain_3d**: Simulador de vuelo 3D continuo sobre un valle infinito con montañas sólidas (Painter's Algorithm), estrellas, sol retro, ladeo de nave (banking) y HUD táctico.
- 💻 **matrix_3d**: Generación de un túnel cilíndrico en 3D en perspectiva con los caracteres descendentes de The Matrix.
- 🎹 **chiptune_synth**: Generador de sonido retro de 3 canales (16kHz) sobre el DAC nativo, con analizador FFT en tiempo real y terreno 3D reactivo a las frecuencias de la música.
- 🌀 **raymarching_3d**: Renderizador de píxeles por Raymarching 3D en doble núcleo con metaballs líquidas (smooth blend), optimización Bounding Sphere y sombreado retro dithered de 1 bit.


### Física y Simulaciones
- ⏳ **falling_sand**: Simulación de física de partículas 2D de arena cayendo y reaccionando a colisiones (Autómatas Celulares).
- 🧠 **mandelbrot_dualcore**: Renderizador del fractal de Mandelbrot que exprime **ambos núcleos** del ESP32 simultáneamente mediante FreeRTOS, duplicando la velocidad.
- 🦅 **flocking_boids**: Simulación de inteligencia artificial colectiva y comportamiento de bandada (120 boids + depredador) acelerada mediante un árbol Quadtree estático.
- 🪢 **verlet_physics**: Caja de arena física (Verlet Integration) interactiva con péndulo, cubo de gelatina deformable, puente y controles por sensor táctil capacitivo nativo (GPIO 4 y 15).



### Redes e IoT (Wi-Fi y Bluetooth)
- 📡 **omni_scanner**: Escáner de Espectro dual (Wi-Fi + BLE). Atrapa redes ocultas y dispositivos Bluetooth en el aire y los despliega en una interfaz cinematográfica.
- 📡 **radar_ble**: Escáner de dispositivos Bluetooth Low Energy (BLE) utilizando doble núcleo.
- 🌐 **web_canvas**: El ESP32 funciona como Servidor Web y Punto de Acceso (AP). Dibuja desde el navegador de tu celular y míralo aparecer en el OLED al instante.
- 📈 **crypto_ticker**: Monitor del precio de Bitcoin en vivo consumiendo la API JSON pública de Binance vía cliente Wi-Fi.

### Arte y Meditación
- 📿 **buddha_wisdom**: Exposición minimalista y solemne de textos del Canon Tibetano (Prajñāpāramitā). Emplea atenuación por hardware puro para que las palabras aparezcan y se desvanezcan en un vacío absoluto.

## 🛠️ Hardware Necesario
- Placa de desarrollo **ESP32** (NodeMCU, WROOM, etc).
- Pantalla **OLED SSD1306 (128x64)** conectada por I2C (SDA, SCL).

## 📥 Instalación
Abre cualquiera de las carpetas y carga el archivo `.ino` correspondiente utilizando el **Arduino IDE**. Asegúrate de tener instaladas las siguientes librerías desde el Gestor de Librerías:
- `Adafruit_SSD1306`
- `Adafruit_GFX`
