#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hk_fpga.h"

/* @brief Pointer to FPGA control registers. */
hk_fpga_reg_mem_t *g_hk_fpga_reg_mem = NULL;

/* @brief The memory file descriptor used to mmap() the FPGA space. */
int                g_hk_fpga_mem_fd = -1;

/*--------------------------------------------------------------------------------------*
 * Init FPGA memory buffers
 *
 * Open memory device and performs memory mapping
 * has already been established it unmaps logical memory regions and close apparent
 * file descriptor.
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is printed on standard error device
 *--------------------------------------------------------------------------------------*/
int hk_fpga_init(void) {

	void *page_ptr;
    long page_addr, page_off, page_size;
    
    page_size = sysconf(_SC_PAGESIZE);

    /* If maybe needed, cleanup the FD & memory pointer */
    if(hk_fpga_uninit() < 0) {
    	return -1;
    }
        
    g_hk_fpga_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if(g_hk_fpga_mem_fd < 0) {
        fprintf(stderr, "open(/dev/mem) failed: %s\n", strerror(errno));
        return -1;
    }

    page_addr = HK_FPGA_BASE_ADDR & (~(page_size-1));
    page_off  = HK_FPGA_BASE_ADDR - page_addr;

    page_ptr = mmap(NULL, HK_FPGA_BASE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_hk_fpga_mem_fd, page_addr);
    if((void *)page_ptr == MAP_FAILED) {
        fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
        hk_fpga_uninit();
        return -1;
    }
    g_hk_fpga_reg_mem = (hk_fpga_reg_mem_t*)((uint8_t*)page_ptr + page_off);
    
    return 0;

}

/*--------------------------------------------------------------------------------------*
 * Cleanup access to FPGA memory buffers
 *
 * Function optionally cleanups access to FPGA memory buffers, i.e. if access
 * has already been established it unmaps logical memory regions and close apparent
 * file descriptor.
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is printed on standard error device
 *--------------------------------------------------------------------------------------*/
int hk_fpga_uninit(void) {

    /* optionally unmap memory regions  */
    if (g_hk_fpga_reg_mem) {
        if (munmap(g_hk_fpga_reg_mem, HK_FPGA_BASE_SIZE) < 0) {
            fprintf(stderr, "munmap() failed: %s\n", strerror(errno));
            return -1;
        }
        /* Update memory pointers */
        g_hk_fpga_reg_mem = NULL;
    }

    /* optionally close file descriptor */
    if(g_hk_fpga_mem_fd >= 0) {
        close(g_hk_fpga_mem_fd);
        g_hk_fpga_mem_fd = -1;
    }

    return 0;
    
}