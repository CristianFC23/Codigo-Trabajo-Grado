/* =========================
  Importar librerias
========================== */

#include <WiFi.h>//OTA
#include <AsyncTCP.h> //OTA
#include <ESPAsyncWebServer.h>//OTA
#include <Update.h>//OTA
#include <Adafruit_INA219.h>//Sensor INA219
#include <Adafruit_NeoPixel.h> //Leds
#include "esp_sleep.h" //Sleep
#include "driver/gpio.h" //Sleep
#include "driver/rtc_io.h" //Sleep, necesario para el pull-up en dominio RTC (deep sleep)
#include <Adafruit_GFX.h>     //Pantalla OLED
#include <Adafruit_SSD1306.h> //Pantalla OLED

/* =========================
  OTA
========================== */

//Constantes con los datos del servidor
const char* ap_ssid     = "ESP32-Mantenimiento";
const char* ap_password = "clave1234";

//Crea el objeto servidor en el puerto 80 (HTTP estándar).
AsyncWebServer server(80); 

unsigned long ultimoClienteVisto = 0; 
const unsigned long TIMEOUT_SERVIDOR = 1UL * 60UL * 1000UL; // cantidad de minutos * 60 segundos * 1000 milisegundos.

//========================= Diseño de la pagina web =========================

//Constante que contendra el diseño de la pagina web, debe ir entre R"rawliteral()rawliteral";
//PROGMEM hace que no se compile hasta que se necesite, ahorra RAM
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32-S3</title>

    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: sans-serif;
            background: #0f0f0f;
            color: #f0f0f0;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 2rem;
        }
        /* CONTENEDOR GENERAL */
        .container {
            display: flex;
            gap: 2rem;
            flex-wrap: wrap;
            justify-content: center;
            align-items: flex-start;
        }
        /* TARJETAS */
        .card {
            background: #1a1a1a;
            border: 1px solid #2a2a2a;
            border-radius: 16px;
            padding: 2rem 2.5rem;
            width: 340px;
        }
        h1 {
            font-size: 1.2rem;
            font-weight: 500;
            margin-bottom: 1.5rem;
            text-align: center;
        }
        /* =========================
        TARJETA INFO ESP32
        ========================== */

        .dot {
            width: 11px;
            height: 11px;
            border-radius: 50%;
            display: inline-block;
            margin-right: 8px;
            animation: pulse 2s infinite;
        }
        .dot.online {
            background: #22c55e;
        }
        .dot.update {
            background: #f59e0b;
        }

        @keyframes pulse {
            0%,
            100% {
                opacity: 1;
            }
            50% {
                opacity: 0.3;
            }
        }
        .info {
            background: #111;
            border-radius: 10px;
            padding: 0.85rem 1rem;
            margin-bottom: 0.75rem;
            text-align: left;
        }
        .info p {
            font-size: 0.75rem;
            color: #666;
            margin-bottom: 3px;
        }
        .info span {
            font-size: 0.95rem;
            font-weight: 500;
        }
        

        /* =========================
        BOTONES
        ========================== */

        .btn {
            display: block;
            width: 100%;
            text-align: center;
            color: white;
            text-decoration: none;
            padding: 0.75rem;
            border-radius: 10px;
            font-size: 0.95rem;
            margin-top: 1.25rem;
            border: none;
            cursor: pointer;
            transition: 0.2s;
        }
        .btn.active {
            background: #f59e0b;
        }
        .btn.active:hover {
            background: #d97706;
        }
        .btn.disabled {
            background: #1f1f1f;
            color: #444;
            cursor: not-allowed;
            border: 1px solid #2a2a2a;
        }
        .btn:disabled {
            background: #3a2e1a;
            color: #555;
            cursor: not-allowed;
        }

        /* =========================
        UPLOAD
        ========================== */

        .upload-text {
            font-size: 0.85rem;
            color: #666;
            margin-bottom: 1.5rem;
            text-align: center;
        }
        .file-label {
            display: block;
            background: #111;
            border: 2px dashed #333;
            border-radius: 10px;
            padding: 1.8rem 1rem;
            cursor: pointer;
            margin-bottom: 1rem;
            font-size: 0.9rem;
            color: #888;
            transition: 0.2s;
            text-align: center;
        }
        .file-label:hover {
            border-color: #f59e0b;
            color: #f0f0f0;
        }
        .file-label.dragover {
            border-color: #f59e0b;
            background: #22190a;
            color: #f0f0f0;
        }
        #file-input {
            display: none;
        }
        #file-name {
            font-size: 0.8rem;
            color: #22c55e;
            margin-bottom: 1rem;
            min-height: 1.2rem;
            text-align: center;
            word-break: break-word;
        }

        /* =========================
        PROGRESS BAR
        ========================== */

        .progress-wrap {
            background: #111;
            border-radius: 99px;
            height: 8px;
            margin: 1.25rem 0 0.5rem;
            overflow: hidden;
            display: none;
        }
        .progress-bar {
            height: 100%;
            width: 0%;
            background: #f59e0b;
            border-radius: 99px;
            transition: width 0.3s;
        }
        #status {
            font-size: 0.8rem;
            color: #888;
            margin-top: 0.5rem;
            min-height: 1.2rem;
            text-align: center;
        }
        /* RESPONSIVE */
        @media(max-width:800px) {
            .container {
                flex-direction: column;
                align-items: center;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- =========================
            PANEL ESTADO ESP32
        ========================== -->
        <div class="card">
            <h1>
                <span class="dot" id="dot"></span>
                ESP32-S3
            </h1>
            <div class="info">
                <p>IP del dispositivo</p>
                <span>192.168.4.1</span>
            </div>
            <div class="info">
                <p>Red WiFi</p>
                <span id="ap">--</span>
            </div>
            <div class="info">
                <p>Clientes conectados</p>
                <span id="clients">--</span>
            </div>
            <div class="info">
                <p>Uptime</p>
                <span id="up">--</span>
            </div>
        </div>

        <!-- =========================
            PANEL UPDATE
        ========================== -->
        <div class="card">
            <h1>Actualizar firmware</h1>
            <p class="upload-text">
                Selecciona o arrastra un archivo .bin
            </p>
            <label class="file-label" id="drop-zone" for="file-input">
                Haz clic o arrastra el firmware aquí
            </label>
            <input type="file" id="file-input" accept=".bin">
            <div id="file-name"></div>
            <button class="btn active" id="btn-upload" disabled onclick="startUpload()">
                Subir firmware
            </button>
            <div class="progress-wrap" id="progress-wrap">
                <div class="progress-bar" id="progress-bar"></div>
            </div>
            <div id="status"></div>
        </div>
    </div>
    <script>
        /* =========================
            PANEL INFO ESP32
            ========================== */

        function fmt(ms) {
            let s = Math.floor(ms / 1000);
            let m = Math.floor(s / 60);
            s %= 60;
            let h = Math.floor(m / 60);
            m %= 60;
            return h + 'h ' + m + 'm ' + s + 's';
        }
        function refresh() {
            fetch('/stats')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('up').textContent =
                        fmt(d.uptime);
                    document.getElementById('ap').textContent =
                        d.ap;
                    document.getElementById('clients').textContent =
                        d.clients;
                    const dot = document.getElementById('dot');
                    if (d.update_enabled) {
                        dot.className = 'dot update';
                    }
                    else {
                        dot.className = 'dot online';
                    }
                });
        }
        refresh();
        setInterval(refresh, 2000);
        /* =========================
            UPLOAD FIRMWARE
            ========================== */

        const fileInput = document.getElementById('file-input');
        const btnUpload = document.getElementById('btn-upload');
        const fileName = document.getElementById('file-name');
        const progressW = document.getElementById('progress-wrap');
        const progressB = document.getElementById('progress-bar');
        const statusEl = document.getElementById('status');
        const dropZone = document.getElementById('drop-zone');

        /* ---------- Selección normal ---------- */

        fileInput.addEventListener('change', () => {
            if (fileInput.files.length > 0) {
                const file = fileInput.files[0];
                fileName.textContent = file.name;
                btnUpload.disabled = false;
            }
        });

        /* =========================
            DRAG & DROP
            ========================== */

        dropZone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropZone.classList.add('dragover');
        });
        dropZone.addEventListener('dragleave', () => {
            dropZone.classList.remove('dragover');
        });
        dropZone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropZone.classList.remove('dragover');
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                const file = files[0];
                /* validar extension */
                if (!file.name.endsWith('.bin')) {
                    statusEl.style.color = '#ff6666';
                    statusEl.textContent =
                        'Solo se permiten archivos .bin';
                    return;
                }
                fileInput.files = files;
                fileName.textContent = file.name;
                btnUpload.disabled = false;
                statusEl.textContent = '';
            }
        });

        /* =========================
            SUBIR FIRMWARE
        ========================== */

        function startUpload() {
            const file = fileInput.files[0];
            if (!file) return;
            const xhr = new XMLHttpRequest();
            const formData = new FormData();
            formData.append('firmware', file);
            btnUpload.disabled = true;
            progressW.style.display = 'block';
            statusEl.style.color = '#888';
            statusEl.textContent = 'Subiendo...';
            xhr.upload.onprogress = (e) => {
                if (e.lengthComputable) {
                    const pct =
                        Math.round((e.loaded / e.total) * 100);
                    progressB.style.width = pct + '%';
                    statusEl.textContent =
                        'Subiendo... ' + pct + '%';
                }
            };
            xhr.onload = () => {
                if (xhr.status === 200) {
                    progressB.style.width = '100%';
                    progressB.style.background = '#22c55e';
                    let cuenta = 5;
                    statusEl.style.color = '#22c55e';
                    statusEl.textContent =
                        '✓ Firmware actualizado. Reiniciando en '
                        + cuenta + 's...';
                    const intervalo = setInterval(() => {
                        cuenta--;
                        if (cuenta > 0) {
                            statusEl.textContent =
                                '✓ Firmware actualizado. Reiniciando en '
                                + cuenta + 's...';
                        }
                        else {
                            clearInterval(intervalo);
                            statusEl.style.color = '#888';
                            statusEl.textContent =
                                'Reiniciando... vuelve a conectarte en unos segundos.';
                        }
                    }, 1000);
                }
                else {
                    statusEl.style.color = '#ff6666';
                    statusEl.textContent =
                        'Error al actualizar.';
                    btnUpload.disabled = false;
                }
            };
            xhr.onerror = () => {
                statusEl.style.color = '#ff6666';
                statusEl.textContent =
                    'Error de conexión.';
                btnUpload.disabled = false;
            };
            xhr.open('POST', '/doUpdate');
            xhr.send(formData);
        }
    </script>
