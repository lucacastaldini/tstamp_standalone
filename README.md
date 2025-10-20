# GPS Timestamp System - API Reference

Sistema di sincronizzazione temporale basato su ricevitore GPS con segnale PPS (Pulse Per Second) e parsing di messaggi NMEA GGA.

## Descrizione

Questo sistema acquisisce timestamp precisi da un ricevitore GPS utilizzando:
- **Segnale PPS**: acquisito tramite GPIO dell'FPGA (bit 7) per sincronizzazione di precisione
- **Messaggi GGA**: ricevuti tramite UART per estrarre l'ora UTC

Il sistema opera in modalità multi-thread con acquisizione continua di PPS e messaggi NMEA, validando la coerenza temporale con il clock di sistema.

## Strutture Dati

### TimeStamp
```c
TimeStamp* tstamp_create(void);
void tstamp_destroy(TimeStamp* tstamp);
```
Struttura principale che gestisce lo stato del sistema, i thread di acquisizione e i flag di stato.

### CurrentTime
```c
typedef struct {
    struct timespec ts;  // Timestamp OS al momento del PPS
    uint32_t hh;         // Ore UTC (0-23)
    uint32_t mm;         // Minuti (0-59)
    uint32_t ss;         // Secondi (0-59)
    uint32_t us;         // Microsecondi (0-999999)
} CurrentTime;
```
Contiene il tempo GPS sincronizzato con il PPS.

### AbsoluteTime
```c
typedef struct {
    uint16_t ppsSliceNo; // Numero di secondi trascorsi dal PPS
    uint8_t year;        // Anno (tm_year format)
    uint8_t month;       // Mese (tm_mon format: 0-11)
    uint8_t day;         // Giorno del mese (1-31)
    uint8_t hh;          // Ore (0-23)
    uint8_t mm;          // Minuti (0-59)
    uint8_t ss;          // Secondi (0-59)
    uint32_t us;         // Microsecondi
} AbsoluteTime;
```
Rappresenta un timestamp assoluto calcolato a partire da un riferimento temporale.

### StatusFlags (TimeSts)
```c
typedef enum {
    TS_VALID  = 0x00,  // Tutti i segnali validi
    TS_NOPPS  = 0x80,  // Segnale PPS non rilevato
    TS_NOUART = 0x40,  // Nessun dato dalla UART
    TS_OVTIME = 0x20,  // Delta PPS-GGA > 1 secondo
    TS_NOTIME = 0x10,  // Tempo GPS non coerente con sistema (>20 min)
} TimeSts;
```

## API Principale

### Inizializzazione e Cleanup

#### `TimeStamp* tstamp_create(void)`
Crea e inizializza la struttura TimeStamp.

**Ritorna:**
- Puntatore all'oggetto TimeStamp (allocazione statica)

**Esempio:**
```c
TimeStamp* tstamp = tstamp_create();
```

---

#### `int tstamp_init(TimeStamp* tstamp)`
Inizializza il sistema GPS timestamp:
- Inizializza FPGA per lettura GPIO PPS
- Inizializza UART per ricezione NMEA
- Avvia i thread di acquisizione PPS e GGA

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp

**Ritorna:**
- `0` in caso di successo
- `-1` in caso di errore

**Esempio:**
```c
if (tstamp_init(tstamp) < 0) {
    fprintf(stderr, "Errore inizializzazione GPS\n");
    return -1;
}
```

---

#### `void tstamp_cleanup(TimeStamp* tstamp)`
Ferma i thread e rilascia le risorse (UART, FPGA).

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp

**Esempio:**
```c
tstamp_cleanup(tstamp);
```

---

#### `void tstamp_destroy(TimeStamp* tstamp)`
Distrugge l'oggetto TimeStamp. Chiama automaticamente `tstamp_cleanup` se necessario.

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp

**Esempio:**
```c
tstamp_destroy(tstamp);
```

---

### Lettura Timestamp

#### `uint32_t tstamp_read(TimeStamp* tstamp, CurrentTime* currTime)`
Legge il timestamp corrente sincronizzato con GPS.

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp
- `currTime`: puntatore a struttura CurrentTime da riempire

**Ritorna:**
- `StatusFlags`: stato corrente del sistema
- `0x00` (TS_VALID) se tutti i segnali sono validi
- Altri valori indicano problemi (vedi StatusFlags)

