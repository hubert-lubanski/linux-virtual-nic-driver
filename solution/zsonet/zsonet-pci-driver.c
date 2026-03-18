#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/string.h>


#include "zsonet-hw.h"
#include "zsonet-driver-common.h"

#include "zsonet-net-driver.h"

#define ON_ERROR_GOTO(evar, expr, label) if ((evar = (expr))) goto label

#define ZSONET_MAX_FRAME_SIZE   \
    (ZSONET_HEADER_SIZE + ETH_FRAME_LEN + ETH_FCS_LEN + ZSONET_FCS_SIZE)

#define ZSONET_TX_DMA_BUFFERS   4
#define ZSONET_TX_BUFFER_SIZE   ETH_FRAME_LEN

#define ZSONET_RX_BUFFER_SIZE   (9 * PAGE_SIZE)

#define DRV_NAME        "zsonet"
#define DRV_DESCRIPTION "MIMUW(R) ZSONET Network Driver"
#define DRV_COPYRIGHT   "Copyright(c) 2023-2024 MIMUW ZSO"

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL v2");


extern const struct net_device_ops zsonet_netdev_ops;

/********************************* PCI Driver *********************************/

/* Helpers */
static int zsonet_alloc(struct zsonet_device *zdev)
{
    // alloc TX buffers
    for (int i = 0; i < ZSONET_TX_DMA_BUFFERS; ++i){
        zdev->tx_buffers[i].cpu_addr
            = dma_alloc_coherent(&zdev->pdev->dev,
                                 ZSONET_TX_BUFFER_SIZE,
                                 &zdev->tx_buffers[i].dma_addr,
                                 GFP_KERNEL);
        
        if (!zdev->tx_buffers[i].cpu_addr) {
            return -ENOMEM;
        }
        zdev->tx_buffers[i].size = ZSONET_TX_BUFFER_SIZE;
    }
    // alloc RX buffer
    zdev->rx_buffer.cpu_addr = dma_alloc_coherent(&zdev->pdev->dev,
                                                  ZSONET_RX_BUFFER_SIZE,
                                                  &zdev->rx_buffer.dma_addr,
                                                  GFP_KERNEL);
    if (!zdev->rx_buffer.cpu_addr)
        return -ENOMEM;

    zdev->rx_buffer.size = ZSONET_RX_BUFFER_SIZE;

    return 0;
}

static void zsonet_free(struct zsonet_device *zdev)
{
    /* free TX buffers */
    for (int i = 0; i < ZSONET_TX_DMA_BUFFERS; ++i){
        dma_free_coherent(&zdev->pdev->dev, ZSONET_TX_BUFFER_SIZE,
                          zdev->tx_buffers[i].cpu_addr,
                          zdev->tx_buffers[i].dma_addr);
    }
    /* free RX buffer */
    dma_free_coherent(&zdev->pdev->dev, ZSONET_RX_BUFFER_SIZE,
                      zdev->rx_buffer.cpu_addr,
                      zdev->rx_buffer.dma_addr);
    
}

static void zsonet_initial_hw_state(struct zsonet_device *zdev)
{
    zdev->current_tx = 0;
    zdev->tx_ready = 0b1111;
    zdev->read_offset = 0;
}

/* IRQ Handler */
static irqreturn_t zsonet_IRQ_Handler(int IRQn, void *dev_specific_ptr)
{
    /* paranoid */
    if (!dev_specific_ptr)
        return IRQ_NONE;

    struct net_device *netdev = dev_specific_ptr;
    struct zsonet_device *zdev = netdev_priv(netdev);

    /* paranoid v2 */
    if (!zdev)
        return IRQ_NONE;

    zsonet_device_ptr device = zdev->zsodev_mm;
    
    /* paranoid v3 */
    if (! device)
        return IRQ_NONE;

    int handled = 0;
    unsigned long flags;

    /* Lock the device */
    spin_lock_irqsave(&zdev->lock, flags);

    uint32_t isr = ioread32(&device->INTR.SR) & ioread32(&device->INTR.MR);

    if (isr & INTR_SR_ROF) {
        handled = 1;
        zsonet_rx(netdev);
    }

    if (isr & INTR_SR_TOF) {
        handled = 1;
        // Let's hope the compiler sees the obvious unroll here
        for (int i = 0; i < 4; ++i)
            if (zsonet_tx_finished(device, i) && !(zdev->tx_ready & (1ul << i))){
                zsonet_mark_tx_free(zdev, i);
                // If we have freed the next buffer - wake up the queue if needed
                if (zdev->current_tx == i && unlikely(netif_queue_stopped(netdev)))
                    netif_wake_queue(netdev);
            }
    }
    
    // Clear processed interrupts the proper way!
    iowrite32(isr, &device->INTR.SR);

    /* Unlock the device */
    spin_unlock_irqrestore(&zdev->lock, flags);

    return IRQ_RETVAL(handled);
}