</body>
</html>
)rawliteral";

/* =========================
  Maquina de Estados
========================== */

enum EstadoSistema {
  ESTADO_SERVIDOR_OTA,     // Servidor web activo, esperando conexión para reprogramar
  ESTADO_PROGRAMANDO,      // Hay un cliente conectado: se pausan las demás tareas
  ESTADO_OPERACION_NORMAL,  // Servidor apagado, ESP32 corriendo sus otras tareas
  ESTADO_LIGHT
};
EstadoSistema estadoActual;

/* =========================
  INA219
========================== */

const int I2C_SDA = 11;
const int I2C_SCL = 12;
Adafruit_INA219 ina219;

unsigned long previousAdcMillis = 0;
const int adcInterval = 200;
float lastCurrentReading_ina = 0.0;
float lastVoltageReading_ina = 0.0;

//========================= Protección por sobrecorriente
const float CORRIENTE_LIMITE_mA = 500.0; 
bool sobreCorriente = false;

//========================= Batería (LiPo 2S)
const float BATERIA_VOLTAJE_MIN = 6.0;  // 0%  (vacía)
const float BATERIA_VOLTAJE_MAX = 8.4;  // 100% (llena)

float porcentajeBateria = 100.0;

const float BATERIA_UMBRAL_20 = 20.0;
const float BATERIA_UMBRAL_10 = 10.0;
const float BATERIA_UMBRAL_CRITICO = 5.0;

