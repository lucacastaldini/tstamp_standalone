#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "../include/hk_fpga.h"
#include "../include/uart.h"
#include "../include/tstamp.h"

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

int tstamp_pps_wait(TimeStamp* tstamp) {
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
			struct timespec ts = {0, 5000}; // 5 microseconds = 5000 nanoseconds
			nanosleep(&ts, NULL);
		}
	}
	return -1;
}

void *ppsAcqThreadFcn(void *ptr) {
	TimeStamp* tstamp = (TimeStamp*)ptr;
	
    sleep(1);
    
    for(;;) {
        int res = tstamp_pps_wait(tstamp);
        if (res == 0) { // PPS found wait till the next one
            if(tstamp_get_flag_reset_pps(tstamp)) {

                tstamp_clear_flag(tstamp, TS_NOPPS);

                #ifndef AUTO_CLEAR_FLAGS
                tstamp_clear_flag_reset_pps(tstamp);
                #endif
            }
        	AUTO_CLEAR(tstamp, TS_NOPPS);
			// printf("PPS received\n");
        	struct timespec ts = {0, 750000000}; // 750ms = 750000000 nanoseconds
        	nanosleep(&ts, NULL);
        } else { // No signal/fix from PPS
        	tstamp_raise_flag(tstamp, TS_NOPPS);
			// printf("PPS not received\n");
        	sleep(1);
        }
    }
    
    // This point should never be reached
    return NULL;
}

void tstamp_gga_read(TimeStamp* tstamp) {

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
            				
								AUTO_CLEAR(tstamp, TS_OVTIME);
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
									tstamp_raise_flag(tstamp, TS_NOTIME);
								} else {
									// printf("System time is within %d minutes of GNGGA time. difference %d min\n", threshold_minutes, minute_difference);
                                    AUTO_CLEAR(tstamp, TS_NOTIME);
								}

								pthread_mutex_unlock(&m_tstamp_lock);
								
							} 
							else {
								// printf("not checking time. delta sec between current OS and PPS sampled time is greater than 1s: %u ns \n", dnsec);
								
                                tstamp_raise_flag(tstamp, TS_OVTIME);
								tstamp_raise_flag(tstamp, TS_NOTIME);
							}
							
						}
					
					}
					
				}
				
			}
		
		}
		
	}

}

void *ggaAcqThreadFcn(void *ptr) {
	TimeStamp* tstamp = (TimeStamp*)ptr;

	sleep(1);
	
    for(;;) {
        int res = uart_read();
        if (res > 0) { // Search GGA sentence
            
            if(tstamp_get_flag_reset_gga(tstamp)) {
                tstamp_clear_flag(tstamp, TS_NOUART);
                tstamp_clear_flag(tstamp, TS_OVTIME);
                tstamp_clear_flag(tstamp, TS_NOTIME);

                #ifndef AUTO_CLEAR_FLAGS
                tstamp_clear_flag_reset_gga(tstamp);
                #endif
            }
        	tstamp_gga_read(tstamp);
        } else {	// No data from UART
        	tstamp_raise_flag(tstamp, TS_NOUART);
			tstamp_raise_flag(tstamp, TS_OVTIME);
			tstamp_raise_flag(tstamp, TS_NOTIME);
        	sleep(1);
        }
    }
    
    // This point should never be reached
    return NULL;
}

TimeStamp* tstamp_create(void) {
	static TimeStamp tstamp; // Allocazione statica
	
	tstamp.threadStarted = 0;
	tstamp.m_status = TS_NOPPS + TS_NOUART + TS_OVTIME + TS_NOTIME;
	pthread_mutex_init(&tstamp.m_status_mutex, NULL);
	
	return &tstamp;
}

void tstamp_destroy(TimeStamp* tstamp) {
	if (!tstamp) {
		return;
	}
	
	if (tstamp->threadStarted) {
        pthread_cancel(tstamp->ggaAcqThreadInfo);
        pthread_join(tstamp->ggaAcqThreadInfo, NULL);    
        pthread_cancel(tstamp->ppsAcqThreadInfo);
        pthread_join(tstamp->ppsAcqThreadInfo, NULL);
        uart_uninit();
        hk_fpga_uninit();
    }
    pthread_mutex_destroy(&tstamp->m_status_mutex);
    // Nessuna free() necessaria - allocazione statica
}

int tstamp_init(TimeStamp* tstamp) {
	if (!tstamp) {
		return -1;
	}

	pthread_mutex_init(&m_tstamp_lock, NULL);

	int res = hk_fpga_init();
    if (res < 0) {
		fprintf(stderr, "tstamp_init: Error: hk_fpga_init() failed\n");
		return -1;
	}
	
	res = uart_init();
	if (res < 0) {
		fprintf(stderr, "tstamp_init: Error: uart_init() failed\n");
		hk_fpga_uninit();
		return -1;
	}

	// Pass tstamp pointer to threads so they can access instance methods
	res = pthread_create(&tstamp->ppsAcqThreadInfo, NULL, ppsAcqThreadFcn, tstamp);
	if (res < 0) {
		fprintf(stderr, "tstamp_init: Error: pps acquisition thread creation failed\n");
		uart_uninit();
        hk_fpga_uninit();
        return -1;
	}
	    
    res = pthread_create(&tstamp->ggaAcqThreadInfo, NULL, ggaAcqThreadFcn, tstamp);
    if (res < 0) {
		fprintf(stderr, "tstamp_init: Error: gga sentence acquisition thread creation failed\n");
		pthread_cancel(tstamp->ppsAcqThreadInfo);
    	pthread_join(tstamp->ppsAcqThreadInfo, NULL);
		uart_uninit();
        hk_fpga_uninit();
        return -1;
	}
    
    tstamp->threadStarted = 1;
	
	return 0;
}