static int zsonet_probe(struct pci_dev *pdev, const struct pci_device_id *pci_id)
{
    int error;
    struct net_device *netdev;
    struct zsonet_device *zdev;

    netdev = alloc_etherdev(sizeof(struct zsonet_device));
    ON_ERROR_GOTO(error, (netdev ? 0 : -ENOMEM), alloc_err);
    pci_set_drvdata(pdev, netdev);

    /* Network device features and interface */
    netdev->netdev_ops = &zsonet_netdev_ops;
    netdev->hw_features |= NETIF_F_RXFCS;

    /* Setup references */
    zdev = netdev_priv(netdev);
    zdev->pdev = pdev;
    zdev->netdev = netdev;

    /* Setup zsonet_device ardware states fields */
    zsonet_initial_hw_state(zdev);

    /* Locks */
    spin_lock_init(&zdev->lock);

    /* PCI */
    ON_ERROR_GOTO(error, pci_enable_device(pdev), enable_err);
    ON_ERROR_GOTO(error, dma_set_mask(&pdev->dev, DMA_BIT_MASK(32)), dma_mask_err);
    ON_ERROR_GOTO(error, dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32)), dma_mask_err);
    ON_ERROR_GOTO(error, pci_request_regions(pdev, DRV_NAME), regions_err);

    /* Sysfs hierarchy */
    SET_NETDEV_DEV(netdev, &pdev->dev);

    /* BAR */
    zdev->BAR = pci_iomap(pdev, 0, ZSONET_BAR_SIZE);
    ON_ERROR_GOTO(error, ((zdev->BAR) ? 0 : -ENOMEM), bar_err);

    // Reset pending interrupts just in case
    // zsonet_hw_reset(zdev->zsodev_mm);

    /* IRQ */
    ON_ERROR_GOTO(error, request_irq(pdev->irq, zsonet_IRQ_Handler, IRQF_SHARED, DRV_NAME, netdev), irq_err);

    
    pci_set_master(pdev);   // Enable DMA subsystem

    /* DMA buffers */
    ON_ERROR_GOTO(error, zsonet_alloc(zdev), dma_buf_err);

    /* MAC address */
    uint8_t hw_address[6];
    zsonet_copy_mac_address(zdev->zsodev_mm, hw_address);

    eth_hw_addr_set(netdev, (u8 *)hw_address);
    if (!is_valid_ether_addr(netdev->dev_addr)) {
        pr_warn("zsonet driver: Invalid MAC address of ZSONET device!\n");
        error = -EAGAIN;
        goto hw_addr_err; 
    }

    /* ZSONET device boot */
    pr_notice("zsonet driver: booting up the device.");
    zsonet_boot(zdev->zsodev_mm, zdev->tx_buffers, zdev->rx_buffer);

    /* Network Device Registration */
    strcpy(netdev->name, "zso_eth%d");
    ON_ERROR_GOTO(error, register_netdev(netdev), net_reg_err);    

    /* Party time! Everything worked (or just not failed) */
    pr_notice("zsonet driver: Ready.");
    pr_info("zsonet driver: MAC address %pM, BAR %p, IRQn %u",
            netdev->dev_addr, zdev->BAR, pdev->irq);

    return 0;
    
net_reg_err:
hw_addr_err:
    pr_notice("zsonet driver: powering off the device.");
    zsonet_poweroff(zdev->zsodev_mm);
dma_buf_err:
    zsonet_free(zdev);
    free_irq(pdev->irq, zdev);
irq_err:
    pci_iounmap(pdev, zdev->BAR);
bar_err:
    pci_release_regions(pdev);
regions_err:
dma_mask_err:
    pci_disable_device(pdev);
enable_err:
    free_netdev(netdev);
alloc_err:
    return error;
}

static void zsonet_remove(struct pci_dev *pdev)
{
    struct net_device *netdev = pci_get_drvdata(pdev);

    if (netdev) {
        struct zsonet_device *zdev = netdev_priv(netdev);
        unregister_netdev(netdev);

        /* Poweroff the device */
        pr_notice("zsonet driver: powering off the device.");
        zsonet_poweroff(zdev->zsodev_mm);

        zsonet_free(zdev);
        free_irq(pdev->irq, netdev);
        pci_iounmap(pdev, zdev->BAR);
        pci_release_regions(pdev);
        pci_disable_device(pdev);

        free_netdev(netdev);
    }
    else {
        pr_err("zsonet driver: No network device provided!");
        // What else could we do in this situation?
        free_irq(pdev->irq, netdev);
        pci_release_regions(pdev);
        pci_disable_device(pdev);
    }
}

/* Kernel structures */
static struct pci_device_id zsonet_id_table[] = {
    { PCI_DEVICE(ZSONET_VENDOR_ID, ZSONET_DEVICE_ID) },
    { 0 }
};

static struct pci_driver zsonet_pci_driver = {
    .name = DRV_NAME,
    .id_table = zsonet_id_table,
    .probe  = zsonet_probe,
    .remove = zsonet_remove
};

/****************************** Module interface ******************************/
static int zsonet_init(void)
{
    pr_info("%s\n", DRV_DESCRIPTION);
    pr_info("%s\n", DRV_COPYRIGHT);
    return pci_register_driver(&zsonet_pci_driver);
}

static void zsonet_exit(void)
{
    pci_unregister_driver(&zsonet_pci_driver);
}

module_init(zsonet_init);
module_exit(zsonet_exit);