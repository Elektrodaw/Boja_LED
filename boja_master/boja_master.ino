/*
 * NADAJNIK (MASTER) ESP32-C3 (Super Mini / Super Mini Plus)
 * Tryb: Access Point + WebServer + ESP-NOW
 * Funkcje: Maksymalna moc TX (+19.5 dBm), status Online/Offline,
 * kolory stałe R/G/B, tryby migania 1s ON / 1s OFF dla każdego koloru,
 * stroboskop Syreny Policyjnej oraz Watchdog połączenia (5s).
 */
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>

// =======================================================
// KONFIGURACJA SIECI NADAJNIKA
// =======================================================
const char* apSSID = "System_LED_Master";
const char* apPass = "SekretneHaslo123";
const int WIFI_CHANNEL = 1; // Sztywny kanał dla AP i ESP-NOW

// =======================================================
// PARAMETRY ANIMACJI I TRYBÓW PRACY
// =======================================================
const unsigned long BLINK_INTERVAL_MS = 1000; // 1s ON / 1s OFF

enum EffectMode {
  MODE_SOLID = 0, // Światło ciągłe
  MODE_BLINK = 1, // Miganie 1s ON / 1s OFF dla wybranego koloru
  MODE_SIREN = 2  // Stroboskop policyjny R/B
};

struct SirenFrame {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint16_t duration;
};

const SirenFrame SIREN_PATTERN[] = {
  {255, 0, 0, 80},   // Czerwony błysk 1
  {0, 0, 0, 60},     // Krótka przerwa
  {255, 0, 0, 80},   // Czerwony błysk 2
  {0, 0, 0, 130},    // Pauza przed zmianą barwy
  {0, 0, 255, 80},   // Niebieski błysk 1
  {0, 0, 0, 60},     // Krótka przerwa
  {0, 0, 255, 80},   // Niebieski błysk 2
  {0, 0, 0, 130}     // Pauza przed kolejnym cyklem
};
const int SIREN_FRAMES_COUNT = sizeof(SIREN_PATTERN) / sizeof(SIREN_PATTERN[0]);

// =======================================================
// KONFIGURACJA ODBIORNIKÓW (SLAVES - ESP32-C3)
// =======================================================
struct SlaveDevice {
  uint8_t mac[6];
  String name;
  bool online;
  uint8_t lastR;
  uint8_t lastG;
  uint8_t lastB;
  EffectMode mode;
  uint8_t animStep;
  unsigned long lastAnimTime;
};

// Zaktualizuj adresy MAC na te odczytane z Twoich odbiorników
const int NUM_SLAVES = 5;
SlaveDevice slaves[NUM_SLAVES] = {
  {{0xe8, 0x3d, 0xc1, 0x84, 0xd8, 0xf0}, "Boja 1"}, //E8:3D:C1:84:D8:F0
  {{0xe8, 0x3d, 0xc1, 0x84, 0xd8, 0x18}, "Boja 2"}, //E8:3D:C1:84:D8:18
  {{0xe8, 0x3d, 0xc1, 0x84, 0xd4, 0xec}, "Boja 3"}, //E8:3D:C1:84:D4:EC
  {{0xe8, 0x3d, 0xc1, 0x84, 0xd9, 0x24}, "Boja 4"}, //E8:3D:C1:84:D9:24
  {{0xe8, 0x3d, 0xc1, 0x84, 0xcb, 0x44}, "Boja 5"}  //E8:3D:C1:84:CB:44
};

// Struktura pakietu danych ESP-NOW
typedef struct struct_message {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} struct_message;

WebServer server(80);
unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 3000;

// =======================================================
// CALLBACK POTWIERDZENIA DOSTARCZENIA (ACK)
// =======================================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  const uint8_t *mac_addr = info->des_addr;
#else
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#endif
  for (int i = 0; i < NUM_SLAVES; i++) {
    if (memcmp(slaves[i].mac, mac_addr, 6) == 0) {
      slaves[i].online = (status == ESP_NOW_SEND_SUCCESS);
      break;
    }
  }
}

