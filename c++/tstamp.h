#ifndef __TSTAMP_V2_H__
#define __TSTAMP_V2_H__

#include <cstdint>
#include <pthread.h>

#ifndef AUTO_CLEAR_FLAGS_DISABLED
    #define AUTO_CLEAR_FLAGS 1  // Default ON
#endif

// Forward declaration for friend functions
class TimeStamp;

// Thread function declarations that can access private members
void *ppsAcqThreadFcn(void *ptr);
void *ggaAcqThreadFcn(void *ptr);

class TimeStamp {

public:

	// Status flags. Used to check the status of the time stamp.
	// 8 bits values handled bitwise and described by the TimeSts enum.
	typedef uint8_t StatusFlags;

	// Time status flags values
    enum TimeSts {
		TS_NOPPS 	= 0x80,
        TS_NOUART 	= 0x40,
        TS_OVTIME   = 0x20,
		TS_NOTIME   = 0x10,
        TS_VALID 	= 0x00,
    };

    
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
	
	TimeStamp();
	~TimeStamp();
	
	int init();
	void destroy();

	// Clear all flags (set them to 0). External reset of the status flags.
	void clearFlags();

	// Get the status flags.
	StatusFlags getFlags();
	
	uint32_t read(CurrentTime *currTime);
	void computeAbsoluteTime(const struct timespec *ts, CurrentTime *currTime, AbsoluteTime *absTime);
	
	// Auto clear method (public for macro usage)
	void autoClear(TimeSts flag);
	
	// Friend functions for thread access
	friend void *ppsAcqThreadFcn(void *ptr);
	friend void *ggaAcqThreadFcn(void *ptr);
	
protected:
	
private:

	bool threadStarted;

	StatusFlags m_status; // Instance status
	pthread_mutex_t m_status_mutex; // Mutex for status protection
	
    pthread_t ppsAcqThreadInfo;
    pthread_t ggaAcqThreadInfo;	

	// Thread-safe flag manipulation methods
	void raiseFlag(TimeSts flag);
	void clearFlag(TimeSts flag);

	inline void gga_read();
	inline int pps_wait();
};

// Instance-based AUTO_CLEAR macro for the new version
#ifdef AUTO_CLEAR_FLAGS
#define AUTO_CLEAR(instance, flag) (instance)->autoClear(flag);
#else
#define AUTO_CLEAR(instance, flag)
#endif

#endif /* __TSTAMP_V2_H__ */
