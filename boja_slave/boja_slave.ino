/*
 * ODBIORNIK (SLAVE) - ESP-NOW
 * Sterowanie taśmą LED RGB
 */
#include <ESP8266WiFi.h>
#include <espnow.h>
extern "C" {
  #include <user_interface.h> // Wymagane do ręcznej zmiany kanału Wi-Fi
}

// Konfiguracja pinów pod MOSFETy
#define PIN_R 5 // GPIO3 (sprzętowy RX)
#define PIN_G 4 // GPIO4 (D2)
#define PIN_B 3 // GPIO5 (D1)

// Struktura pakietu danych (musi być identyczna na Nadajniku i Odbiorniku)
typedef struct struct_message {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} struct_message;

struct_message myData;

// Callback wywoływany przy odbiorze danych przez ESP-NOW
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len == sizeof(myData)) {
    memcpy(&myData, incomingData, sizeof(myData));
    
    // Ustawienie wypełnienia PWM dla poszczególnych kanałów (0-255)
    analogWrite(PIN_R, myData.r);
    analogWrite(PIN_G, myData.g);
    analogWrite(PIN_B, myData.b);
  }
}

void setup() {
  // UWAGA: Celowo NIE wywołujemy Serial.begin(), 
  // aby nie zakłócać sygnału PWM na pinie GPIO3 (RX).
  
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  // Dostosowanie zakresu PWM do standardu 8-bitowego RGB (0-255)
  // Domyślnie ESP8266 używa zakresu 0-1023.
  analogWriteRange(255);
  
  // Stan początkowy - wyłączone
  analogWrite(PIN_R, 0);
  analogWrite(PIN_G, 0);
  analogWrite(PIN_B, 0);

  // Konfiguracja Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Rozłączamy się z ewentualnymi sieciami

  // Sztywne ustawienie kanału (musi być ten sam co w Masterze - np. kanał 1)
  wifi_promiscuous_enable(1);
  wifi_set_channel(1);
  wifi_promiscuous_enable(0);

  // Inicjalizacja protokołu ESP-NOW
  if (esp_now_init() != 0) {
    // W przypadku błędu mignij czerwoną diodą (opcjonalna diagnostyka)
    analogWrite(PIN_R, 255);
    delay(100);
    analogWrite(PIN_R, 0);
    return;
  }

  // Ustawienie roli urządzenia
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  
  // Rejestracja funkcji nasłuchującej
  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  // ESP-NOW działa asynchronicznie (przerwania)
  // Nie blokujemy pętli głównej
}