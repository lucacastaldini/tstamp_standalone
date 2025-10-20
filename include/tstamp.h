#ifndef __TSTAMP_V2_H__
#define __TSTAMP_V2_H__

#include <stdint.h>
#include <pthread.h>
#include <time.h>

#ifndef AUTO_CLEAR_FLAGS_DISABLED
    #define AUTO_CLEAR_FLAGS 1  // Default ON
#endif

// Forward declaration for thread functions
typedef struct TimeStamp TimeStamp;

// Thread function declarations that can access private members
void *ppsAcqThreadFcn(void *ptr);
void *ggaAcqThreadFcn(void *ptr);

// Status flags. Used to check the status of the time stamp.
// 8 bits values handled bitwise and described by the TimeSts enum.
typedef uint8_t StatusFlags;

// Time status flags values
typedef enum {
    TS_NOPPS 	= 0x80,
    TS_NOUART 	= 0x40,
    TS_OVTIME   = 0x20,
    TS_NOTIME   = 0x10,
    TS_VALID 	= 0x00,
} TimeSts;

// Time tag data
typedef union {
    uint8_t _p8[24];
    uint16_t _p16[12];
    uint32_t _p32[6];
    struct __attribute__((packed)) {
        struct timespec ts; 	// 1-2, 2 x 32
        uint32_t hh;			// 3
        uint32_t mm;			// 4
        uint32_t ss;			// 5
        uint32_t us;			// 6
    };
} CurrentTime;

typedef union {
    uint8_t _p8[44];
    uint16_t _p16[22];
    uint32_t _p32[11];
    struct __attribute__((packed)) {
        uint16_t ppsSliceNo; // 0
        uint8_t year;
        uint8_t month;	
        uint8_t day;		// 1
        uint8_t hh;
        uint8_t mm;
        uint8_t ss;
        uint32_t us;		// 2
    };
} AbsoluteTime;

// TimeStamp structure
struct TimeStamp {
    int threadStarted;
    uint8_t f_reset_pps;
    uint8_t f_reset_gga;
    StatusFlags m_status; // Instance status
    pthread_mutex_t m_status_mutex; // Mutex for status protection
    pthread_mutex_t m_status_reset_mutex; // Mutex for status reset protection
    
    pthread_t ppsAcqThreadInfo;
    pthread_t ggaAcqThreadInfo;	
};

// Function declarations
TimeStamp* tstamp_create(void);
void tstamp_destroy(TimeStamp* tstamp);

int tstamp_init(TimeStamp* tstamp);
void tstamp_cleanup(TimeStamp* tstamp);

// Enable flags reset. External reset of the status flags.
void tstamp_enable_flags_reset(TimeStamp* tstamp);

// Get the status flags.
StatusFlags tstamp_get_flags(TimeStamp* tstamp);

uint32_t tstamp_read(TimeStamp* tstamp, CurrentTime *currTime);
void tstamp_compute_absolute_time(TimeStamp* tstamp, const struct timespec *ts, CurrentTime *currTime, AbsoluteTime *absTime);

// Thread-safe flag manipulation methods
void tstamp_raise_flag(TimeStamp* tstamp, TimeSts flag);
void tstamp_clear_flag(TimeStamp* tstamp, TimeSts flag);

// Internal methods
void tstamp_gga_read(TimeStamp* tstamp);
int tstamp_pps_wait(TimeStamp* tstamp);

// reset flags
uint8_t tstamp_get_flag_reset_pps(TimeStamp* tstamp);
uint8_t tstamp_get_flag_reset_gga(TimeStamp* tstamp);
void tstamp_set_flag_reset_pps(TimeStamp* tstamp);
void tstamp_set_flag_reset_gga(TimeStamp* tstamp);
void tstamp_clear_flag_reset_pps(TimeStamp* tstamp);
void tstamp_clear_flag_reset_gga(TimeStamp* tstamp);
void tstamp_set_flag_reset(TimeStamp* tstamp, uint8_t* reset_flag_x, uint8_t value);

// Instance-based AUTO_CLEAR macro for the new version
#ifdef AUTO_CLEAR_FLAGS
#define AUTO_CLEAR(instance, flag) tstamp_clear_flag(instance, flag);
#else
#define AUTO_CLEAR(instance, flag)
#endif

#endif /* __TSTAMP_V2_H__ */