//========================= Prototipo de función 
void ReadCurrentSensor(); 

/* =========================
  Control de motores
========================== */

//========================= Pines de los botones
const int BOTON_HORARIO = 9;
const int BOTON_ANTIHORARIO = 10;
const int BOTON_TERCERO = 13; 

//========================= Pin de habilitación compartido (nSLEEP) de los 3 DRV8833
const int DRV_ENABLE = 7;

//========================= Pines de dirección/PWM de cada motor
const int MOTOR_PIN_1 = 5;
const int MOTOR_PIN_2 = 6;

//========================= Parametros PWM
const int PWM_FREQ = 10000;  // Rango optimo 5 - 20 KHz
const int PWM_RES = 8;       // 8 bits (0-255)

//========================= Variables motor
int VELOCIDAD = 200; //Duty
bool motorEnMarcha = false;

// Dirección real del motor en este instante, la actualizan directamente
// girarHorario()/girarAntihorario()/detenerMotor(), sin importar si el
// movimiento vino del modo temporizado o del modo manual (boton3).
// 1 = horario, -1 = antihorario, 0 = detenido.
int direccionMotorActual = 0;

//========================= Movimiento temporizado (boton horario/antihorario solos)
const unsigned long DURACION_MOVIMIENTO_MS = 1600; // cuánto dura el giro al presionar horario/antihorario solos
bool motorEnMovimientoTemporizado = false;
unsigned long tiempoInicioMovimiento = 0;
int direccionMovimientoTemporizado = 0; // 1 = horario, -1 = antihorario, 0 = ninguno

