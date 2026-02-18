# HARMONOGRAM ZADAŃ CRON

Projekt realizuje funkcjonalny odpowiednik systemowego narzędzia cron dla systemów operacyjnych zgodnych ze standardem POSIX. Jest to zaawansowany harmonogram zadań, który pozwala na planowanie i automatyczne uruchamianie programów z argumentami wiersza poleceń w określonym czasie. System opiera się na modelu Serwer-Klient.

Funkcjonalności:
- Planowanie zadań: obsługa trzech trybów czasu: względnego (odliczanie), bezwzględnego (konkretna godzina) oraz cyklicznego (interwały).
- Zarządzanie zadaniami: możliwość wyświetlania listy zaplanowanych operacji oraz anulowania wybranych zadań.
- Logowanie zdarzeń: integracja z zewnętrzną biblioteką logowania (zrealizowaną w poprzednim projekcie), dokumentującą start, wykonanie i zakończenie zadań.
- Zegary interwałowe: precyzyjne odmierzanie czasu oparte na mechanizmach sygnałowych zegarów systemowych.
- Bezpieczeństwo: zapewnienie działania tylko jednej instancji serwera w danym czasie.

Kompilacja: gcc main.c cron_server.c cron_client.c logger.c -o my_cron -pthread -lrt
Uruchomienie: ./my_cron
