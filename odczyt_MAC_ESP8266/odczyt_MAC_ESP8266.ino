/*
 * NARZĘDZIE POMOCNICZE: Odczyt adresu MAC
 * Wgraj na każdą płytkę Slave i otwórz Serial Monitor (115200 baud).
 */
#include <ESP8266WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Ustawienie trybu stacji, aby odczytać poprawny MAC
  WiFi.mode(WIFI_STA);
  
  Serial.println();
  Serial.println("=====================================");
  Serial.print("Adres MAC tej płytki: ");
  Serial.println(WiFi.macAddress());
  Serial.println("=====================================");
}

void loop() {
  // Pusta pętla
}