//========================= Prototipo de función 
void detenerMotor();
void ControlMotoresPorBotones();
void girarHorario();
void girarAntihorario();

//========================= Debounce de botones
struct BotonDebounce {
  uint8_t pin;
  bool estadoEstable;      // true = presionado, ya filtrado
  bool ultimaLecturaBruta; // última lectura cruda del pin, sin filtrar
  unsigned long ultimoCambioMillis;
};

const unsigned long DEBOUNCE_MS = 30; // tiempo que debe mantenerse estable la lectura

BotonDebounce botonHorario     = { BOTON_HORARIO,     false, false, 0 };
BotonDebounce botonAntihorario = { BOTON_ANTIHORARIO, false, false, 0 };
BotonDebounce botonTercero     = { BOTON_TERCERO,     false, false, 0 };

bool ActualizarBoton(BotonDebounce &b); //Lee un botón con filtro anti-rebote y devuelve su estado estable

/* =========================
  Sleep
========================== */
//========================= Máscara de bits con ambos pines, usada para ext1_wakeup
#define MASCARA_BOTONES ((1ULL << BOTON_HORARIO) | (1ULL << BOTON_ANTIHORARIO) | (1ULL << BOTON_TERCERO))

//========================= Variables de tiempo para despertar
unsigned long tiempoUltimaActividad = 0;
const unsigned long TIEMPO_INACTIVIDAD_PARA_LIGHT_SLEEP = 10000;          // 10s
#define TIEMPO_CHEQUEO_BATERIA_US   (30ULL * 60ULL * 1000000ULL)  // 30 min
#define TIEMPO_ALERTA_BATERIA_US    (5ULL  * 60ULL * 1000000ULL)  // 5 min

/* =========================
  LED
========================== */

#define LED_PIN 48
#define NUM_LEDS 1
#define BRIGHTNESS 50

Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Control del LED rojo temporizado ---
bool ledRojoActivo = false;
unsigned long ledRojoMillis = 0;
const unsigned long ledRojoDuration = 1000; // 1 segundo

void ActualizarLED(); //Decide y aplica el color del LED según el estado actual

/* =========================
  Setup
========================== */

void setup() {
  Serial.begin(115200); 

/* =========================
    Configuración Servidor
========================== */

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

//========================= Informativo, borrar luego
  Serial.println("Access Point levantado");
  Serial.print("Nombre de red: ");
  Serial.println(ap_ssid);
  Serial.print("Contrasena:    ");
  Serial.println(ap_password);
  Serial.print("IP:            ");
  Serial.println(WiFi.softAPIP()); // siempre 192.168.4.1

//========================= Configuración de la ruta web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html",INDEX_HTML
    );
  });

//========================= Stats JSON
  server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"uptime\":"  + String(millis()) + ",";
    json += "\"ap\":\""    + String(ap_ssid) + "\",";
    json += "\"clients\":" + String(WiFi.softAPgetStationNum());
    json += "}";
    request->send(200, "application/json", json);
  });

//========================= Función para actualizar Firmware
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      bool ok = !Update.hasError();
      AsyncWebServerResponse *r = request->beginResponse(
        200, "text/plain", ok ? "OK" : "FALLO"
      );
      r->addHeader("Connection", "close");
      request->send(r);
      delay(5000);
      ESP.restart();  
    },
    [](AsyncWebServerRequest *request, String filename,
       size_t index, uint8_t *data, size_t len, bool final) {
      if (!index)  Update.begin(UPDATE_SIZE_UNKNOWN);
      Update.write(data, len);
      if (final)   Update.end(true);
    }
  );

//========================= Informativo, borrar luego
  server.begin(); //Inicio del servidor
  Serial.println("OTA listo en http://192.168.4.1/");

//========================= Arranca el conteo de inactividad del servidor OTA
  ultimoClienteVisto = millis();

/* =========================
  Configuración Sensor INA219
========================== */

  Wire.begin(I2C_SDA, I2C_SCL);
//========================= Informativo, borrar luego  
  if (!ina219.begin()) {
    Serial.println("No se encontró el INA219, continuando sin sensor...");
  }

/* =========================
  Configuración Motores
========================== */

//========================= Botones con resistencia pull-up interna
  pinMode(BOTON_HORARIO, INPUT_PULLUP);
  pinMode(BOTON_ANTIHORARIO, INPUT_PULLUP);
  pinMode(BOTON_TERCERO, INPUT_PULLUP);

//========================= Pin de habilitación (nSLEEP) de los DRV8833
  pinMode(DRV_ENABLE, OUTPUT);
  digitalWrite(DRV_ENABLE, LOW); // arrancan en reposo hasta que se necesite girar