**Note:**
- Thread-safe: utilizza mutex interno
- `currTime` viene riempito solo se lo stato è `TS_VALID`

**Esempio:**
```c
CurrentTime currTime;
StatusFlags status = tstamp_read(tstamp, &currTime);

if (status == TS_VALID) {
    printf("GPS Time: %02d:%02d:%02d.%06d\n", 
           currTime.hh, currTime.mm, currTime.ss, currTime.us);
} else {
    printf("GPS time non valido. Status: 0x%02X\n", status);
}
```

---

#### `void tstamp_compute_absolute_time(TimeStamp* tstamp, const struct timespec* ts, CurrentTime* currTime, AbsoluteTime* absTime)`
Calcola il timestamp assoluto per un evento avvenuto al tempo `ts`, usando come riferimento il `CurrentTime` valido.

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp
- `ts`: timestamp dell'evento (da `clock_gettime`)
- `currTime`: tempo GPS di riferimento (ottenuto da `tstamp_read`)
- `absTime`: puntatore a struttura AbsoluteTime da riempire

**Utilizzo tipico:**
Usato per timestampare eventi con precisione GPS calcolando il delta rispetto all'ultimo PPS sincronizzato.

**Esempio:**
```c
// Ottieni il tempo GPS valido
CurrentTime currTime;
if (tstamp_read(tstamp, &currTime) == TS_VALID) {
    
    // Quando avviene un evento, cattura il timestamp
    struct timespec event_ts;
    clock_gettime(CLOCK_REALTIME, &event_ts);
    
    // Calcola il tempo assoluto dell'evento
    AbsoluteTime absTime;
    tstamp_compute_absolute_time(tstamp, &event_ts, &currTime, &absTime);
    
    printf("Evento: %04d-%02d-%02d %02d:%02d:%02d.%06d (PPS slice: %d)\n",
           absTime.year + 1900, absTime.month + 1, absTime.day,
           absTime.hh, absTime.mm, absTime.ss, absTime.us,
           absTime.ppsSliceNo);
}
```

---

### Gestione Flag di Stato

#### `StatusFlags tstamp_get_flags(TimeStamp* tstamp)`
Legge i flag di stato correnti.

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp

**Ritorna:**
- `StatusFlags`: combinazione bitwise dei flag attivi

**Esempio:**
```c
StatusFlags flags = tstamp_get_flags(tstamp);

if (flags & TS_NOPPS) {
    printf("ATTENZIONE: Segnale PPS non rilevato\n");
}
if (flags & TS_NOUART) {
    printf("ATTENZIONE: Nessun dato UART\n");
}
if (flags & TS_OVTIME) {
    printf("ATTENZIONE: Delta PPS-GGA eccessivo\n");
}
if (flags & TS_NOTIME) {
    printf("ATTENZIONE: Tempo GPS non coerente con sistema\n");
}
```

---

#### `void tstamp_enable_flags_reset(TimeStamp* tstamp)`
Abilita il reset dei flag di errore. I thread di acquisizione cancelleranno automaticamente i flag quando rilevato un segnale valido.

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp

**Nota:**
- Utilizzare quando `AUTO_CLEAR_FLAGS` non è definito
- Con `AUTO_CLEAR_FLAGS` definito, i flag vengono cancellati automaticamente

**Esempio:**
```c
#ifndef AUTO_CLEAR_FLAGS
    tstamp_enable_flags_reset(tstamp);
#endif
```

---

#### `void tstamp_raise_flag(TimeStamp* tstamp, TimeSts flag)`
Imposta manualmente un flag di errore (thread-safe).

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp
- `flag`: flag da impostare

---

#### `void tstamp_clear_flag(TimeStamp* tstamp, TimeSts flag)`
Cancella manualmente un flag di errore (thread-safe).

**Parametri:**
- `tstamp`: puntatore all'oggetto TimeStamp
- `flag`: flag da cancellare

---

## Esempio di Utilizzo Completo

