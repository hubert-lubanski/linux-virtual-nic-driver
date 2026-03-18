#ifndef ZSONET_DRIVER_H
#define ZSONET_DRIVER_H

#include "zsonet.h"
#include <linux/if_ether.h>

#ifdef __NO_USE_MMIO
#define zsonet_device_ptr  ZSONET_TypeDef *
#else
#define zsonet_device_ptr  ZSONET_TypeDef __iomem *
#endif


struct zsonet_dma_buffer {
    size_t size;
    void *cpu_addr;
    dma_addr_t dma_addr;
};

/* Board specific private data structre */
struct zsonet_device {
    /* OS defined structures */
    struct pci_dev *pdev;
    struct net_device *netdev;

    /* Locks */
    spinlock_t lock;

    /* Pointer to device memory - possible MMIO */
    union
    {
        void __iomem *BAR;
        zsonet_device_ptr zsodev_mm;
    };
    
    /* DMA Buffers */
    struct zsonet_dma_buffer tx_buffers[4]; 
    struct zsonet_dma_buffer rx_buffer; 
    
    /* TX additional info */
    struct sk_buff *tx_skb[4];

    /* Hardware state */
    uint8_t  current_tx : 2;
    uint8_t  tx_ready : 4;
    uint32_t read_offset;
};

static inline
int zsonet_check_tx_ready(struct zsonet_device *zdev) {
    return (zdev->tx_ready & (1ul << zdev->current_tx));
}

static inline
void zsonet_mark_tx_used(struct zsonet_device *zdev, struct sk_buff *skb) {
    zdev->tx_ready &= ~(1ul << zdev->current_tx); 
    zdev->tx_skb[zdev->current_tx] = skb;
    zdev->current_tx++;
}

static inline
void zsonet_mark_tx_free(struct zsonet_device *zdev, uint8_t tx) {
    zdev->tx_ready |= (1ul << tx);
    dev_kfree_skb(zdev->tx_skb[tx]);
}


#endif /* ZSONET_DRIVER_H */