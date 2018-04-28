/*
	Chad Coates
	ECE 373
	Homework #8
	June 4, 2017

	This is the ACME: ece_led driver
*/

#ifndef ACME_H
#define ACME_H

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>

#define DEVCOUNT					1
#define DEVNAME						"ece_led"
#define BLINKRATE					2
#define LEDCTL 						0x00E00
#define LED_OFF						0x0
#define GREEN							0x0E0E00
#define RGREEN						0x000E00
#define LGREEN						0x0E0000

#define ACME_RXD	 				0x10	//number of descriptors
#define DESC_SIZE					0x800 // 2048 bytes
#define ICR								0x000C0
#define LSC_INT						0x81000004
#define RXQ_INT						0x80100080	
#define RDBAL							0x02800
#define RDBAH							0x02804
#define RDLEN							0x02808
#define RDH								0x02810
#define RDT								0x02818
#define RCTL							0x00100
#define UPE								(1<<3)	
#define MPE								(1<<4)
#define BAM								(1<<15)
#define EN								(1<<1)
#define RXQ								(1<<20)
#define CTRL							0x00000
#define IMC								0x000D8
#define IMS								0x000D0
#define LSC								(1<<2)
#define INT_ASSERTED			(1<<31)
#define GCR								0x05B00
#define GCR2							0x05B64
#define MDIC							0x00020
#define CTRL_RST					(1<<26)

#define ACME_GET_DESC(R, i, type) (&(((struct type *)((R).desc))[i]))
#define ACME_RX_DESC(R, i)	ACME_GET_DESC(R, i, acme_rx_desc)

//static void acme_timer_cb(unsigned long);

struct pci_hw{
	void __iomem *hw_addr;
	void __iomem *led_addr;
};

struct acme_ring{
	struct acme_rx_desc *desc;
	dma_addr_t dma;
	u8 *wtf[ACME_RXD];
	unsigned int size;
	unsigned int count;
	u16 next_to_use;
	u16 next_to_clean;
};

struct acme_dev{
	struct cdev cdev;
	struct pci_hw hw;
	struct work_struct task;
	struct acme_ring rx_ring;
	//u32 rxd_cmd;
	u32 led_ctl;
	//int irq_info;
	int blink_rate; //blinks per second
} *acme_devp;

struct acme_rx_desc{
	__le64 buff_addr;
	__le16 length;
	__le16 csum;
	u8 status;
	u8 errors;
	__le16 special;
};

struct ring_info{
	int head;
	int tail;
	u32 icr;
	u32 rh;
	u32 rl;
	u32 len;
	u32 led;
};

#endif
