#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <errno.h>

#include "hk_fpga.h"
#include "uart.h"

#include "tstamp.h"

#define TH_MINUTES 20

static struct timespec m_pps_ts;
static struct timespec m_gga_ts;

static struct timespec m_tstamp_ts;
static int m_tstamp_hh = 0;
static int m_tstamp_mm = 0;
static int m_tstamp_ss = 0;
static int m_tstamp_us = 0;

pthread_mutex_t m_tstamp_lock;

static inline uint32_t delta_nsec(const struct timespec *t1, const struct timespec *t0) {
	uint32_t dsec = t1->tv_sec - t0->tv_sec;
	int32_t dnsec = t1->tv_nsec - t0->tv_nsec;
	if (dsec > 0 ){
		if (dsec > 2){
			dsec = 2000000000U;
		}
		else{
			dsec = dsec * 1000000000U;
		}
		dsec += (uint32_t) dnsec;
	}
	return (uint32_t) dsec; 
}

static inline void delta_time(const struct timespec *t1, const struct timespec *t0, struct timespec *dt) {
	dt->tv_sec = t1->tv_sec - t0->tv_sec;
	dt->tv_nsec = t1->tv_nsec - t0->tv_nsec;
	if (dt->tv_sec > 0) {
		dt->tv_sec -= 1;
		dt->tv_nsec += 1000000000;
	}
	if (dt->tv_nsec >= 1000000000) {
		dt->tv_sec += 1;
		dt->tv_nsec -= 1000000000;
	}
}

inline int TimeStamp::pps_wait() {
	int count = 0;
	uint32_t state, old_state = 0x0000 ;
	for(int i = 0; i < 150000; i++) {
		state = g_hk_fpga_reg_mem->in_p & HK_FPGA_GPIO_BIT7;
		if ( state != old_state ) {
			old_state = state;
			if (i > 0) { //If PPS does not change from 1, then PPS is not active
				clock_gettime(CLOCK_REALTIME, &m_pps_ts);
				return 0;
			}	
		} else {
			count++;
			usleep(5);
		}
	}
	return -1;
}

void *ppsAcqThreadFcn(void *ptr) {
	TimeStamp* timestamp = static_cast<TimeStamp*>(ptr);
	
    timestamp->clearFlag(TimeStamp::TS_NOPPS);

	sleep(1);
    
    for(;;) {
        int res = timestamp->pps_wait();
        if (res == 0) { // PPS found wait till the next one
        	AUTO_CLEAR(timestamp, TimeStamp::TS_NOPPS);
			// printf("PPS received\n");
        	usleep(750000);
        } else { // No signal/fix from PPS
        	timestamp->raiseFlag(TimeStamp::TS_NOPPS);
			// printf("PPS not received\n");
        	sleep(1);
        }
    }
    
    // This point should never be reached
    return EXIT_SUCCESS;
}

