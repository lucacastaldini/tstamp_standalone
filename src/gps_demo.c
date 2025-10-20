#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "../include/tstamp.h"

// Flag per gestire il segnale di interruzione
volatile int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, shutting down gracefully...\n", sig);
        running = 0;
    }
}

void print_status_flags(StatusFlags flags) {
    printf("Status Flags: 0x%02X [", flags);
    
    if (flags == TS_VALID) {
        printf("VALID");
    } else {
        int first = 1;
        if (flags & TS_NOPPS) {
            printf("%sNOPPS", first ? "" : "|");
            first = 0;
        }
        if (flags & TS_NOUART) {
            printf("%sNOUART", first ? "" : "|");
            first = 0;
        }
        if (flags & TS_OVTIME) {
            printf("%sOVTIME", first ? "" : "|");
            first = 0;
        }
        if (flags & TS_NOTIME) {
            printf("%sNOTIME", first ? "" : "|");
            first = 0;
        }
    }
    printf("]\n");
}

void print_current_time(const CurrentTime* currTime) {
    printf("Current Time: %02d:%02d:%02d.%06d (ts: %ld.%09ld)\n", 
           currTime->hh, currTime->mm, currTime->ss, currTime->us,
           currTime->ts.tv_sec, currTime->ts.tv_nsec);
}

void print_absolute_time(const AbsoluteTime* absTime) {
    printf("Absolute Time: %04d-%02d-%02d %02d:%02d:%02d.%06d (PPS slice: %d)\n",
           absTime->year + 1900, absTime->month + 1, absTime->day,
           absTime->hh, absTime->mm, absTime->ss, absTime->us,
           absTime->ppsSliceNo);
}

int main() {
    printf("=== GPS Timestamp Demo ===\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Imposta i gestori di segnale
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Crea l'oggetto TimeStamp
    TimeStamp* tstamp = tstamp_create();
    if (!tstamp) {
        printf("Error: Failed to create TimeStamp object\n");
        return EXIT_FAILURE;
    }
    
    // Inizializza il sistema
    printf("Initializing GPS timestamp system...\n");
    int res = tstamp_init(tstamp);
    if (res < 0) {
        printf("Error: Failed to initialize GPS timestamp system\n");
        tstamp_destroy(tstamp);
        return EXIT_FAILURE;
    }
    printf("GPS timestamp system initialized successfully!\n\n");
    
    // Loop principale
    int iteration = 0;
    while (running) {
        iteration++;
        
        printf("=== Iteration %d ===\n", iteration);
        
        // Leggi i flag di stato
        StatusFlags flags = tstamp_get_flags(tstamp);
        print_status_flags(flags);
        
        // Leggi il tempo corrente
        CurrentTime currTime;
        uint32_t status = tstamp_read(tstamp, &currTime);
        
        if (status == TS_VALID) {
            print_current_time(&currTime);
            
            // Calcola il tempo assoluto
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            
            AbsoluteTime absTime;
            tstamp_compute_absolute_time(tstamp, &now, &currTime, &absTime);
            print_absolute_time(&absTime);
            
        } else {
            printf("GPS time not valid (status: 0x%02X)\n", status);
        }
        printf("\n");

        #ifndef AUTO_CLEAR_FLAGS
            printf("Clearing flags...\n");
            tstamp_enable_flags_reset(tstamp);
        #endif
        
        // Attendi 2 secondi
        sleep(2);
    }
    
    // Cleanup
    printf("Shutting down GPS timestamp system...\n");
    tstamp_cleanup(tstamp);
    tstamp_destroy(tstamp);
    printf("GPS timestamp system shut down successfully.\n");
    
    return EXIT_SUCCESS;
}
