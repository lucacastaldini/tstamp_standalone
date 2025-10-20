#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include "tstamp.h"

// Flag per gestire il segnale di interruzione
volatile bool running = true;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, shutting down gracefully...\n", sig);
        running = false;
    }
}

void print_status_flags(TimeStamp::StatusFlags flags) {
    printf("Status Flags: 0x%02X [", flags);
    
    if (flags == TimeStamp::TS_VALID) {
        printf("VALID");
    } else {
        bool first = true;
        if (flags & TimeStamp::TS_NOPPS) {
            printf("%sNOPPS", first ? "" : "|");
            first = false;
        }
        if (flags & TimeStamp::TS_NOUART) {
            printf("%sNOUART", first ? "" : "|");
            first = false;
        }
        if (flags & TimeStamp::TS_OVTIME) {
            printf("%sOVTIME", first ? "" : "|");
            first = false;
        }
        if (flags & TimeStamp::TS_NOTIME) {
            printf("%sNOTIME", first ? "" : "|");
            first = false;
        }
    }
    printf("]\n");
}

void print_current_time(const TimeStamp::CurrentTime* currTime) {
    printf("Current Time: %02d:%02d:%02d.%06d (ts: %ld.%09ld)\n", 
           currTime->hh, currTime->mm, currTime->ss, currTime->us,
           currTime->ts.tv_sec, currTime->ts.tv_nsec);
}

void print_absolute_time(const TimeStamp::AbsoluteTime* absTime) {
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
    TimeStamp tstamp;
    
    // Inizializza il sistema
    printf("Initializing GPS timestamp system...\n");
    int res = tstamp.init();
    if (res < 0) {
        printf("Error: Failed to initialize GPS timestamp system\n");
        return EXIT_FAILURE;
    }
    printf("GPS timestamp system initialized successfully!\n\n");
    
    // Loop principale
    int iteration = 0;
    while (running) {
        iteration++;
        
        printf("=== Iteration %d ===\n", iteration);
        
        // Leggi i flag di stato
        TimeStamp::StatusFlags flags = tstamp.getFlags();
        print_status_flags(flags);
        
        // Leggi il tempo corrente
        TimeStamp::CurrentTime currTime;
        uint32_t status = tstamp.read(&currTime);
        
        if (status == TimeStamp::TS_VALID) {
            print_current_time(&currTime);
            
            // Calcola il tempo assoluto
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            
            TimeStamp::AbsoluteTime absTime;
            tstamp.computeAbsoluteTime(&now, &currTime, &absTime);
            print_absolute_time(&absTime);
        } else {
            printf("GPS time not valid (status: 0x%02X)\n", status);
        }
        
        printf("\n");
        
        // Attendi 2 secondi
        sleep(2);
    }
    
    // Cleanup
    printf("Shutting down GPS timestamp system...\n");
    tstamp.destroy();
    printf("GPS timestamp system shut down successfully.\n");
    
    return EXIT_SUCCESS;
}
