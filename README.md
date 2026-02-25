# Harmonogram Zadań Cron (POSIX) 🕒

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Architecture](https://img.shields.io/badge/Architecture-Client--Server-blueviolet?style=for-the-badge)
![POSIX](https://img.shields.io/badge/POSIX-Compliance-lightgrey?style=for-the-badge)
  
## 📋 Opis projektu

Projekt realizuje funkcjonalny odpowiednik systemowego narzędzia **cron** dla systemów operacyjnych zgodnych ze standardem POSIX. Jest to zaawansowany harmonogram zadań, działający w modelu **Serwer-Klient**, który pozwala na planowanie i automatyczne uruchamianie procesów z argumentami wiersza poleceń. System zarządza precyzyjnym odliczaniem czasu i kolejką zadań, działając w tle jako demon systemowy.

---

## 🏗️ Struktura Projektu (Zgodnie z wymaganiami)

### 1. Model Serwer-Klient
System został rozdzielony na dwie niezależne jednostki:
* **Serwer**: Odpowiada za zarządzanie kolejką zadań, monitorowanie czasu i fizyczne uruchamianie procesów potomnych.
* **Klient**: Narzędzie interfejsu użytkownika, służące do wysyłania komend planowania, listowania zadań oraz ich anulowania.

### 2. Zaawansowane planowanie zadań
Obsługa trzech trybów harmonogramowania pozwala na dużą elastyczność:
* **Tryb Względny**: Uruchomienie zadania po upływie określonego czasu (odliczanie).
* **Tryb Bezwzględny**: Wykonanie operacji o konkretnej, zadanej godzinie.
* **Tryb Cykliczny**: Automatyczne powtarzanie zadania w określonych interwałach czasowych.

### 3. Precyzyjne odmierzanie czasu
W projekcie zrezygnowano z prostych pętli opóźniających na rzecz profesjonalnych mechanizmów systemowych:
* Wykorzystanie **zegarów interwałowych** (Interval Timers).
* Obsługa sygnałów systemowych do powiadamiania o upływie czasu, co zapewnia wysoką dokładność i niskie obciążenie procesora.

### 4. Integracja z Loggerem i Monitoring
System jest zintegrowany z autorską biblioteką logowania (Signal Driven Logger):
* Dokumentowanie pełnego cyklu życia zadania: dodanie, start, zakończenie, ewentualne błędy.
* Możliwość dynamicznej zmiany poziomu logowania serwera bez jego zatrzymywania.

### 5. Bezpieczeństwo i stabilność
* **Single Instance**: Zastosowanie mechanizmów blokujących (np. pliki blokujące/PID files), gwarantujących działanie tylko jednej instancji serwera w systemie.
* **Zarządzanie procesami**: Poprawna obsługa sygnałów i unikanie procesów "zombie" poprzez monitorowanie statusu zakończenia zadań.

---

## 🛠️ Technologie

* **Język:** C (Standard C11)
* **Mechanizmy POSIX:** Real-time Signals, Timers, IPC.
* **Wielowątkowość:** Pthreads (do równoległej obsługi wielu klientów).
* **Kompilacja:** GCC z flagami `-lrt` (Real-time extensions) oraz `-pthread`.

---

## 🚀 Kompilacja i uruchomienie

1. Skompiluj projekt za pomocą polecenia:
   ```bash
   gcc main.c cron_server.c cron_client.c logger.c -o my_cron -pthread -lrt