```c
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "tstamp.h"

volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

int main() {
    signal(SIGINT, signal_handler);
    
    // 1. Crea l'oggetto TimeStamp
    TimeStamp* tstamp = tstamp_create();
    if (!tstamp) {
        return -1;
    }
    
    // 2. Inizializza il sistema
    if (tstamp_init(tstamp) < 0) {
        tstamp_destroy(tstamp);
        return -1;
    }
    
    printf("Sistema GPS inizializzato\n");
    
    // 3. Loop principale
    while (running) {
        CurrentTime currTime;
        StatusFlags status = tstamp_read(tstamp, &currTime);
        
        if (status == TS_VALID) {
            printf("GPS Time: %02d:%02d:%02d.%06d\n", 
                   currTime.hh, currTime.mm, currTime.ss, currTime.us);
            
            // Esempio: timestamp di un evento
            struct timespec event_ts;
            clock_gettime(CLOCK_REALTIME, &event_ts);
            
            AbsoluteTime absTime;
            tstamp_compute_absolute_time(tstamp, &event_ts, &currTime, &absTime);
            
            printf("Absolute Time: %04d-%02d-%02d %02d:%02d:%02d.%06d\n",
                   absTime.year + 1900, absTime.month + 1, absTime.day,
                   absTime.hh, absTime.mm, absTime.ss, absTime.us);
        } else {
            printf("GPS non valido (status: 0x%02X)\n", status);
        }
        
        #ifndef AUTO_CLEAR_FLAGS
            tstamp_enable_flags_reset(tstamp);
        #endif
        
        sleep(2);
    }
    
    // 4. Cleanup
    tstamp_cleanup(tstamp);
    tstamp_destroy(tstamp);
    
    return 0;
}
```

---

## Configurazione

### Compilazione con AUTO_CLEAR_FLAGS

Per default, i flag di errore vengono automaticamente cancellati quando il segnale torna valido:

```bash
gcc -DAUTO_CLEAR_FLAGS gps_demo.c tstamp.c uart.c hk_fpga.c -lpthread -o gps_demo
```

Per disabilitare il clear automatico e richiedere il reset manuale:

```bash
gcc -DAUTO_CLEAR_FLAGS_DISABLED gps_demo.c tstamp.c uart.c hk_fpga.c -lpthread -o gps_demo
```

---

## Requisiti Hardware

- **FPGA**: Accesso memory-mapped a `0x40000000` per GPIO
  - GPIO bit 7 (P): deve essere connesso al segnale PPS del GPS
- **UART**: Device `/dev/ttyPS1` configurato a 9600 baud
  - Collegato al ricevitore GPS per messaggi NMEA

---

## Validazione Temporale

Il sistema implementa diverse validazioni:

1. **PPS Detection**: Il segnale PPS deve essere rilevato entro 750ms
2. **GGA Timeout**: I messaggi GGA devono arrivare entro 5 secondi
3. **PPS-GGA Delta**: Il delta tra PPS e ricezione GGA deve essere < 1 secondo
4. **System Time Check**: Il tempo GPS deve essere entro ±20 minuti dal clock di sistema

Se una validazione fallisce, il corrispondente flag di errore viene impostato.

---

## Thread Safety

Tutte le funzioni pubbliche sono thread-safe:
- `tstamp_read()`: protetto da mutex
- `tstamp_get_flags()`, `tstamp_raise_flag()`, `tstamp_clear_flag()`: protezione mutex
- I thread interni (`ppsAcqThreadFcn`, `ggaAcqThreadFcn`) accedono ai dati condivisi con sincronizzazione appropriata

---

## Note Importanti

1. **Privilegi root**: L'accesso a `/dev/mem` richiede privilegi di root
2. **Formato GGA**: Il sistema si aspetta messaggi NMEA standard `$G[PNL]GGA,HHMMSS.sss,...`
3. **PPS Edge**: Il codice rileva cambiamenti di stato sul GPIO (sia rising che falling edge)
4. **Time Source**: Il sistema usa `CLOCK_REALTIME` che dovrebbe essere sincronizzato con NTP
5. **Allocazione statica**: `TimeStamp` usa allocazione statica, quindi una sola istanza per processo

---

## Dipendenze

- `pthread`: thread POSIX e mutex
- `/dev/mem`: accesso diretto alla memoria FPGA
- `/dev/ttyPS1`: porta seriale UART

---

## Autore & Licenza

Vedere i file sorgente per informazioni su autore e licenza.