//=========================Pines del motor como salida
  pinMode(MOTOR_PIN_1, OUTPUT);
  pinMode(MOTOR_PIN_2, OUTPUT);

//========================= Configuración PWM para ESP32-S3 con Core ESP 3.X
  ledcAttach(MOTOR_PIN_1, PWM_FREQ, PWM_RES);
  ledcAttach(MOTOR_PIN_2, PWM_FREQ, PWM_RES);

  detenerMotor(); //Detener motores en caso de reinicio o inicio anomalo

/* =========================
  Configuración Sleep
========================== */

  esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();// Decide en qué estado arranca la máquina de estados

  if (causa == ESP_SLEEP_WAKEUP_EXT1 || causa == ESP_SLEEP_WAKEUP_GPIO) {
    //Si llega despues de despertar del deep sleep pasa directo al funcionamiento normal sin encender el servidor
    estadoActual = ESTADO_OPERACION_NORMAL;

    server.end();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    //========================= Informativo, borrar luego
    Serial.println("[BOOT] Arranque despues de deep sleep");
  } 
  else {
    estadoActual = ESTADO_SERVIDOR_OTA;
    //========================= Informativo, borrar luego
    Serial.println("[BOOT] Arranque normal");
  }

  tiempoUltimaActividad = millis();

/* =========================
  LED
========================== */

  led.begin();
  led.setBrightness(BRIGHTNESS);
  
}

/* =========================
  Bucle
========================== */

