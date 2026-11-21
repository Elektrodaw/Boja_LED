/*
 * ODBIORNIK (SLAVE) - ESP32-C3 (Super Mini / Super Mini Plus)
 * Sterowanie analogową taśmą LED RGB (MOSFET) po protokole ESP-NOW
 * Piny: GPIO2 (R), GPIO3 (G), GPIO4 (B)
 */
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// =======================================================
// KONFIGURACJA SPRZĘTOWA I RADIOWA
// =======================================================
#define PIN_R 3 // GPIO2 - Kanał czerwony (R)
#define PIN_G 4 // GPIO3 - Kanał zielony (G)
#define PIN_B 2 // GPIO4 - Kanał niebieski (B)

#define WIFI_CHANNEL 1 // Musi być identyczny jak w Masterze

// Struktura pakietu danych (identyczna jak w kodzie Mastera)
typedef struct struct_message {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} struct_message;

struct_message myData;

// =======================================================
// CALLBACK ODBIORU DANYCH ESP-NOW (Rdzeń ESP32 v2.x i v3.x)
// =======================================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  if (len == sizeof(myData)) {
    memcpy(&myData, incomingData, sizeof(myData));

    // Sprzętowy kontroler LEDC na ESP32 domyślnie pracuje w zakresie 8-bit (0-255)
    analogWrite(PIN_R, myData.r);
    analogWrite(PIN_G, myData.g);
    analogWrite(PIN_B, myData.b);
  }
}

// =======================================================
// INICJALIZACJA SYSTEMU
// =======================================================
void setup() {
  // Bezpieczna inicjalizacja portu szeregowego przez natywne USB-C
  Serial.begin(115200);

  // Konfiguracja wyjść PWM
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  // Stan początkowy po podaniu zasilania - taśma wygaszona
  analogWrite(PIN_R, 0);
  analogWrite(PIN_G, 0);
  analogWrite(PIN_B, 0);
  delay(1000);
  analogWrite(PIN_R, 1);
  analogWrite(PIN_G, 0);
  analogWrite(PIN_B, 0);
  delay(1000);
  analogWrite(PIN_R, 0);
  analogWrite(PIN_G, 1);
  analogWrite(PIN_B, 0);
  delay(1000);
  analogWrite(PIN_R, 0);
  analogWrite(PIN_G, 0);
  analogWrite(PIN_B, 1);
  delay(1000);
  analogWrite(PIN_R, 0);
  analogWrite(PIN_G, 1);
  analogWrite(PIN_B, 0);
  delay(1000);

  // Konfiguracja modułu radiowego
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Wyłączenie uśpienia modemu dla natychmiastowej reakcji na szybkie błyski (syrena)
  WiFi.setSleep(false);

  // Maksymalna moc nadawania dla pewnego odsyłania potwierdzeń ACK do Mastera
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // Wymuszenie sztywnego kanału komunikacji zgodnego z Masterem
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // Inicjalizacja stosu ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd inicjalizacji ESP-NOW!");
    // Sygnalizacja błędu krótkim mignięciem kanału R
    analogWrite(PIN_R, 128);
    delay(150);
    analogWrite(PIN_R, 0);
    return;
  }

  // Rejestracja funkcji nasłuchującej
  esp_now_register_recv_cb(onDataRecv);

  delay(500);
  Serial.println("\n=====================================");
  Serial.println("Slave ESP32-C3 uruchomiony.");
  Serial.print("Adres MAC do wpisania w Masterze: ");
  Serial.println(WiFi.macAddress());
  Serial.println("=====================================");
}

void loop() {
  // Odbiór pakietów i generowanie PWM odbywają się sprzętowo w tle
}