inline void TimeStamp::gga_read() {

	if (g_uart_nbytes < 17) {
		return;
	}
	
	if (g_uart_buff[0] == '$') {
		
		if (g_uart_buff[1] == 'G') {
		
			if (g_uart_buff[2] == 'P' || g_uart_buff[2] == 'L' || g_uart_buff[2] == 'N') {
				
				if (g_uart_buff[3] == 'G') { // GGA
				
					if (g_uart_buff[4] == 'G') {
					
						if (g_uart_buff[5] == 'A') {
						
							clock_gettime(CLOCK_REALTIME, &m_gga_ts);
							
							uint32_t dnsec = delta_nsec(&m_gga_ts, &m_pps_ts);
							if (dnsec < 1000000000) {
								// printf("delta sec between current OS and PPS sampled time is lower than 1s: %u ns \n", dnsec);
            				
								AUTO_CLEAR(this, TimeStamp::TS_OVTIME);
								pthread_mutex_lock(&m_tstamp_lock);

								m_tstamp_ts.tv_sec = m_pps_ts.tv_sec;
								m_tstamp_ts.tv_nsec = m_pps_ts.tv_nsec;

								m_tstamp_hh = (uint32_t)(g_uart_buff[7]-'0')*10 + (uint32_t)(g_uart_buff[8]-'0');
								m_tstamp_mm = (uint32_t)(g_uart_buff[9]-'0')*10 + (uint32_t)(g_uart_buff[10]-'0');
								m_tstamp_ss = (uint32_t)(g_uart_buff[11]-'0')*10 + (uint32_t)(g_uart_buff[12]-'0');
								m_tstamp_us = (uint32_t)(g_uart_buff[14]-'0')*100000 + (uint32_t)(g_uart_buff[15]-'0')*10000 + (uint32_t)(g_uart_buff[16]-'0');
								
								time_t rawtime;
								struct tm *timeinfo;
    							time(&rawtime);
								timeinfo = localtime(&rawtime);
								int current_hour = timeinfo->tm_hour;
								int current_minute = timeinfo->tm_min;

								// Compare parsed time with system time
								int minute_difference = (m_tstamp_hh * 60 + m_tstamp_mm) - (current_hour * 60 + current_minute);
								int threshold_minutes = TH_MINUTES;  // Set the threshold range of minutes
								
								if (abs(minute_difference) > threshold_minutes) {
									// printf("Error: System time is not within %d minutes of GNGGA time: current %d:%d ; gps %d:%d\n", threshold_minutes, current_hour, current_minute, m_tstamp_hh, m_tstamp_mm );
									raiseFlag(TimeStamp::TS_NOTIME);
								} else {
									// printf("System time is within %d minutes of GNGGA time. difference %d min\n", threshold_minutes, minute_difference);
									AUTO_CLEAR(this, TimeStamp::TS_NOTIME);
								}

								pthread_mutex_unlock(&m_tstamp_lock);
								
							} 
							else {
								// printf("not checking time. delta sec between current OS and PPS sampled time is greater than 1s: %u ns \n", dnsec);
								raiseFlag(TimeStamp::TS_OVTIME);
								raiseFlag(TimeStamp::TS_NOTIME);
							}
							
						}
					
					}
					
				}
				
			}
		
		}
		
	}

}

void *ggaAcqThreadFcn(void *ptr) {
	TimeStamp* timestamp = static_cast<TimeStamp*>(ptr);
	
	timestamp->clearFlag(TimeStamp::TS_NOUART);
	timestamp->clearFlag(TimeStamp::TS_OVTIME);
	timestamp->clearFlag(TimeStamp::TS_NOTIME);

	sleep(1);
    
    for(;;) {
        int res = uart_read();
        if (res > 0) { // Search GGA sentence 
        	AUTO_CLEAR(timestamp, TimeStamp::TS_NOUART );
			AUTO_CLEAR(timestamp, TimeStamp::TS_NOTIME);
			AUTO_CLEAR(timestamp, TimeStamp::TS_OVTIME);
        	timestamp->gga_read();
        } else {	// No data from UART
        	timestamp->raiseFlag(TimeStamp::TS_NOUART);
			timestamp->raiseFlag(TimeStamp::TS_OVTIME);
			timestamp->raiseFlag(TimeStamp::TS_NOTIME);
        	sleep(1);
        }
    }
    
    // This point should never be reached
    return EXIT_SUCCESS;
}
	
TimeStamp::TimeStamp() {
	threadStarted = false;
	m_status = TimeStamp::TS_NOPPS + TimeStamp::TS_NOUART + TimeStamp::TS_OVTIME + TimeStamp::TS_NOTIME;
	pthread_mutex_init(&m_status_mutex, NULL);
}

TimeStamp::~TimeStamp() {
	if (threadStarted) {
        pthread_cancel(ggaAcqThreadInfo);
        pthread_join(ggaAcqThreadInfo, NULL);    
        pthread_cancel(ppsAcqThreadInfo);
        pthread_join(ppsAcqThreadInfo, NULL);
        uart_uninit();
        hk_fpga_uninit();
    }
    pthread_mutex_destroy(&m_status_mutex);
}

