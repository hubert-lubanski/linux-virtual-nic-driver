#ifndef ZSONET_HARDWARE_H
#define ZSONET_HARDWARE_H

#include "zsonet.h"
#include "zsonet-driver-common.h"
#include <linux/uaccess.h>

#ifdef __NO_USE_MMIO
#define device_write32(val, reg)    reg = (val)
#define device_write8(val, reg)     reg = (val)

#define device_read32(reg)  (reg)
#define device_read8(reg)   (reg)

#else

#define device_write32(val, reg) iowrite32((val), &(reg))
#define device_write8(val, reg)  iowrite8((val), &(reg))
    
#define device_read32(reg)  ioread32(&(reg))
#define device_read8(reg)  ioread8(&(reg))

#endif /* __NO_USE_MMIO */

static inline
void zsonet_clear_intr_sr(zsonet_device_ptr device)
{
    device_write32((uint32_t)(-1), device->INTR.SR);
}

static inline
void zsonet_clear_intr_mr(zsonet_device_ptr device)
{
    device_write32(0, device->INTR.MR);
}

static inline
void zsonet_set_enable(zsonet_device_ptr device)
{
    device_write32(0xc0ffee, device->ENR);
}

static inline
void zsonet_clear_enable(zsonet_device_ptr device)
{
    device_write32(0x0, device->ENR);
}


// NOTE This will initiate o 0-byte packet send
static inline
void zsonet_tx_sr_reset(zsonet_device_ptr device, int tx_idx)
{
    switch (tx_idx)
    {
        case 0: device_write32(0x0, device->TX.SR0); break;
        case 1: device_write32(0x0, device->TX.SR1); break;
        case 2: device_write32(0x0, device->TX.SR2); break;
        case 3: device_write32(0x0, device->TX.SR3); break;
    }
}


static inline
int zsonet_tx_finished(zsonet_device_ptr device, int tx_idx)
{

    switch (tx_idx)
    {
        case 0: return (device_read32(device->TX.SR0) & TX_SR_FINISHED);
        case 1: return (device_read32(device->TX.SR1) & TX_SR_FINISHED);
        case 2: return (device_read32(device->TX.SR2) & TX_SR_FINISHED);
        case 3: return (device_read32(device->TX.SR3) & TX_SR_FINISHED);
        default:
            return -1;
    }
}

static inline
void zsonet_hw_tx(zsonet_device_ptr device, uint8_t tx_idx, uint32_t len)
{
    uint32_t val = ((len & 0xffff) << 16);
    switch (tx_idx)
    {
        case 0: device_write32(val, device->TX.SR0); break;
        case 1: device_write32(val, device->TX.SR1); break;
        case 2: device_write32(val, device->TX.SR2); break;
        case 3: device_write32(val, device->TX.SR3); break;
    }
}

static inline
int zsonet_rx_has_frame(zsonet_device_ptr device)
{
    return (device_read32(device->RX.SR) & RX_SR_HAS_DATA);
}

static inline
int zsonet_rx_missed_count(zsonet_device_ptr device)
{
    return device_read32(device->RX.MFR);
}

static inline
void zsonet_rx_clear_missed(zsonet_device_ptr device)
{
    device_write32(0xca11ab1e, device->RX.MFR);
}


static inline
void zsonet_copy_mac_address(zsonet_device_ptr device, uint8_t *dest)
{
    dest[0] = device_read8(device->MAC_INFO.MAC0);
    dest[1] = device_read8(device->MAC_INFO.MAC1);
    dest[2] = device_read8(device->MAC_INFO.MAC2);
    dest[3] = device_read8(device->MAC_INFO.MAC3);
    dest[4] = device_read8(device->MAC_INFO.MAC4);
    dest[5] = device_read8(device->MAC_INFO.MAC5);
}


void zsonet_boot(zsonet_device_ptr device,
                 struct zsonet_dma_buffer tx_buf[4],
                 struct zsonet_dma_buffer rx_buf);

void zsonet_poweroff(zsonet_device_ptr device);

void zsonet_hw_reset(zsonet_device_ptr device);

#endif /* ZSONET_HARDWARE_H */