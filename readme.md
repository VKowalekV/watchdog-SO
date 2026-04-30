# Watchdog – Instrukcja uruchomienia

Systemowy demon nadzorujący procesy. Uruchamia podane programy jako podprocesy, monitoruje ich stan i restartuje je w razie zakończenia.

---

## Wymagania

- System Linux (testowano na Ubuntu)
- Kompilator `gcc` i `make`
- Uprawnienia `root` (demon zapisuje pliki w `/etc/`, `/var/run/`, `/var/log/`)

---

## 1. Kompilacja

```bash
make
```

Tworzy pliki:

- `build/watchdog` – główny demon
- `build/tests/CPU_test` – program testowy obciążający CPU
- `build/tests/RAM_test` – program testowy zapełniający RAM

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

Każda linia opisuje jeden nadzorowany proces:

```
# Komentarze zaczynają się od '#'
# Format: <Nazwa>  <CPU_sek>  <RAM_MB>  <Ścieżka>

NazwaProcesu  30  50  /bezwzgledna/sciezka/do/programu
```

| Pole      | Opis                                                |
| --------- | --------------------------------------------------- |
| `Nazwa`   | Dowolna nazwa (używana w logach), bez spacji        |
| `CPU_sek` | Limit czasu CPU w sekundach (`0` = brak limitu)     |
| `RAM_MB`  | Limit pamięci RAM w megabajtach (`0` = brak limitu) |
| `Ścieżka` | Bezwzględna ścieżka do pliku wykonywalnego          |

### Przykładowy plik konfiguracyjny

```
# Nazwa      CPU_sek  RAM_MB  Ścieżka
CPU_test     20       100     /home/user/watchdog_so/build/tests/CPU_test
RAM_test     50       30      /home/user/watchdog_so/build/tests/RAM_test
```

> **Uwaga:** Ścieżki muszą być bezwzględne. Plik wykonywalny musi mieć prawo wykonania (`chmod +x`).

---

## 3. Uruchomienie

```bash
sudo ./build/watchdog
```

Demon uruchamia się w tle (odłącza od terminala). Demon działa dalej jako usługa systemowa.

Logi można obserwować na bieżąco:

```bash
sudo tail -f /var/log/watchdog.log
```

---

## 4. Logi

Plik logów: `/var/log/watchdog.log`

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

### CPU_test

Symuluje proces pochłaniający 100% czasu procesora (nieskończona pętla obliczeniowa). Służy do weryfikacji limitu `RLIMIT_CPU`.

Po upływie limitu CPU jądro wysyła sygnał `SIGXCPU`, a demon odnotowuje to w logach i restartuje proces.

**Wpis w konfiguracji** (`/etc/watchdog/watchdog.conf`):

```
# Nazwa    CPU_sek  RAM_MB  Ścieżka
CPU_test   20       20       /ścieżka/do/projektu/build/tests/CPU_test
```

### RAM_test

Symuluje wyciek pamięci – co sekundę alokuje 5 MB. Służy do weryfikacji limitu `RLIMIT_AS`.

Gdy limit zostanie osiągnięty, `malloc()` zwraca `NULL`, program próbuje zapisać pod adres zerowy i wywołuje `SIGSEGV`. Demon wykrywa śmierć procesu i restartuje go.

**Wpis w konfiguracji** (`/etc/watchdog/watchdog.conf`):

```
# Nazwa    CPU_sek  RAM_MB  Ścieżka
RAM_test   30        30      /ścieżka/do/projektu/build/tests/RAM_test
```

### Przykład użycia obu programów jednocześnie

```
CPU_test   20   20    /home/user/watchdog_so/build/tests/CPU_test
RAM_test   30   30    /home/user/watchdog_so/build/tests/RAM_test
```

Po uruchomieniu demona zaczną się pojawiać wpisy w logach:

```bash
sudo tail -f /var/log/watchdog.log
```

W logach powinny pojawiać się wpisy `[RESTART]` z przyczyną zakończenia procesu (`SIGXCPU` dla CPU_test, `SIGSEGV` dla RAM_test).

---

## 9. Ochrona przed podwójnym uruchomieniem

Próba uruchomienia drugiej instancji demona kończy się komunikatem:

```
Watchdog już działa (PID: 1234)!
```

i natychmiastowym zakończeniem programu.
