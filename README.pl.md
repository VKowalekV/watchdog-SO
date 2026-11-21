> [English version](README.md)

[![CI](https://github.com/VKowalekV/watchdog-SO/actions/workflows/ci.yml/badge.svg)](https://github.com/VKowalekV/watchdog-SO/actions/workflows/ci.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

# Watchdog – Systemowy demon nadzorujący procesy w Linuksie

Lekki, napisany w czystym C demon nadzorujący procesy dla systemu Linux bez zewnętrznych zależności. Uruchamia skonfigurowane programy jako procesy potomne, wymusza limity CPU i pamięci na poziomie jądra, monitoruje ich stan oraz automatycznie restartuje procesy po zakończeniu pracy.

---

## Główne funkcje

- **Daemonizacja i tryb pierwszoplanowy:** Odłącza się od terminala poprzez podwójny `fork`, `setsid()`, `umask(0)`, `chdir("/")` i przekierowanie I/O do `/dev/null` lub działa na pierwszym planie (`--foreground`) z dublowaniem logów na `stderr`.
- **Konfigurowalne ścieżki uruchomienia:** Obsługa własnych ścieżek konfiguracji, logów i pliku PID przez flagi CLI (`--config`, `--log`, `--pid`), co pozwala działać bez uprawnień `root` oraz w kontenerach.
- **Deklaratywna konfiguracja:** Nadzoruje do 32 procesów jednocześnie (`MAX_PROCESSES`) zdefiniowanych w `/etc/watchdog/watchdog.conf` (z obsługą argumentów CLI oraz cudzysłowów).
- **Limity zasobów na poziomie jądra:** Nakłada limity przez `setrlimit()` (`RLIMIT_CPU` w sekundach oraz `RLIMIT_AS` w MB; `0` wyłącza limit).
- **Izolacja grup procesów:** Umieszcza każde dziecko we własnej grupie procesów (`setpgid(0, 0)`), aby czysto zamykać całe drzewa potomków.
- **Automatyczny restart i diagnostyka:** Wykrywa zakończenie przez blokujący `waitpid()` i loguje kod wyjścia lub sygnał kończący (`SIGXCPU`, `SIGSEGV`, `SIGKILL`, `SIGTERM`) przed restartem.
- **Hot-reload bez przestoju:** Przeładowuje konfigurację na sygnał `SIGHUP`, pozostawiając niezmienione procesy aktywne oraz aktualizując lub zatrzymując zmienione wpisy.
- **Bezpieczne zamykanie:** Obsługuje `SIGTERM` / `SIGINT`, wysyła `SIGTERM` do nadzorowanych grup procesów, usuwa `/var/run/watchdog.pid` i kończy pracę.
- **Ochrona przed podwójnym uruchomieniem:** Blokuje uruchomienie duplikatu za pomocą `/var/run/watchdog.pid` i sprawdzenia `kill(pid, 0)`.

---

## Struktura projektu

```text
.
├── .github/workflows/
│   └── ci.yml              # GitHub Actions CI (-Werror + ASan/UBSan)
├── Makefile
├── watchdog.c              # Punkt wejścia demona, blokada PID, obsługa sygnałów, pętla waitpid
├── include/
│   ├── config.h            # Struktury konfiguracji, MAX_PROCESSES, MAX_ARGS, CONFIG_FILE
│   ├── daemon.h            # Interfejs daemonizacji
│   ├── logger.h            # Interfejs logowania i ścieżka LOG_FILE
│   └── process.h           # Interfejs zarządzania cyklem życia procesów
├── src/
│   ├── config.c            # Parser konfiguracji, tokenizator argumentów, generator szablonu
│   ├── daemon.c            # Daemonizacja w tle przez podwójny fork
│   ├── logger.c            # Logger z timestampami do pliku i stderr
│   └── process.c           # fork/exec, setrlimit, przeładowanie SIGHUP, restart, zamykanie
└── tests/
    ├── CPU_test.c          # Program testowy obciążający CPU (RLIMIT_CPU)
    ├── RAM_test.c          # Program testowy alokujący RAM (RLIMIT_AS)
    ├── unit_tests.c        # Testy jednostkowe parsera konfiguracji i argumentów
    ├── integration_test.sh # Testy end-to-end trybu --foreground, SIGHUP i SIGTERM
    ├── luzny_test.sh       # Pomocniczy skrypt testowy
    └── test_petla.sh       # Pomocniczy skrypt pętli testowej
```

---

## Wymagania

- System Linux (testowano na Ubuntu)
- Kompilator `gcc` i `make`
- Uprawnienia `root` przy korzystaniu z domyślnych ścieżek systemowych (`/etc/`, `/var/run/`, `/var/log/`) — przy podaniu własnych ścieżek przez flagi CLI demon działa bez `roota`

---

## 1. Kompilacja

```bash
make
```

Tworzy pliki:

- `build/watchdog` – główny demon
- `build/tests/CPU_test` – program testowy obciążający CPU
- `build/tests/RAM_test` – program testowy zapełniający RAM

Aby uruchomić automatyczne testy jednostkowe i integracyjne (bez uprawnień `root`):

```bash
make test
```

Aby wyczyścić pliki binarne:

```bash
make clean
```

---

## 2. Konfiguracja

Plik konfiguracyjny znajduje się w `/etc/watchdog/watchdog.conf`.

Przy **pierwszym uruchomieniu** demon automatycznie tworzy ten plik z szablonem i kończy działanie z komunikatem. Następnie należy go uzupełnić:

```bash
sudo nano /etc/watchdog/watchdog.conf
```

### Format pliku

Każda linia opisuje jeden nadzorowany proces (z obsługą opcjonalnych argumentów CLI oraz cudzysłowów `'...'` / `"..."`):

```text
# Komentarze zaczynają się od '#'
# Format: <Nazwa>  <CPU_sek>  <RAM_MB>  <Ścieżka> [Argumenty...]

NazwaProcesu  30  50  /bezwzgledna/sciezka/do/programu --port 8080
```

| Pole        | Opis                                                   |
| ----------- | ------------------------------------------------------ |
| `Nazwa`     | Dowolna nazwa (używana w logach), bez spacji           |
| `CPU_sek`   | Limit czasu CPU w sekundach (`0` = brak limitu)        |
| `RAM_MB`    | Limit pamięci RAM w megabajtach (`0` = brak limitu)    |
| `Ścieżka`   | Bezwzględna ścieżka do pliku wykonywalnego             |
| `Argumenty` | Opcjonalne argumenty CLI (obsługuje `'...'` i `"..."`) |

### Przykładowy plik konfiguracyjny

```text
# Nazwa      CPU_sek  RAM_MB  Ścieżka [Argumenty...]
CPU_test     20       100     /home/user/watchdog-SO/build/tests/CPU_test
RAM_test     50       30      /home/user/watchdog-SO/build/tests/RAM_test
worker       0        128     /usr/bin/python3 /opt/worker.py --interval 5
shell_loop   0        0       /bin/sh -c "while true; do sleep 1; done"
```

> **Uwaga:** Ścieżki muszą być bezwzględne. Plik wykonywalny musi mieć prawo wykonania (`chmod +x`).

---

## 3. Uruchomienie i opcje CLI

Domyślny tryb demona w tle (korzysta z `/etc/watchdog/watchdog.conf`, `/var/log/watchdog.log`, `/var/run/watchdog.pid`):

```bash
sudo ./build/watchdog
```

### Opcje CLI

| Flaga | Opis | Domyślnie |
| ----- | ---- | --------- |
| `-c, --config <ścieżka>` | Ścieżka do pliku konfiguracyjnego | `/etc/watchdog/watchdog.conf` |
| `-l, --log <ścieżka>` | Ścieżka do pliku logów | `/var/log/watchdog.log` |
| `-p, --pid <ścieżka>` | Ścieżka do pliku PID | `/var/run/watchdog.pid` |
| `-f, --foreground` | Uruchom na pierwszym planie (bez daemonizacji; logi także na `stderr`) | wyłączone |
| `-h, --help` | Wyświetl pomoc i zakończ | — |
| `-v, --version` | Wyświetl wersję i zakończ | — |

### Uruchomienie bez `roota` (tryb pierwszoplanowy)

```bash
./build/watchdog --foreground -c ./watchdog.conf -l ./watchdog.log -p ./watchdog.pid
```

W trybie demona logi można obserwować na bieżąco:

```bash
sudo tail -f /var/log/watchdog.log
```

---

## 4. Logi

Domyślny plik logów: `/var/log/watchdog.log` (można nadpisać flagą `-l, --log <ścieżka>`). W trybie `--foreground` logi są dodatkowo wypisywane na `stderr`.

Wpisy mają format `[YYYY-MM-DD HH:MM:SS] <komunikat>`.

### Przykładowe logi

```text
[2026-11-21 21:00:00] [SYSTEM] Watchdog started successfully and began supervision.
[2026-11-21 21:00:00] [START] Started 'CPU_test' (PID: 12345) | Limits: CPU 20 s, RAM 20 MB
[2026-11-21 21:00:20] [RESTART] Process 'CPU_test' (PID: 12345) terminated. Reason: SIGXCPU (CPU time limit exceeded). Restarting...
[2026-11-21 21:00:21] [START] Started 'CPU_test' (PID: 12399) | Limits: CPU 20 s, RAM 20 MB
[2026-11-21 21:01:00] [SYSTEM] Received SIGHUP. Reloading configuration...
[2026-11-21 21:01:00] [SYSTEM] Configuration reload completed successfully.
```

---

## 5. Sprawdzenie stanu

Demon zapisuje swój PID w `/var/run/watchdog.pid`:

```bash
cat /var/run/watchdog.pid
```

Sprawdzenie, czy demon działa:

```bash
ps aux | grep watchdog
```

---

## 6. Sterowanie demonem sygnałami

Sprawdź PID demona:

```bash
cat /var/run/watchdog.pid
```

| Akcja                               | Komenda                    |
| ----------------------------------- | -------------------------- |
| Przeładuj konfigurację (hot-reload) | `sudo kill -SIGHUP <PID>`  |
| Zatrzymaj demona (grzecznie)        | `sudo kill -SIGTERM <PID>` |

### Hot-reload (zmiana konfiguracji bez zatrzymania)

1. Edytuj plik konfiguracyjny:
   ```bash
   sudo nano /etc/watchdog/watchdog.conf
   ```
2. Wyślij sygnał `SIGHUP`:
   ```bash
   sudo kill -SIGHUP <PID>
   ```
3. Demon wczyta nową konfigurację:
   - procesy **bez zmian** – pozostają uruchomione
   - procesy **zmodyfikowane lub usunięte** – zostają zatrzymane
   - procesy **nowe lub zmodyfikowane** – zostają uruchomione

---

## 7. Zatrzymanie demona

```bash
sudo kill -SIGTERM <PID>
```

Demon wysyła `SIGTERM` do wszystkich nadzorowanych procesów, usuwa plik PID i kończy działanie. Zdarzenie jest odnotowane w logach.

---

## 8. Programy testowe

Przygotowaliśmy dwa programy testowe, które pozwalają zademonstrować działanie mechanizmu limitów zasobów. Po skompilowaniu projektu (`make`) gotowe pliki binarne znajdują się w katalogu `build/tests/`.

### `CPU_test`

Symuluje proces pochłaniający 100% czasu procesora (nieskończona pętla obliczeniowa). Służy do weryfikacji limitu `RLIMIT_CPU`.

Po upływie limitu CPU jądro wysyła sygnał `SIGXCPU`, a demon odnotowuje to w logach i restartuje proces.

**Wpis w konfiguracji** (`/etc/watchdog/watchdog.conf`):

```text
# Nazwa    CPU_sek  RAM_MB  Ścieżka
CPU_test   20       20      /ścieżka/do/projektu/build/tests/CPU_test
```

### `RAM_test`

Symuluje wyciek pamięci – co sekundę alokuje 5 MB. Służy do weryfikacji limitu `RLIMIT_AS`.

Gdy limit zostanie osiągnięty, `malloc()` zwraca `NULL`, program próbuje zapisać pod adres zerowy i wywołuje `SIGSEGV`. Demon wykrywa śmierć procesu i restartuje go.

**Wpis w konfiguracji** (`/etc/watchdog/watchdog.conf`):

```text
# Nazwa    CPU_sek  RAM_MB  Ścieżka
RAM_test   30       30      /ścieżka/do/projektu/build/tests/RAM_test
```

### Przykład użycia obu programów jednocześnie

```text
CPU_test   20   20    /home/user/watchdog-SO/build/tests/CPU_test
RAM_test   30   30    /home/user/watchdog-SO/build/tests/RAM_test
```

Po uruchomieniu demona zaczną się pojawiać wpisy w logach:

```bash
sudo tail -f /var/log/watchdog.log
```

W logach powinny pojawiać się wpisy `[RESTART]` z przyczyną zakończenia procesu (`SIGXCPU` dla `CPU_test`, `SIGSEGV` dla `RAM_test`).

---

## 9. Ochrona przed podwójnym uruchomieniem

Próba uruchomienia drugiej instancji demona kończy się komunikatem:

```text
Watchdog is already running (PID: 1234)!
```

i natychmiastowym zakończeniem programu.

---

## Plany rozwoju (Roadmap)

- Atomowe blokowanie pliku PID za pomocą `flock()`
- Exponential backoff i ochrona przed crash-loopami przy powtarzających się awariach
- Eskalacja zamykania procesów (`SIGTERM` -> timeout -> `SIGKILL`)
- Przekierowanie `stdout`/`stderr` procesów potomnych oraz rotacja logów
- Opcjonalne limity zasobów przez `cgroups v2` obok `setrlimit()`

---

## Licencja

Projekt udostępniany jest na licencji MIT – szczegóły w pliku [LICENSE](LICENSE).