void loop() {

  unsigned long ahora = millis();

  switch (estadoActual) {

    //=========================================================
    // ESTADO 1: Servidor OTA activo, esperando conexión
    //=========================================================

    case ESTADO_SERVIDOR_OTA: {

      ReadCurrentSensor();
      ControlMotoresPorBotones();
      ActualizarLED();
      
      if (WiFi.softAPgetStationNum() > 0) {
        // Hay al menos un dispositivo conectado a la red.
        // Pasamos a modo "solo programación" para no correr
        // ninguna otra tarea mientras alguien puede estar
        // subiendo firmware.
        ultimoClienteVisto = millis();

        //========================= Informativo, borrar luego
        Serial.println("Cliente conectado. Pasando a modo solo programación.");

        detenerMotor();
        estadoActual = ESTADO_PROGRAMANDO;
      }
      else if (millis() - ultimoClienteVisto >= TIMEOUT_SERVIDOR) {
        // Pasaron X minutos sin ningún dispositivo conectado
        // -> apagamos el servidor y el AP, y cambiamos de estado.
        server.end();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);

        //========================= Informativo, borrar luego
        Serial.println("Sin dispositivos conectados durante x min.");
        Serial.println("Servidor OTA apagado. Pasando a operación normal.");

        tiempoUltimaActividad = millis();
        estadoActual = ESTADO_OPERACION_NORMAL;
      }

      break;
    }

    //=========================================================
    // ESTADO 2: Solo programación, hay un cliente conectado
    //=========================================================

    case ESTADO_PROGRAMANDO: {

      // Mientras haya al menos un dispositivo conectado, no se
      // ejecuta ninguna otra tarea: solo se atiende el servidor
      // web (AsyncWebServer responde en segundo plano mediante
      // sus propios callbacks, no bloquea este loop).
      // El LED y la pantalla sí se actualizan, para que quede
      // claro que el sistema está en modo programación.

      ActualizarLED();

      if (WiFi.softAPgetStationNum() == 0) {
        // El cliente se desconectó sin completar una actualización
        // (si hubiera completado, el ESP.restart() del /doUpdate
        // ya habría reiniciado el equipo). Volvemos a esperar,
        // reiniciando el contador de inactividad.
        ultimoClienteVisto = millis();

        //========================= Informativo, borrar luego
        Serial.println("Cliente desconectado. Volviendo a esperar conexión.");

        estadoActual = ESTADO_SERVIDOR_OTA;
      }

      break;
    }

    //=========================================================
    // ESTADO 3: Operación normal, servidor OTA ya apagado
    //=========================================================

    case ESTADO_OPERACION_NORMAL: {

      // Acá van las demás tareas del ESP32, las que se necesiten
      // correr una vez que el servidor de reprogramación ya no
      // está activo 

      ReadCurrentSensor();
      ControlMotoresPorBotones();
      ActualizarLED();

      bool presionado1 = botonHorario.estadoEstable;
      bool presionado2 = botonAntihorario.estadoEstable;
      bool presionado3 = botonTercero.estadoEstable;

      if (presionado1 || presionado2 || presionado3) {
        tiempoUltimaActividad = ahora;   // cualquiera de los dos resetea la inactividad
      }

      if (ahora - tiempoUltimaActividad >= TIEMPO_INACTIVIDAD_PARA_LIGHT_SLEEP) {
        estadoActual = ESTADO_LIGHT;
      }

      break;
    }

    case ESTADO_LIGHT: {

      Serial.println("Sin actividad por 10s -> entrando en light sleep...");
      Serial.flush();

      // Apagar LED y pantalla antes de dormir, para no gastar energía
      led.clear();
      led.show();

      // Fuente de despertar 1: ambos botones (GPIO normal, válido en light sleep)
      gpio_wakeup_enable((gpio_num_t)BOTON_HORARIO, GPIO_INTR_LOW_LEVEL);
      gpio_wakeup_enable((gpio_num_t)BOTON_ANTIHORARIO, GPIO_INTR_LOW_LEVEL);
      gpio_wakeup_enable((gpio_num_t)BOTON_TERCERO, GPIO_INTR_LOW_LEVEL);
      esp_sleep_enable_gpio_wakeup();

      // Fuente de despertar 2: temporizador, cuya duración depende
      // de la última lectura de batería que tengamos.
      uint64_t tiempoSleepUs = (porcentajeBateria <= BATERIA_UMBRAL_20)
                                  ? TIEMPO_ALERTA_BATERIA_US
                                  : TIEMPO_CHEQUEO_BATERIA_US;

      esp_sleep_enable_timer_wakeup(tiempoSleepUs);

      esp_light_sleep_start();
      // ---- El código continúa AQUÍ al despertar (light sleep no reinicia) ----

      esp_sleep_wakeup_cause_t causaLight = esp_sleep_get_wakeup_cause();
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);

      if (causaLight == ESP_SLEEP_WAKEUP_GPIO) {
        // El GPIO sigue vivo (no hubo reinicio), así que podemos leer
        // directamente cuál botón sigue/estuvo presionado.
        bool boton1 = (digitalRead(BOTON_HORARIO) == LOW);
        bool boton2 = (digitalRead(BOTON_ANTIHORARIO) == LOW);
        bool boton3 = (digitalRead(BOTON_TERCERO) == LOW);

        //========================= Informativo, borrar luego
        Serial.print("Desperté del light sleep. BOTON_1=");
        Serial.print(boton1);
        Serial.print(" BOTON_2=");
        Serial.print(boton2);
        Serial.print(" BOTON_3=");
        Serial.println(boton3);

        tiempoUltimaActividad = millis();
        Serial.flush();
        estadoActual = ESTADO_OPERACION_NORMAL;
      } else {
        // ---- Despertó por temporizador: chequeo de batería ----
        Serial.println("Desperté por temporizador -> chequeando batería...");

        float voltaje_ina = ina219.getBusVoltage_V();
        lastVoltageReading_ina = voltaje_ina;

        porcentajeBateria = (voltaje_ina - BATERIA_VOLTAJE_MIN) /
                             (BATERIA_VOLTAJE_MAX - BATERIA_VOLTAJE_MIN) * 100.0;
        porcentajeBateria = constrain(porcentajeBateria, 0.0, 100.0);

        Serial.printf("Chequeo periódico -> V: %.2f  Bateria: %.1f%%\n",
                       voltaje_ina, porcentajeBateria);

        if (porcentajeBateria <= BATERIA_UMBRAL_20) {

          Serial.println("Batería baja -> alerta LED");

          // Parpadeo de alerta ~2s. Se sale antes si se pulsa un botón,
          // para no ignorar al usuario mientras alertamos.
          for (int i = 0; i < 4; i++) {
            led.setPixelColor(0, led.Color(255, 0, 0));
            led.show();
            delay(250);
            led.setPixelColor(0, led.Color(0, 0, 0));
            led.show();
            delay(250);

            if (digitalRead(BOTON_HORARIO) == LOW ||
                digitalRead(BOTON_ANTIHORARIO) == LOW ||
                digitalRead(BOTON_TERCERO) == LOW) {
              tiempoUltimaActividad = millis();
              estadoActual = ESTADO_OPERACION_NORMAL;
              break;
            }
          }
        }

        // Si no se detectó botón, estadoActual sigue en ESTADO_LIGHT.
        // En la próxima vuelta del switch se vuelve a entrar aquí y
        // se recalcula tiempoSleepUs con el porcentajeBateria actualizado.
        Serial.flush();
      }

      break;
    }
  }
}

/* =========================
  Funciones
========================== */