// =======================================================
// KOD HTML/JS FRONTENDU (PROGMEM)
// =======================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>System Oświetlenia LED</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121212; color: #ffffff; padding: 20px; max-width: 600px; margin: 0 auto; }
    h2 { text-align: center; color: #4CAF50; margin-bottom: 25px; }
    .card { background: #1e1e1e; padding: 20px; margin-bottom: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #333; padding-bottom: 10px; margin-bottom: 15px; }
    .header h3 { margin: 0; font-size: 1.15rem; }
    .status { font-size: 0.85rem; font-weight: bold; display: flex; align-items: center; gap: 6px; padding: 4px 8px; border-radius: 12px; background: #2a2a2a; }
    .dot { width: 10px; height: 10px; border-radius: 50%; display: inline-block; }
    .online .dot { background: #4CAF50; box-shadow: 0 0 8px #4CAF50; }
    .online { color: #81c784; }
    .offline .dot { background: #f44336; box-shadow: 0 0 8px #f44336; }
    .offline { color: #e57373; }
    .controls { display: flex; align-items: center; justify-content: space-between; gap: 10px; flex-wrap: wrap; }
    .effects-row { margin-top: 12px; padding-top: 12px; border-top: 1px dashed #333; display: flex; align-items: center; justify-content: space-between; gap: 8px; flex-wrap: wrap; }
    .row-label { font-size: 0.78rem; color: #9e9e9e; text-transform: uppercase; letter-spacing: 0.5px; width: 100%; margin-bottom: 2px; }
    input[type="color"] { width: 55px; height: 42px; border: none; outline: none; background: transparent; cursor: pointer; }
    .btn-group { display: flex; gap: 7px; flex-wrap: wrap; }
    button { color: white; border: 1px solid transparent; padding: 9px 12px; border-radius: 6px; font-weight: bold; cursor: pointer; transition: 0.15s; font-size: 0.84rem; }
    button:active { transform: scale(0.94); }
    .btn-r { background: #c62828; }
    .btn-g { background: #2e7d32; }
    .btn-b { background: #1565c0; }
    .btn-blink-r { background: #7f1d1d; border-color: #ef5350; }
    .btn-blink-g { background: #1b5e20; border-color: #66bb6a; }
    .btn-blink-b { background: #0d47a1; border-color: #42a5f5; }
    .btn-blink-c { background: #37474f; border-color: #90a4ae; }
    .btn-siren { background: linear-gradient(135deg, #c62828 50%, #1565c0 50%); border-color: #666; }
    .btn-off { background: #424242; border-color: #616161; }

    button.active {
      border: 2px solid #ffffff !important;
      animation: btnPulse 1s infinite alternate;
    }
    @keyframes btnPulse {
      0% { opacity: 1; box-shadow: 0 0 10px rgba(255, 255, 255, 0.75); }
      100% { opacity: 0.65; box-shadow: 0 0 2px rgba(255, 255, 255, 0.2); }
    }

    .loader { text-align: center; margin-top: 50px; color: #888; }

    #connection-overlay {
      display: none;
      position: fixed;
      top: 0; left: 0; width: 100%; height: 100%;
      background: rgba(18, 18, 18, 0.92);
      backdrop-filter: blur(4px);
      z-index: 9999;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      text-align: center;
      padding: 20px;
      box-sizing: border-box;
    }
    .error-box {
      background: #251616;
      border: 2px solid #d32f2f;
      padding: 30px 25px;
      border-radius: 12px;
      max-width: 400px;
      box-shadow: 0 10px 25px rgba(0,0,0,0.6);
    }
    .error-box h3 { color: #ff5252; margin-top: 0; font-size: 1.4rem; }
    .error-box p { color: #e0e0e0; font-size: 0.95rem; line-height: 1.5; margin-bottom: 0; }
    .spinner {
      width: 36px; height: 36px;
      border: 4px solid rgba(255, 82, 82, 0.2);
      border-top-color: #ff5252;
      border-radius: 50%;
      animation: spin 1s linear infinite;
      margin: 0 auto 15px auto;
    }
    @keyframes spin { to { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <div id="connection-overlay">
    <div class="error-box">
      <div class="spinner"></div>
      <h3>Utracono połączenie!</h3>
      <p>Brak odpowiedzi z jednostki głównej (ESP32-C3) od ponad 5 sekund.<br><br>Sprawdź połączenie z siecią Wi-Fi <strong>System_LED_Master</strong>. Panel wróci automatycznie.</p>
    </div>
  </div>

  <h2>Zarządzanie Strefami LED</h2>
  <div id="app"><div class="loader">Ładowanie konfiguracji...</div></div>

  <script>
    let lastCall = {};
    let timers = {};
    let lastSuccessTime = Date.now();
    let isInitialized = false;

    async function fetchWithTimeout(resource, options = {}) {
      const { timeout = 2000 } = options;
      const controller = new AbortController();
      const id = setTimeout(() => controller.abort(), timeout);
      const response = await fetch(resource, {
        ...options,
        signal: controller.signal  
      });
      clearTimeout(id);
      return response;
    }

    function rgbToHex(r, g, b) {
      return "#" + [r, g, b].map(x => x.toString(16).padStart(2, '0')).join('');
    }

    function clearEffectHighlights(id) {
      ['blink_r_', 'blink_g_', 'blink_b_', 'blink_c_', 'siren_'].forEach(prefix => {
        const el = document.getElementById(prefix + id);
        if (el) el.classList.remove('active');
      });
    }

    function applyEffectHighlights(dev) {
      clearEffectHighlights(dev.id);
      if (dev.mode === 2) {
        const el = document.getElementById('siren_' + dev.id);
        if (el) el.classList.add('active');
      } else if (dev.mode === 1) {
        if (dev.r === 255 && dev.g === 0 && dev.b === 0) {
          document.getElementById('blink_r_' + dev.id)?.classList.add('active');
        } else if (dev.r === 0 && dev.g === 255 && dev.b === 0) {
          document.getElementById('blink_g_' + dev.id)?.classList.add('active');
        } else if (dev.r === 0 && dev.g === 0 && dev.b === 255) {
          document.getElementById('blink_b_' + dev.id)?.classList.add('active');
        } else {
          document.getElementById('blink_c_' + dev.id)?.classList.add('active');
        }
      }
    }

    function sendColorRequest(id, r, g, b) {
      clearEffectHighlights(id);
      fetchWithTimeout(`/set?id=${id}&r=${r}&g=${g}&b=${b}`, { timeout: 1500 })
        .then(res => {
          if (res.ok) {
            lastSuccessTime = Date.now();
            checkConnectionHealth();
          }
        })
        .catch(err => console.warn("Błąd wysyłania koloru:", err));
    }

    function throttleSend(id, hexColor) {
      const r = parseInt(hexColor.slice(1, 3), 16);
      const g = parseInt(hexColor.slice(3, 5), 16);
      const b = parseInt(hexColor.slice(5, 7), 16);
      
      const now = Date.now();
      if (!lastCall[id]) lastCall[id] = 0;
      
      if (now - lastCall[id] >= 50) {
        sendColorRequest(id, r, g, b);
        lastCall[id] = now;
      } else {
        clearTimeout(timers[id]);
        timers[id] = setTimeout(() => {
          sendColorRequest(id, r, g, b);
          lastCall[id] = Date.now();
        }, 50 - (now - lastCall[id]));
      }
    }

    function setQuickColor(id, r, g, b, hex) {
      document.getElementById('color_' + id).value = hex;
      sendColorRequest(id, r, g, b);
    }

    function startBlink(id, r, g, b, hex) {
      document.getElementById('color_' + id).value = hex;
      fetchWithTimeout(`/blink?id=${id}&r=${r}&g=${g}&b=${b}`, { timeout: 1500 })
        .then(res => {
          if (res.ok) {
            lastSuccessTime = Date.now();
            checkConnectionHealth();
            syncWithMaster();
          }
        })
        .catch(err => console.warn("Błąd uruchamiania migania:", err));
    }

    function startBlinkFromPicker(id) {
      let hex = document.getElementById('color_' + id).value;
      if (hex === '#000000') {
        hex = '#ffffff';
        document.getElementById('color_' + id).value = hex;
      }
      const r = parseInt(hex.slice(1, 3), 16);
      const g = parseInt(hex.slice(3, 5), 16);
      const b = parseInt(hex.slice(5, 7), 16);
      startBlink(id, r, g, b, hex);
    }

    function toggleSiren(id) {
      fetchWithTimeout(`/siren?id=${id}`, { timeout: 1500 })
        .then(res => {
          if (res.ok) {
            lastSuccessTime = Date.now();
            checkConnectionHealth();
            syncWithMaster();
          }
        })
        .catch(err => console.warn("Błąd przełączania syreny:", err));
    }

    function renderDevices(data) {
      const app = document.getElementById('app');
      app.innerHTML = '';
      data.forEach(dev => {
        const statusClass = dev.online ? 'online' : 'offline';
        const statusText = dev.online ? 'Online' : 'Offline';
        const initialHex = rgbToHex(dev.r, dev.g, dev.b);
        app.innerHTML += `
          <div class="card">
            <div class="header">
              <h3>${dev.name}</h3>
              <span id="status_${dev.id}" class="status ${statusClass}">
                <span class="dot"></span>${statusText}
              </span>
            </div>
            <div class="controls">
              <input type="color" id="color_${dev.id}" value="${initialHex}" oninput="throttleSend(${dev.id}, this.value)">
              <div class="btn-group">
                <button class="btn-r" onclick="setQuickColor(${dev.id}, 255, 0, 0, '#ff0000')">R</button>
                <button class="btn-g" onclick="setQuickColor(${dev.id}, 0, 255, 0, '#00ff00')">G</button>
                <button class="btn-b" onclick="setQuickColor(${dev.id}, 0, 0, 255, '#0000ff')">B</button>
                <button class="btn-off" onclick="setQuickColor(${dev.id}, 0, 0, 0, '#000000')">WYŁĄCZ</button>
              </div>
            </div>
            <div class="effects-row">
              <div class="row-label">Tryby migania (1s ON / 1s OFF) i efekty</div>
              <div class="btn-group">
                <button id="blink_r_${dev.id}" class="btn-blink-r" onclick="startBlink(${dev.id}, 255, 0, 0, '#ff0000')">Migaj R</button>
                <button id="blink_g_${dev.id}" class="btn-blink-g" onclick="startBlink(${dev.id}, 0, 255, 0, '#00ff00')">Migaj G</button>
                <button id="blink_b_${dev.id}" class="btn-blink-b" onclick="startBlink(${dev.id}, 0, 0, 255, '#0000ff')">Migaj B</button>
                <button id="blink_c_${dev.id}" class="btn-blink-c" onclick="startBlinkFromPicker(${dev.id})">Migaj Kolor</button>
                <button id="siren_${dev.id}" class="btn-siren" onclick="toggleSiren(${dev.id})">POLICJA</button>
              </div>
            </div>
          </div>
        `;
        applyEffectHighlights(dev);
      });
      isInitialized = true;
    }

    function syncWithMaster() {
      fetchWithTimeout('/api/devices', { timeout: 2000 })
        .then(res => {
          if (!res.ok) throw new Error('HTTP Error');
          return res.json();
        })
        .then(data => {
          lastSuccessTime = Date.now();
          checkConnectionHealth();

          if (!isInitialized) {
            renderDevices(data);
          } else {
            data.forEach(dev => {
              const badge = document.getElementById('status_' + dev.id);
              if (badge) {
                badge.className = 'status ' + (dev.online ? 'online' : 'offline');
                badge.innerHTML = `<span class="dot"></span>${dev.online ? 'Online' : 'Offline'}`;
              }
              applyEffectHighlights(dev);
            });
          }
        })
        .catch(() => {
          checkConnectionHealth();
        });
    }

    function checkConnectionHealth() {
      const overlay = document.getElementById('connection-overlay');
      if (Date.now() - lastSuccessTime > 5000) {
        overlay.style.display = 'flex';
      } else {
        overlay.style.display = 'none';
      }
    }

    syncWithMaster();
    setInterval(syncWithMaster, 2000);
    setInterval(checkConnectionHealth, 1000);
  </script>
</body>
</html>
)rawliteral";

// =======================================================
// FUNKCJE POMOCNICZE WYSYŁANIA DANYCH DO SLAVE'A
// =======================================================
void sendRGB(int id, uint8_t r, uint8_t g, uint8_t b) {
  struct_message payload;
  payload.r = r;
  payload.g = g;
  payload.b = b;
  esp_now_send(slaves[id].mac, (uint8_t *) &payload, sizeof(payload));
}

void sendSteadyStateToSlave(int id) {
  sendRGB(id, slaves[id].lastR, slaves[id].lastG, slaves[id].lastB);
}

// =======================================================
// INICJALIZACJA SYSTEMU
// =======================================================
void setup() {
  // Nieblokująca inicjalizacja USB-CDC (działa także na samym zasilaczu bez PC)
  Serial.begin(115200);

  // 1. Konfiguracja Wi-Fi (Access Point + ESP-NOW na tym samym kanale)
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false); // Blokada skanowania innych kanałów przez część STA
  WiFi.setSleep(false);         // Wyłączenie oszczędzania energii dla minimalnych opóźnień
  WiFi.softAP(apSSID, apPass, WIFI_CHANNEL, 0, 4);

  // Maksymalna moc nadawania radia (+19.5 dBm)
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.print("\nMaster ESP32-C3 uruchomiony. IP panelu: http://");
  Serial.println(WiFi.softAPIP());

  // 2. Inicjalizacja ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd inicjalizacji ESP-NOW!");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  // 3. Rejestracja Odbiorników (Peers)
  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;

  for (int i = 0; i < NUM_SLAVES; i++) {
    memcpy(peerInfo.peer_addr, slaves[i].mac, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.printf("Nie udalo sie dodac odbiornika %d\n", i);
    }
  }

  // 4. Endpointy serwera WWW
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", index_html);
  });

  server.on("/api/devices", HTTP_GET, []() {
    String json = "[";
    for (int i = 0; i < NUM_SLAVES; i++) {
      json += "{\"id\":" + String(i) + 
              ",\"name\":\"" + slaves[i].name + "\"" +
              ",\"online\":" + (slaves[i].online ? "true" : "false") +
              ",\"mode\":" + String((int)slaves[i].mode) +
              ",\"r\":" + String(slaves[i].lastR) +
              ",\"g\":" + String(slaves[i].lastG) +
              ",\"b\":" + String(slaves[i].lastB) + "}";
      if (i < NUM_SLAVES - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  // Ustawienie stałego koloru (wyłącza tryby migania oraz syrenę)
  server.on("/set", HTTP_GET, []() {
    if (!server.hasArg("id") || !server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
      server.send(400, "text/plain", "Bad Request");
      return;
    }
    
    int id = server.arg("id").toInt();
    if (id >= 0 && id < NUM_SLAVES) {
      slaves[id].mode = MODE_SOLID;
      slaves[id].lastR = constrain(server.arg("r").toInt(), 0, 255);
      slaves[id].lastG = constrain(server.arg("g").toInt(), 0, 255);
      slaves[id].lastB = constrain(server.arg("b").toInt(), 0, 255);
      
      sendSteadyStateToSlave(id);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid ID");
    }
  });

  // Uruchomienie migania 1s ON / 1s OFF dla zadanego koloru
  server.on("/blink", HTTP_GET, []() {
    if (!server.hasArg("id") || !server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
      server.send(400, "text/plain", "Bad Request");
      return;
    }

    int id = server.arg("id").toInt();
    if (id >= 0 && id < NUM_SLAVES) {
      slaves[id].mode = MODE_BLINK;
      slaves[id].lastR = constrain(server.arg("r").toInt(), 0, 255);
      slaves[id].lastG = constrain(server.arg("g").toInt(), 0, 255);
      slaves[id].lastB = constrain(server.arg("b").toInt(), 0, 255);
      slaves[id].animStep = 0; // Klatka 0 = ON
      slaves[id].lastAnimTime = millis();

      sendRGB(id, slaves[id].lastR, slaves[id].lastG, slaves[id].lastB);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid ID");
    }
  });

  // Przełączanie trybu Syreny Policyjnej
  server.on("/siren", HTTP_GET, []() {
    if (!server.hasArg("id")) {
      server.send(400, "text/plain", "Bad Request");
      return;
    }
    
    int id = server.arg("id").toInt();
    if (id >= 0 && id < NUM_SLAVES) {
      if (slaves[id].mode == MODE_SIREN) {
        slaves[id].mode = MODE_SOLID;
        sendSteadyStateToSlave(id);
      } else {
        slaves[id].mode = MODE_SIREN;
        slaves[id].animStep = 0;
        slaves[id].lastAnimTime = millis();
        sendRGB(id, SIREN_PATTERN[0].r, SIREN_PATTERN[0].g, SIREN_PATTERN[0].b);
      }
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid ID");
    }
  });

  server.begin();
  Serial.println("Serwer HTTP wystartował.");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  // 1. Obsługa aktywnych animacji (Miganie 1s/1s oraz Syrena Policyjna)
  for (int i = 0; i < NUM_SLAVES; i++) {
    if (slaves[i].mode == MODE_BLINK) {
      if (now - slaves[i].lastAnimTime >= BLINK_INTERVAL_MS) {
        slaves[i].lastAnimTime = now;
        slaves[i].animStep = (slaves[i].animStep == 0) ? 1 : 0;

        if (slaves[i].animStep == 0) {
          sendRGB(i, slaves[i].lastR, slaves[i].lastG, slaves[i].lastB);
        } else {
          sendRGB(i, 0, 0, 0);
        }
      }
    } 
    else if (slaves[i].mode == MODE_SIREN) {
      if (now - slaves[i].lastAnimTime >= SIREN_PATTERN[slaves[i].animStep].duration) {
        slaves[i].animStep = (slaves[i].animStep + 1) % SIREN_FRAMES_COUNT;
        slaves[i].lastAnimTime = now;
        sendRGB(i, 
                SIREN_PATTERN[slaves[i].animStep].r, 
                SIREN_PATTERN[slaves[i].animStep].g, 
                SIREN_PATTERN[slaves[i].animStep].b);
      }
    }
  }

  // 2. Heartbeat co 3 sekundy dla stref w trybie światła ciągłego
  if (now - lastPingTime >= PING_INTERVAL) {
    lastPingTime = now;
    for (int i = 0; i < NUM_SLAVES; i++) {
      if (slaves[i].mode == MODE_SOLID) {
        sendSteadyStateToSlave(i);
      }
    }
  }
}