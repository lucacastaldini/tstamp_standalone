#ifndef __HK_FPGA_H__
#define __HK_FPGA_H__

#include <stdint.h>
#include <unistd.h>

// Starting address of FPGA registers handling Housekeeping module. 
#define HK_FPGA_BASE_ADDR 	0x40000000UL 

// The size of FPGA registers handling Housekeeping module. 
#define HK_FPGA_BASE_SIZE 	0x30000 

// GPIO direction
typedef enum {
	HK_FPGA_GPIO_IN_ALL = 0x00000000, 
} hk_fpga_gpio_out_t;

// GPIO bit position
typedef enum {
	HK_FPGA_GPIO_BIT7 = (1<<7),
	HK_FPGA_GPIO_BIT6 = (1<<6),
	HK_FPGA_GPIO_BIT5 = (1<<5),
	HK_FPGA_GPIO_BIT4 = (1<<4),
	HK_FPGA_GPIO_BIT3 = (1<<3),
	HK_FPGA_GPIO_BIT2 = (1<<2),
	HK_FPGA_GPIO_BIT1 = (1<<1),
	HK_FPGA_GPIO_BIT0 = 1,
} hk_fpga_gpio_bit_t;


/* FPGA registry structure for Housekeeping core module.
 * This structure is direct image of physical FPGA memory. It assures
 * direct read/write FPGA access when it is mapped to the appropriate memory address
 * through /dev/mem device.
 */
typedef struct {

	/* off  [0x00] ID:
     * bit  [31:4] - Reserved
     * bit   [3:0] - Design ID
     */
    uint32_t id;
    
    /* off  [0x04] DNA part 1:
     * bit  [31:0] - DNA[31:0]
     */
    uint32_t dna_lo;
    
    /* off  [0x08] DNA part 2:
     * bit [31:25] - Reserved
     * bit  [24:0] - DNA[56:32]
     */
    uint32_t dna_hi;
    
    /* off  [0x0C] Digital Loopback:
     * bit  [31:1] - Reserved
     * bit     [0] - digital_loop
     */
    uint32_t digital_loop;
    
    /* off  [0x10] Expansion connector direction P:
     * bit  [31:8] - Reserved
     * bit   [7:0] - Direction of P lines (0 out, 1 in)
     */
    uint32_t dir_p;
    
    /* off  [0x14] Expansion connector direction N:
     * bit  [31:8] - Reserved
     * bit   [7:0] - Direction of N lines (0 out, 1 in)
     */
    uint32_t dir_n;
    
    /* off  [0x18] Expansion connector output P:
     * bit  [31:8] - Reserved
     * bit   [7:0] - Output of P lines
     */
    uint32_t out_p;
    
    /* off  [0x1C] Expansion connector output N:
     * bit  [31:8] - Reserved
     * bit   [7:0] - Output of N lines
     */
    uint32_t out_n;
    
    /* off  [0x20] Expansion connector input P:
     * bit  [31:8] - Reserved
     * bit   [7:0] - Input of P lines
     */
    uint32_t in_p;
    
    /* off  [0x24] Expansion connector input N:
     * bit  [31:8] - Reserved
     * bit   [7:0] - Input of N lines
     */
    uint32_t in_n;
    
    /* off  [0x30] Led control:
     * bit  [31:8] - Reserved
     * bit   [7:0] - LEDs 7-0
     */
    uint32_t led;

} hk_fpga_reg_mem_t;

extern hk_fpga_reg_mem_t	*g_hk_fpga_reg_mem;
extern int                 	g_hk_fpga_mem_fd;

/* function declarations, detailed descriptions is in apparent implementation file  */
int hk_fpga_init(void);
int hk_fpga_uninit(void);

#endif /* __HK_FPGA_H__ */
