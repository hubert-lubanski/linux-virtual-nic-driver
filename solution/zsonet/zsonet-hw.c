#include "zsonet-hw.h"

/********************************* Bare metal *********************************/

void zsonet_hw_reset(zsonet_device_ptr device)
{
    // Reset Interrupts
    zsonet_clear_intr_mr(device);
    zsonet_clear_intr_sr(device);
    // RX Missed Frame Register
    zsonet_rx_clear_missed(device);
}

void zsonet_boot(
    zsonet_device_ptr device,
    struct zsonet_dma_buffer tx_buf[4],
    struct zsonet_dma_buffer rx_buf)
{
    // TX DMA Buffers
    iowrite32(tx_buf[0].dma_addr, &device->TX.BUF0);
    iowrite32(tx_buf[1].dma_addr, &device->TX.BUF1);
    iowrite32(tx_buf[2].dma_addr, &device->TX.BUF2);
    iowrite32(tx_buf[3].dma_addr, &device->TX.BUF3);
    // TX Status Registers
    iowrite32(0, &device->TX.SR0);
    iowrite32(0, &device->TX.SR1);
    iowrite32(0, &device->TX.SR2);
    iowrite32(0, &device->TX.SR3);
    // RX DMA Buffer
    iowrite32(rx_buf.dma_addr, &device->RX.BUF);
    iowrite32(rx_buf.size, &device->RX.BSIZE);
    // RX Read Offset register
    iowrite32(0, &device->RX.BRDOFF);
    // RX Missed Frame Register
    zsonet_rx_clear_missed(device);
    // Reset INTR Status Register
    zsonet_clear_intr_sr(device);
    // Turn on RX OK int and TX OK int
    iowrite32(INTR_SR_ROF | INTR_SR_TOF, &device->INTR.MR);

    // Start
    zsonet_set_enable(device);
}

void zsonet_poweroff(zsonet_device_ptr device)
{
    zsonet_clear_enable(device);
    zsonet_clear_intr_mr(device);
    zsonet_clear_intr_sr(device);
}