int TimeStamp::init() {

	pthread_mutex_init(&m_tstamp_lock, NULL);

	int res = hk_fpga_init();
    if (res < 0) {
		fprintf(stderr, "TimeStamp::init: Error: hk_fpga_init() failed\n");
		return -1;
	}
	
	res = uart_init();
	if (res < 0) {
		fprintf(stderr, "TimeStamp::init: Error: uart_init() failed\n");
		hk_fpga_uninit();
		return -1;
	}

	// Pass 'this' pointer to threads so they can access instance methods
	res = pthread_create(&ppsAcqThreadInfo, NULL, ppsAcqThreadFcn, this);
	if (res < 0) {
		fprintf(stderr, "TimeStamp::init: Error: pps acquisition thread creation failed\n");
		uart_uninit();
        hk_fpga_uninit();
        return -1;
	}
	    
    res = pthread_create(&ggaAcqThreadInfo, NULL, ggaAcqThreadFcn, this);
    if (res < 0) {
		fprintf(stderr, "TimeStamp::init: Error: gga sentence acquisition thread creation failed\n");
		pthread_cancel(ppsAcqThreadInfo);
    	pthread_join(ppsAcqThreadInfo, NULL);
		uart_uninit();
        hk_fpga_uninit();
        return -1;
	}
    
    threadStarted = true;
	
	return 0;
}

void TimeStamp::destroy() {

	 if (threadStarted) {
    
        pthread_cancel(ggaAcqThreadInfo);
        pthread_join(ggaAcqThreadInfo, NULL);
        
        pthread_cancel(ppsAcqThreadInfo);
        pthread_join(ppsAcqThreadInfo, NULL);
        
        uart_uninit();
        
        hk_fpga_uninit();

        threadStarted = false;
        
    }

}

uint32_t TimeStamp::read(CurrentTime *currTime) {

	pthread_mutex_lock(&m_tstamp_lock);
	
	StatusFlags currentStatus = getFlags(); // Get current status
	if (currentStatus == 0x00) {
		currTime->ts.tv_sec = m_tstamp_ts.tv_sec;
		currTime->ts.tv_nsec = m_tstamp_ts.tv_nsec;
		currTime->hh = m_tstamp_hh;
		currTime->mm = m_tstamp_mm;
		currTime->ss = m_tstamp_ss;
		currTime->us = m_tstamp_us;
	}
	
	pthread_mutex_unlock(&m_tstamp_lock);
	
	return currentStatus;

}

void TimeStamp::computeAbsoluteTime(const struct timespec *ts, CurrentTime *currTime, AbsoluteTime *absTime) {

	struct timespec delta_ts;
	struct timespec tempTs; // Temporary variable for proper alignment
        tempTs = currTime->ts;  // Copy the packed member into the temporary variable
        delta_time(ts, &tempTs, &delta_ts); // Pass the temporary variable
	
	
	absTime->ppsSliceNo = (uint16_t)delta_ts.tv_sec;
	absTime->hh = currTime->hh;
	absTime->mm = currTime->mm;
	absTime->ss = currTime->ss + (uint32_t)delta_ts.tv_sec;
	absTime->us = currTime->us + (uint32_t)delta_ts.tv_nsec/1000;
	
	// Get year/month/day
	time_t timer; 
	time(&timer); 
	struct tm* tm_info = localtime(&timer);
    absTime->year = tm_info->tm_year;
	absTime->month = tm_info->tm_mon;
	absTime->day = tm_info->tm_mday;; 
	
}

// Thread-safe flag manipulation methods (public interface)
void TimeStamp::raiseFlag(TimeSts flag) {
	pthread_mutex_lock(&m_status_mutex);
	m_status |= (uint32_t)flag;
	pthread_mutex_unlock(&m_status_mutex);
}

void TimeStamp::clearFlag(TimeSts flag) {
	pthread_mutex_lock(&m_status_mutex);
	m_status &= ~((uint32_t)flag);
	pthread_mutex_unlock(&m_status_mutex);
}

void TimeStamp::clearFlags() {
	pthread_mutex_lock(&m_status_mutex);
	m_status = TimeStamp::TS_VALID;
	pthread_mutex_unlock(&m_status_mutex);
}

TimeStamp::StatusFlags TimeStamp::getFlags() {
	pthread_mutex_lock(&m_status_mutex);
	StatusFlags currentStatus = m_status;
	pthread_mutex_unlock(&m_status_mutex);
	return currentStatus;
}

void TimeStamp::autoClear(TimeSts flag) {
#ifdef AUTO_CLEAR_FLAGS
	clearFlag(flag);
#endif
}
