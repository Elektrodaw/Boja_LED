# System Sterowania Oświetleniem LED RGB (ESP-NOW)

Rozproszony system bezprzewodowego sterowania analogowymi taśmami LED RGB oparty na protokole **ESP-NOW**. Architektura wykorzystuje jedną jednostkę centralną (**Master** na układzie ESP32) pełniącą rolę punktu dostępowego Wi-Fi z serwerem WWW oraz dowolną liczbę modułów wykonawczych (**Slave** na układach ESP8266).

---

## 🏗️ Architektura Systemu

* **Jednostka główna (Master – ESP32):**
  * Pracuje w trybie `WIFI_AP_STA` (tworzy własną, wydzieloną sieć Wi-Fi na sztywnym kanale `1`).
  * Hostuje responsywny panel sterowania WWW zapisany w pamięci Flash (`PROGMEM`).
  * Nadaje z maksymalną dostępną mocą radiową (`+19.5 dBm`) dla zapewnienia maksymalnego zasięgu.
  * Weryfikuje dostępność odbiorników w czasie rzeczywistym (sprzętowe potwierdzenia ACK + cykliczny *heartbeat* co 3 sekundy).
* **Jednostki wykonawcze (Slave – ESP8266):**
  * Nasłuchują pakietów unicast bezpośrednio po protokole ESP-NOW na kanale `1`.
  * Sterują trzema kanałami PWM (zakres 8-bitowy `0–255`) podłączonymi do bramek tranzystorów MOSFET.
  * Posiadają wyłączoną obsługę sprzętowego portu szeregowego (`Serial`), co pozwala na bezkolizyjne wykorzystanie pinu `GPIO3 (RX)` jako wyjścia PWM.

---

## 🔌 Schemat Podłączenia (Slave – ESP8266)

Każdy odbiornik steruje taśmą LED RGB za pomocą 3 tranzystorów MOSFET (np. IRLZ44N lub AO3400) z wspólną masą (GND):

| Kanał LED | Pin ESP8266 | Oznaczenie na płytce (Wemos/NodeMCU) | Uwagi sprzętowe |
| :--- | :--- | :--- | :--- |
| **Czerwony (R)** | `GPIO3` | `RX` | **Odłączyć przed programowaniem przez USB!** |
| **Zielony (G)** | `GPIO4` | `D2` | Zalecany rezystor 10kΩ pull-down do masy |
| **Niebieski (B)** | `GPIO5` | `D1` | Zalecany rezystor 10kΩ pull-down do masy |

> **⚠️ Krytyczna uwaga dotycząca pinu GPIO3 (RX):**  
> Pin `GPIO3` jest sprzętowo współdzielony z linią odbiorczą konwertera USB-UART. Podłączony do niego tranzystor MOSFET zakłóci transmisję podczas wgrywania oprogramowania i spowoduje błąd `espcomm_sync failed` / `PermissionError`. **Na czas flashowania płytki przez USB przewód od kanału R musi być fizycznie odłączony.**

---

## 🚀 Instrukcja Wdrożenia (Krok po kroku)

### Krok 1: Odczyt adresów MAC odbiorników
1. Otwórz szkic pomocniczy do odczytu adresu MAC w Arduino IDE.
2. Wybierz płytkę z rodziny **ESP8266** (np. *LOLIN(WEMOS) D1 R2 & mini*).
3. Wgraj kod na każdą płytkę Slave i odczytaj jej unikalny adres MAC w **Serial Monitorze** (`115200 baud`).

### Krok 2: Programowanie odbiorników (Slave – ESP8266)
1. Upewnij się, że pin `GPIO3 (RX)` jest odłączony.
2. Wgraj szkic odbiornika (**Slave v1**) na wszystkie docelowe płytki ESP8266.
3. Po zakończeniu wgrywania podłącz przewód sterujący kanałem czerwonym (R) do pinu `GPIO3`.

### Krok 3: Konfiguracja i programowanie Mastera (ESP32)
1. W kodzie **Master (ESP32 AP)** zaktualizuj tablicę `slaves[]`, wpisując odczytane wcześniej adresy MAC oraz nazwy własne stref:
   ```cpp
   const int NUM_SLAVES = 2;
   SlaveDevice slaves[NUM_SLAVES] = {
     {{0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, "Strefa 1 - Salon", false, 0, 0, 0},
     {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "Strefa 2 - Sypialnia", false, 0, 0, 0}
   };