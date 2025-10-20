# GPS Timestamp Library Build Guide

Questo Makefile compila il codice GPS/TimeStamp creando sia librerie che eseguibili di test.

## Struttura Output

```
tstamp/
├── build/          # File oggetto (.o)
├── lib/            # Librerie
│   ├── libtstamp.a     # Libreria statica
│   └── libtstamp.so    # Libreria condivisa
└── bin/            # Eseguibili
    └── gps_timestamp_demo  # Demo GPS
```

## Comandi di Build

### Build completo
```bash
make all
```

### Build con debug/trace abilitati
```bash
make debug
```

### Solo librerie
```bash
make lib/libtstamp.a lib/libtstamp.so
```

### Solo eseguibili
```bash
make bin/gps_timestamp_demo
```

## Esecuzione

### Demo GPS
```bash
./bin/gps_timestamp_demo
```

### Test automatico
```bash
make test
```

## Installazione Sistema

### Installa librerie e binari
```bash
sudo make install
```

### Disinstalla
```bash
sudo make uninstall
```

## Personalizzazione

### Modello RedPitaya
```bash
make MODEL=Z20 all
```

### Debug abilitato
```bash
make DEBUG=1 all
```

### Prefisso installazione personalizzato
```bash
make PREFIX=/opt/myapp install
```

## Informazioni Build
```bash
make info
```

## Pulizia
```bash
make clean
```

## Dipendenze

Il Makefile gestisce automaticamente:
- Compilazione di `tstamp.cpp` e `uart.cpp`
- Linking con `hk_fpga.o` e `globals.o` dal parent directory
- Librerie RedPitaya necessarie
- Creazione delle directory di output

## Note

- La libreria condivisa richiede `LD_LIBRARY_PATH` per l'esecuzione
- Il test principale usa il `main()` già presente in `tstamp.cpp`
- Il demo GPS è un esempio semplificato per testare le funzionalità
