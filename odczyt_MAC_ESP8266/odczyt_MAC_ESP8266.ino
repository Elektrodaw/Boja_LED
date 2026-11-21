/*
 * NARZĘDZIE POMOCNICZE: Odczyt adresu MAC dla ESP32 / ESP32-C3
 * W Arduino IDE ustaw: Tools -> USB CDC On Boot -> Enabled
 */
#include <WiFi.h>

void setup() {
  Serial.begin(9600);
  
  // Inicjalizacja modułu Wi-Fi w trybie stacji (wymagane przed odczytem MAC)
  WiFi.mode(WIFI_STA);
  delay(500);
}

void loop() {
  Serial.print("Adres MAC ESP32-C3: ");
  Serial.println(WiFi.macAddress());
  delay(2000);
}