void tstamp_cleanup(TimeStamp* tstamp) {
	if (!tstamp) {
		return;
	}

	 if (tstamp->threadStarted) {
    
        pthread_cancel(tstamp->ggaAcqThreadInfo);
        pthread_join(tstamp->ggaAcqThreadInfo, NULL);
        
        pthread_cancel(tstamp->ppsAcqThreadInfo);
        pthread_join(tstamp->ppsAcqThreadInfo, NULL);
        
        uart_uninit();
        
        hk_fpga_uninit();

        tstamp->threadStarted = 0;
        
    }
}

uint32_t tstamp_read(TimeStamp* tstamp, CurrentTime *currTime) {
	if (!tstamp || !currTime) {
		return 0xFF; // Error status
	}

	pthread_mutex_lock(&m_tstamp_lock);
	
	StatusFlags currentStatus = tstamp_get_flags(tstamp); // Get current status
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

void tstamp_compute_absolute_time(TimeStamp* tstamp, const struct timespec *ts, CurrentTime *currTime, AbsoluteTime *absTime) {
	if (!tstamp || !ts || !currTime || !absTime) {
		return;
	}

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
void tstamp_raise_flag(TimeStamp* tstamp, TimeSts flag) {
	if (!tstamp) {
		return;
	}
	pthread_mutex_lock(&tstamp->m_status_mutex);
	tstamp->m_status |= (uint32_t)flag;
	pthread_mutex_unlock(&tstamp->m_status_mutex);
}

void tstamp_clear_flag(TimeStamp* tstamp, TimeSts flag) {
	if (!tstamp) {
		return;
	}
	pthread_mutex_lock(&tstamp->m_status_mutex);
	tstamp->m_status &= ~((uint32_t)flag);
	pthread_mutex_unlock(&tstamp->m_status_mutex);
}

StatusFlags tstamp_get_flags(TimeStamp* tstamp) {
	if (!tstamp) {
		return 0xFF; // Error status
	}
	pthread_mutex_lock(&tstamp->m_status_mutex);
	StatusFlags currentStatus = tstamp->m_status;
	pthread_mutex_unlock(&tstamp->m_status_mutex);
	return currentStatus;
}

void tstamp_enable_flags_reset(TimeStamp* tstamp) {
	if (!tstamp) {
		return;
	}
	tstamp_set_flag_reset_pps(tstamp);
	tstamp_set_flag_reset_gga(tstamp);
}

uint8_t tstamp_get_flag_reset_pps(TimeStamp* tstamp) {
	if (!tstamp) {
		return 0;
	}
	pthread_mutex_lock(&tstamp->m_status_reset_mutex);
    uint8_t f_reset_pps = tstamp->f_reset_pps;
	pthread_mutex_unlock(&tstamp->m_status_reset_mutex);
    return f_reset_pps;
}

uint8_t tstamp_get_flag_reset_gga(TimeStamp* tstamp) {
	if (!tstamp) {
		return 0;
	}
	pthread_mutex_lock(&tstamp->m_status_reset_mutex);
    uint8_t f_reset_gga = tstamp->f_reset_gga;
	pthread_mutex_unlock(&tstamp->m_status_reset_mutex);
    return f_reset_gga;

}

void tstamp_set_flag_reset(TimeStamp* tstamp, uint8_t* reset_flag_x, uint8_t value) {
	if (!reset_flag_x) {
		return;
	}
	pthread_mutex_lock(&tstamp->m_status_reset_mutex);
	*reset_flag_x = value;
	pthread_mutex_unlock(&tstamp->m_status_reset_mutex);
}

void tstamp_set_flag_reset_gga(TimeStamp* tstamp) {
	if (!tstamp) {
		return;
	}
	tstamp_set_flag_reset(tstamp, &tstamp->f_reset_gga, 1);
}

void tstamp_set_flag_reset_pps(TimeStamp* tstamp) {
	if (!tstamp) {
		return;
	}
	tstamp_set_flag_reset(tstamp, &tstamp->f_reset_pps, 1);
}

void tstamp_clear_flag_reset_gga(TimeStamp* tstamp) {
	if (!tstamp) {
		return;
	}
	tstamp_set_flag_reset(tstamp, &tstamp->f_reset_gga, 0);
}

void tstamp_clear_flag_reset_pps(TimeStamp* tstamp) {
	if (!tstamp) {
		return;
	}
	tstamp_set_flag_reset(tstamp, &tstamp->f_reset_pps, 0);
}