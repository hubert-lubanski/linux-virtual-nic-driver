/**************************************************************************//**
 * @file     zsonet.h
 * @brief    MIMUW ZSO-NET Device Access Layer Header File
 * @version  V0.0.1
 ******************************************************************************/
/*
 * Na wzór nagłówka mikrokontrolera STM32 :)
*/
#ifndef ZSONET_DEVICE_H
#define ZSONET_DEVICE_H

#include "utils.h"
#include <linux/types.h>

#ifdef __NO_USE_MMIO
/* When using io(read/write) procedures to work with MMIO memory the volatile
   part will raise warnings about wrong pointer type (being volatile).
   So we use the volatile specifier only for bare-metal accessing.
*/
    #define __I     volatile const  /*!< 'read only' permissions */
    #define __O     volatile        /*!< 'write only' permissions */
    #define __IO    volatile        /*!< 'read / write' permissions */
#else
    #define __I     const           /*!< 'read only' permissions */
    #define __O                     /*!< 'write only' permissions */
    #define __IO                    /*!< 'read / write' permissions */
#endif /* __USE_MMIO */

/* Quick generation of L and H register for accesing lower and upper half
   NOTE: This is correct only for Little-endiand access patterns!   */
#define __ACCESS_LO_HI(type, half_type, name) \
    union { struct {half_type L##name; half_type H##name;}; type name; }

/* ************************************************************************** **
              ZSONET Device Parameters and Registers definitions
** ************************************************************************** */

/* Section 1: PCI ids */
#define ZSONET_VENDOR_ID 0x0250
#define ZSONET_DEVICE_ID 0x250e

/***************************  MAC Address Registers  **************************/
#define MAC_R0      0x00    // Byte 0 device MAC address register
#define MAC_R1      0x01    // Byte 1 device MAC address register
#define MAC_R2      0x02    // Byte 2 device MAC address register
#define MAC_R3      0x03    // Byte 3 device MAC address register
#define MAC_R4      0x04    // Byte 4 device MAC address register
#define MAC_R5      0x05    // Byte 5 device MAC address register
#define MAC_Base    MAC_R0  // device MAC address base register

/**********************  Transmission Control Registers  **********************/
#define TX_SR_0     0x10    // TX Buffer 0 status register
#define TX_SR_1     0x14    // TX Buffer 1 status register
#define TX_SR_2     0x18    // TX Buffer 2 status register
#define TX_SR_3     0x1c    // TX Buffer 3 status register

#define TX_BUF_0    0x20    // TX DMA buffer 0 address register 
#define TX_BUF_1    0x24    // TX DMA buffer 1 address register 
#define TX_BUF_2    0x28    // TX DMA buffer 2 address register 
#define TX_BUF_3    0x2c    // TX DMA buffer 3 address register 
#define TX_Base     TX_SR_0

/************************  Reception Control Registers  ***********************/
#define RX_BUF        0x30  // RX DMA buffer address register
#define RX_BUF_SIZE   0x34  // RX DMA buffer size register
#define RX_BUF_ROFF   0x38  // RX DMA last-read offset reg
#define RX_BUF_WOFF   0x3c  // RX DMA last-write offset reg

#define RX_SR         0x40  // RX Status register
#define RX_MFR        0x44  // RX Missed frames count register
#define RX_Base       RX_BUF

/************************  Interrupt Control Registers  ***********************/
#define INTR_MR       0x50  // Interrupt masking register
#define INTR_SR       0x54  // Interrupt status register
#define INTR_Base     INTR_MR


#define ZSONET_ENR    0x60  // Enable register


/*************** Bit definitions for Interrupt Status register ****************/
#define INTR_SR_RO_Bit  0x0
#define INTR_SR_ROF     (1ul << INTR_SR_RO_Bit)    // RX-OK interrupt flag
#define INTR_SR_TO_Bit  0x1
#define INTR_SR_TOF     (1ul << INTR_SR_TO_Bit)    // TX-OK interrupt flag

/******************* Bit definitions for RX Status register *******************/
#define RX_SR_HD_Bit    0x0
#define RX_SR_HAS_DATA  (1ul << RX_SR_HD_Bit)       // RX HAS-DATA Flag

/******************* Bit definitions for TX Status register *******************/
#define TX_SR_FIN_Bit   0x0
#define TX_SR_FINISHED  (1ul << TX_SR_FIN_Bit)      // TX Finished bit

#define ZSONET_HEADER_SIZE  4
#define ZSONET_FCS_SIZE     4

#define ZSONET_BAR_SIZE 0x100


/********************** Device parts C struct definitions *********************/
// With offset assertions
typedef union
{
    struct __attribute__((packed))
    {
        __I uint8_t MAC0;
        __I uint8_t MAC1;
        __I uint8_t MAC2;
        __I uint8_t MAC3;
        __I uint8_t MAC4;
        __I uint8_t MAC5;
    };
    __I uint8_t __MAC[6];
} MAC_TypeDef;  assert_size(MAC_TypeDef, 6);

assert_offset(MAC_TypeDef, MAC0, (MAC_R0 - MAC_Base));
assert_offset(MAC_TypeDef, MAC1, (MAC_R1 - MAC_Base));
assert_offset(MAC_TypeDef, MAC2, (MAC_R2 - MAC_Base));
assert_offset(MAC_TypeDef, MAC3, (MAC_R3 - MAC_Base));
assert_offset(MAC_TypeDef, MAC4, (MAC_R4 - MAC_Base));
assert_offset(MAC_TypeDef, MAC5, (MAC_R5 - MAC_Base));

typedef struct
{
    __ACCESS_LO_HI(__IO uint32_t, __IO uint16_t, SR0);
    __ACCESS_LO_HI(__IO uint32_t, __IO uint16_t, SR1);
    __ACCESS_LO_HI(__IO uint32_t, __IO uint16_t, SR2);
    __ACCESS_LO_HI(__IO uint32_t, __IO uint16_t, SR3);

    __O uint32_t BUF0;
    __O uint32_t BUF1;
    __O uint32_t BUF2;
    __O uint32_t BUF3;
} TX_TypeDef;   assert_size(TX_TypeDef, 32);

assert_offset(TX_TypeDef, SR0,  (TX_SR_0 - TX_Base));
assert_offset(TX_TypeDef, SR1,  (TX_SR_1 - TX_Base));
assert_offset(TX_TypeDef, SR2,  (TX_SR_2 - TX_Base));
assert_offset(TX_TypeDef, SR3,  (TX_SR_3 - TX_Base));
assert_offset(TX_TypeDef, BUF0, (TX_BUF_0 - TX_Base));
assert_offset(TX_TypeDef, BUF1, (TX_BUF_1 - TX_Base));
assert_offset(TX_TypeDef, BUF2, (TX_BUF_2 - TX_Base));
assert_offset(TX_TypeDef, BUF3, (TX_BUF_3 - TX_Base));

typedef struct
{
    __O  uint32_t BUF;
    __O  uint32_t BSIZE;
    __O  uint32_t BRDOFF;
    __I  uint32_t BWROFF;

    __I  uint32_t SR;
    __IO uint32_t MFR;

} RX_TypeDef;   assert_size(RX_TypeDef, 24);

assert_offset(RX_TypeDef, BUF,    (RX_BUF - RX_Base));
assert_offset(RX_TypeDef, BSIZE,  (RX_BUF_SIZE - RX_Base));
assert_offset(RX_TypeDef, BRDOFF, (RX_BUF_ROFF - RX_Base));
assert_offset(RX_TypeDef, BWROFF, (RX_BUF_WOFF - RX_Base));
assert_offset(RX_TypeDef, SR,     (RX_SR - RX_Base));
assert_offset(RX_TypeDef, MFR,    (RX_MFR - RX_Base));

typedef struct
{
    __IO uint32_t MR;
    __IO uint32_t SR;
} INTR_TypeDef; assert_size(INTR_TypeDef, 8);

assert_offset(INTR_TypeDef, MR, (INTR_MR - INTR_Base));
assert_offset(INTR_TypeDef, SR, (INTR_SR - INTR_Base));

typedef struct
{
    MAC_TypeDef   MAC_INFO;
    TX_TypeDef    TX    __attribute__((aligned(16)));
    RX_TypeDef    RX    __attribute__((aligned(16)));
    INTR_TypeDef  INTR  __attribute__((aligned(16)));
    __IO uint32_t ENR   __attribute__((aligned(16)));
} ZSONET_TypeDef;

assert_offset(ZSONET_TypeDef, MAC_INFO, MAC_Base);
assert_offset(ZSONET_TypeDef, TX,       TX_Base);
assert_offset(ZSONET_TypeDef, RX,       RX_Base);
assert_offset(ZSONET_TypeDef, INTR,     INTR_Base);
assert_offset(ZSONET_TypeDef, ENR,      ZSONET_ENR);
static_assert(sizeof(ZSONET_TypeDef) <= ZSONET_BAR_SIZE, "MMIO BAR SIZE OVERFLOW!");


#endif // ZSONET_DEVICE_H