//========================= Debounce de botones
bool ActualizarBoton(BotonDebounce &b) {

  bool lecturaActual = (digitalRead(b.pin) == LOW); // true = presionado

  if (lecturaActual != b.ultimaLecturaBruta) {
    // el pin cambió "en crudo": reinicia el temporizador de debounce
    b.ultimaLecturaBruta = lecturaActual;
    b.ultimoCambioMillis = millis();
  }

  if (millis() - b.ultimoCambioMillis >= DEBOUNCE_MS) {
    // el nivel se mantuvo estable el tiempo suficiente: se acepta
    b.estadoEstable = b.ultimaLecturaBruta;
  }

  return b.estadoEstable;
}

//========================= Sensor de corriente 
void ReadCurrentSensor() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousAdcMillis >= adcInterval) {

    previousAdcMillis = currentMillis;

    // ─── Lectura: INA219 (I2C) ─────────────────────────────────────────
    float voltaje_ina = ina219.getBusVoltage_V();
    float corriente_ina_mA = ina219.getCurrent_mA();

    lastVoltageReading_ina = voltaje_ina;
    lastCurrentReading_ina = corriente_ina_mA / 1000.0;

    // ─── Porcentaje de batería (aproximación lineal por voltaje) ──────
    porcentajeBateria = (voltaje_ina - BATERIA_VOLTAJE_MIN) /
                         (BATERIA_VOLTAJE_MAX - BATERIA_VOLTAJE_MIN) * 100.0;
    porcentajeBateria = constrain(porcentajeBateria, 0.0, 100.0);

    Serial.println("INA");
    Serial.printf("V: %.2f  I: %.2f mA\n", voltaje_ina, corriente_ina_mA);

        //========================= Protección por sobrecorriente
    if (corriente_ina_mA >= CORRIENTE_LIMITE_mA) {

      if (!sobreCorriente) {
        //========================= Informativo, borrar luego
        Serial.print("¡SOBRECORRIENTE! I = ");
        Serial.print(corriente_ina_mA);
        Serial.println(" mA -> Deteniendo motor.");

        // Dispara la alerta visual roja de 1 segundo
        ledRojoActivo = true;
        ledRojoMillis = millis();
      }

      sobreCorriente = true;

      // Corte inmediato, sin esperar a la próxima vuelta de ControlMotoresPorBotones()
      detenerMotor();
      motorEnMarcha = false;

    } else {

      if (sobreCorriente) {
        //========================= Informativo, borrar luego
        Serial.println("Corriente normalizada. Motor habilitado nuevamente.");
      }

      sobreCorriente = false;
    }
  }
}

//========================= Control de motores mediante botones
void ControlMotoresPorBotones() {

  bool presionadoHorario = ActualizarBoton(botonHorario);
  bool presionadoAntihorario = ActualizarBoton(botonAntihorario);
  bool presionadoTercero = ActualizarBoton(botonTercero);

  // Flancos de subida: detectan el instante exacto en que se presiona
  // horario/antihorario, para disparar el movimiento temporizado una
  // sola vez por pulsación (no una vez por cada vuelta del loop).
  static bool horarioAnterior = false;
  static bool antihorarioAnterior = false;
  bool horarioFlanco = (presionadoHorario && !horarioAnterior);
  bool antihorarioFlanco = (presionadoAntihorario && !antihorarioAnterior);
  horarioAnterior = presionadoHorario;
  antihorarioAnterior = presionadoAntihorario;

  if (sobreCorriente) {
    // Bloqueado hasta que ReadCurrentSensor() detecte que la
    // corriente volvió a estar dentro del límite.
    if (motorEnMarcha) {
      motorEnMarcha = false;
      Serial.println("Comando ignorado: sobrecorriente activa.");
    }
    motorEnMovimientoTemporizado = false;
    detenerMotor();
    return;
  }

  //========================= MODO MANUAL (boton3 + horario, o boton3 + antihorario)
  // Gira mientras ambos se mantengan presionados; al soltar cualquiera, se detiene.
  if (presionadoTercero && presionadoHorario && !presionadoAntihorario) {

    motorEnMovimientoTemporizado = false; // el modo manual cancela cualquier temporizado en curso
    if (!motorEnMarcha) {
      motorEnMarcha = true;
      Serial.println("Manual: horario (boton3 + boton1)");
    }
    girarHorario();
    return;
  }

  if (presionadoTercero && presionadoAntihorario && !presionadoHorario) {

    motorEnMovimientoTemporizado = false;
    if (!motorEnMarcha) {
      motorEnMarcha = true;
      Serial.println("Manual: antihorario (boton3 + boton2)");
    }
    girarAntihorario();
    return;
  }

  //========================= MODO TEMPORIZADO (horario/antihorario solos, sin boton3)
  // Un solo toque dispara el giro por DURACION_MOVIMIENTO_MS; se ignoran
  // pulsaciones nuevas mientras el movimiento en curso no haya terminado.
  if (!motorEnMovimientoTemporizado && !presionadoTercero) {

    if (horarioFlanco && !presionadoAntihorario) {
      motorEnMovimientoTemporizado = true;
      direccionMovimientoTemporizado = 1;
      tiempoInicioMovimiento = millis();
      motorEnMarcha = true;
      Serial.println("Movimiento temporizado: horario");
    }
    else if (antihorarioFlanco && !presionadoHorario) {
      motorEnMovimientoTemporizado = true;
      direccionMovimientoTemporizado = -1;
      tiempoInicioMovimiento = millis();
      motorEnMarcha = true;
      Serial.println("Movimiento temporizado: antihorario");
    }
  }

  if (motorEnMovimientoTemporizado) {

    if (millis() - tiempoInicioMovimiento >= DURACION_MOVIMIENTO_MS) {
      motorEnMovimientoTemporizado = false;
      motorEnMarcha = false;
      detenerMotor();
      Serial.println("Tiempo de movimiento cumplido, deteniendo motor.");
    }
    else if (direccionMovimientoTemporizado == 1) {
      girarHorario();
    }
    else {
      girarAntihorario();
    }
  }
  else {
    if (motorEnMarcha) {
      motorEnMarcha = false;
      Serial.println("stop");
    }
    detenerMotor();
  }
}

