/*
 * NADAJNIK (MASTER) ESP32 - ESP-NOW + WebServer
 * Maksymalna moc TX, monitoring statusu Online/Offline, przyciski R/G/B
 * + Watchdog połączenia przeglądarka <-> Master (timeout 5s)
 */
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>

// =======================================================
// KONFIGURACJA SIECI NADAJNIKA
// =======================================================
const char* apSSID = "System_LED_Master";
const char* apPass = "solar";
const int WIFI_CHANNEL = 1; // Sztywny kanał dla AP i ESP-NOW

// =======================================================
// KONFIGURACJA ODBIORNIKÓW (SLAVES - ESP8266)
// =======================================================
struct SlaveDevice {
  uint8_t mac[6];
  String name;
  bool online;
  uint8_t lastR;
  uint8_t lastG;
  uint8_t lastB;
};

// Zaktualizuj adresy MAC na te odczytane z Twoich Wemosów
const int NUM_SLAVES = 5;
SlaveDevice slaves[NUM_SLAVES] = {
  {{0xec, 0xfa, 0xbc, 0x28, 0x5c, 0x21}, "Boja 1"}, //ec:fa:bc:28:5c:21
  {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "Boja 2"}, //empty
  {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "Boja 2"}, //empty
  {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "Boja 2"}, //empty
  {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "Boja 2"}  //empty
};

// Struktura pakietu danych
typedef struct struct_message {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} struct_message;

WebServer server(80);
unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 3000; // Sprawdzanie połączenia ze Slave'ami co 3 sekundy

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
    input[type="color"] { width: 60px; height: 45px; border: none; outline: none; background: transparent; cursor: pointer; }
    .btn-group { display: flex; gap: 8px; flex-wrap: wrap; }
    button { color: white; border: none; padding: 10px 14px; border-radius: 6px; font-weight: bold; cursor: pointer; transition: 0.15s; font-size: 0.9rem; }
    button:active { transform: scale(0.94); }
    .btn-r { background: #c62828; }
    .btn-g { background: #2e7d32; }
    .btn-b { background: #1565c0; }
    .btn-off { background: #424242; border: 1px solid #616161; }
    .loader { text-align: center; margin-top: 50px; color: #888; }

    /* Pełnoekranowy komunikat o utracie połączenia */
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
  <!-- Nakładka błędu połączenia -->
  <div id="connection-overlay">
    <div class="error-box">
      <div class="spinner"></div>
      <h3>Utracono połączenie!</h3>
      <p>Brak odpowiedzi z jednostki głównej (ESP32) od ponad 5 sekund.<br><br>Sprawdź połączenie z siecią Wi-Fi <strong>System_LED_Master</strong>. Panel wróci automatycznie po odzyskaniu łączności.</p>
    </div>
  </div>

  <h2>Zarządzanie Strefami LED</h2>
  <div id="app"><div class="loader">Ładowanie konfiguracji...</div></div>

  <script>
    let lastCall = {};
    let timers = {};
    let lastSuccessTime = Date.now();
    let isInitialized = false;

    // Fetch z limitem czasu (timeout), aby zerwane połączenie nie wisiało w nieskończoność
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

    function sendColorRequest(id, r, g, b) {
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

    function renderDevices(data) {
      const app = document.getElementById('app');
      app.innerHTML = '';
      data.forEach(dev => {
        const statusClass = dev.online ? 'online' : 'offline';
        const statusText = dev.online ? 'Online' : 'Offline';
        app.innerHTML += `
          <div class="card">
            <div class="header">
              <h3>${dev.name}</h3>
              <span id="status_${dev.id}" class="status ${statusClass}">
                <span class="dot"></span>${statusText}
              </span>
            </div>
            <div class="controls">
              <input type="color" id="color_${dev.id}" value="#000000" oninput="throttleSend(${dev.id}, this.value)">
              <div class="btn-group">
                <button class="btn-r" onclick="setQuickColor(${dev.id}, 255, 0, 0, '#ff0000')">R</button>
                <button class="btn-g" onclick="setQuickColor(${dev.id}, 0, 255, 0, '#00ff00')">G</button>
                <button class="btn-b" onclick="setQuickColor(${dev.id}, 0, 0, 255, '#0000ff')">B</button>
                <button class="btn-off" onclick="setQuickColor(${dev.id}, 0, 0, 0, '#000000')">WYŁĄCZ</button>
              </div>
            </div>
          </div>
        `;
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
          // Sukces - aktualizujemy znacznik czasu połączenia
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
            });
          }
        })
        .catch(() => {
          // Brak odpowiedzi - sprawdzamy czy minęło > 5s
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

    // Pierwsze pobranie od razu po załadowaniu
    syncWithMaster();

    // Cykliczne odpytywanie Mastera co 2 sekundy (pozwala precyzyjnie wykryć 5-sekundowy zanik)
    setInterval(syncWithMaster, 2000);
    // Niezależny zegar sprawdzający przekroczenie 5 sekund na wypadek zawieszenia żądania
    setInterval(checkConnectionHealth, 1000);
  </script>
</body>
</html>
)rawliteral";

// =======================================================
// FUNKCJA POMOCNICZA WYSYŁANIA STANU DO SLAVE'A
// =======================================================
void sendPayloadToSlave(int id) {
  struct_message payload;
  payload.r = slaves[id].lastR;
  payload.g = slaves[id].lastG;
  payload.b = slaves[id].lastB;
  esp_now_send(slaves[id].mac, (uint8_t *) &payload, sizeof(payload));
}

// =======================================================
// INICJALIZACJA SYSTEMU
// =======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\nInicjalizacja Mastera (ESP32)...");

  // 1. Konfiguracja Wi-Fi
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID, apPass, WIFI_CHANNEL, 0, 4);
  WiFi.disconnect();

  // Wymuszenie maksymalnej mocy nadawania radia (+19.5 dBm)
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.print("Access Point uruchomiony. IP: ");
  Serial.println(WiFi.softAPIP());

  // 2. Inicjalizacja ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd inicjalizacji ESP-NOW!");
    return;
  }

  // Rejestracja funkcji sprawdzającej dostarczenie pakietu (status połączenia)
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
              ",\"online\":" + (slaves[i].online ? "true" : "false") + "}";
      if (i < NUM_SLAVES - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/set", HTTP_GET, []() {
    if (!server.hasArg("id") || !server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
      server.send(400, "text/plain", "Bad Request");
      return;
    }
    
    int id = server.arg("id").toInt();
    if (id >= 0 && id < NUM_SLAVES) {
      slaves[id].lastR = server.arg("r").toInt();
      slaves[id].lastG = server.arg("g").toInt();
      slaves[id].lastB = server.arg("b").toInt();
      
      sendPayloadToSlave(id);
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

  if (millis() - lastPingTime >= PING_INTERVAL) {
    lastPingTime = millis();
    for (int i = 0; i < NUM_SLAVES; i++) {
      sendPayloadToSlave(i);
    }
  }
}