//========================= Movimiento Motor
void girarHorario() {
  digitalWrite(DRV_ENABLE, HIGH);
  ledcWrite(MOTOR_PIN_1, VELOCIDAD);
  ledcWrite(MOTOR_PIN_2, 0);
  direccionMotorActual = 1;
}

void girarAntihorario() {
  digitalWrite(DRV_ENABLE, HIGH);
    ledcWrite(MOTOR_PIN_1, 0);
    ledcWrite(MOTOR_PIN_2, VELOCIDAD);
    direccionMotorActual = -1;
}

void detenerMotor() {
  digitalWrite(DRV_ENABLE, LOW);
  ledcWrite(MOTOR_PIN_1, 0);
  ledcWrite(MOTOR_PIN_2, 0);
  direccionMotorActual = 0;
}

//========================= LED de estado
bool ParpadeoActivo(unsigned long periodoMs) {
  // true durante la primera mitad del periodo, false en la segunda: onda cuadrada de parpadeo
  return (millis() % periodoMs) < (periodoMs / 2);
}

void ActualizarLED() {

  static uint32_t colorAnterior = 0xFFFFFFFF; // valor imposible, fuerza el primer show()
  uint32_t color;

  // La alerta roja vence sola después de ledRojoDuration
  if (ledRojoActivo && (millis() - ledRojoMillis >= ledRojoDuration)) {
    ledRojoActivo = false;
  }

  if (ledRojoActivo) {
    color = led.Color(255, 0, 0);              // rojo: sobrecorriente
  }
  else if (direccionMotorActual == 1) {
    color = led.Color(0, 255, 0);              // verde: girando horario
  }
  else if (direccionMotorActual == -1) {
    color = led.Color(0, 0, 255);              // azul: girando antihorario
  }
  else if (porcentajeBateria <= BATERIA_UMBRAL_CRITICO) {
    // // parpadeo rápido: batería a punto de apagarse
    // color = ParpadeoActivo(250) ? led.Color(255, 255, 255) : led.Color(0, 0, 0);
    color = ParpadeoActivo(500) ? led.Color(255, 255, 255) : led.Color(0, 0, 0);
  }
  else if (porcentajeBateria <= BATERIA_UMBRAL_10) {
    // parpadeo medio: batería al 10%
    color = ParpadeoActivo(600) ? led.Color(255, 255, 255) : led.Color(0, 0, 0);
  }
  else if (porcentajeBateria <= BATERIA_UMBRAL_20) {
    // parpadeo lento: batería al 20%
    color = ParpadeoActivo(1200) ? led.Color(255, 255, 255) : led.Color(0, 0, 0);
  }
  else if (estadoActual == ESTADO_PROGRAMANDO) {
    color = led.Color(255, 0, 255);            // magenta: subiendo firmware
  }
  else if (estadoActual == ESTADO_SERVIDOR_OTA) {
    color = led.Color(255, 150, 0);            // ámbar: servidor esperando conexión
  }
  else {
    color = led.Color(0, 0, 0);                // apagado: motor detenido, operación normal
  }

  if (color != colorAnterior) {
    led.setPixelColor(0, color);
    led.show();
    colorAnterior = color;
  }
}
