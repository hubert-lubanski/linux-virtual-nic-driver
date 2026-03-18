#include "zsonet.h"

#include "zsonet-net-driver.h"

#include <linux/netdevice.h>
#include <linux/etherdevice.h>

/* Helpers */
static inline
void read_from_cb(void *cb,   uint32_t cb_size,
                  void *dest, uint32_t *r_off,  uint32_t count)
{
    const uint32_t max_read = cb_size - *r_off;

    if (max_read > count) {
        memcpy(dest, (cb + *r_off), count);
        *r_off += count;
    } else {
        memcpy(dest, (cb + *r_off), max_read);
        memcpy(dest + max_read, cb, count - max_read);
        *r_off = count - max_read;
    }
}

static inline
void print_eth_frame(const char *log_level, void *frame, size_t len)
{
    uint8_t *dest_mac   = frame;
    uint8_t *src_mac    = (frame + 6);
    
    void *data          = (frame + 14);

    size_t datalen      = len - 18;
    

    printk("%sDMAC: %pM SMAC: %pM datalen: %zu",
           log_level, dest_mac, src_mac, datalen);
    print_hex_dump(log_level, "", DUMP_PREFIX_OFFSET, 16, 1, data, datalen, 1);
}


/* This function is *only* supposed to be called from interrupt handler
 * by doing so we have ownership of the device via zdev->lock
*/
void zsonet_rx(struct net_device *netdev)
{
    struct sk_buff *skb;
    struct zsonet_device *zdev = netdev_priv(netdev);
    ZSONET_TypeDef __iomem *device = zdev->zsodev_mm;

    /* paranoid */
    if (unlikely(!spin_is_locked(&zdev->lock))) {
        pr_alert("zsonet rx: accessed without device lock!");
        return;
    }
        
    const uint32_t rxb_size = zdev->rx_buffer.size;
    void *rxb_cpu = zdev->rx_buffer.cpu_addr;
    uint32_t datalen;
    
    while (zsonet_rx_has_frame(device)) {
        /* Reading from last remembered offset in buffer.
           The device will not override any data until we update offser register
        */
        read_from_cb(rxb_cpu, rxb_size, &datalen, &zdev->read_offset, 4);

        skb = netdev_alloc_skb(netdev, datalen);
        
        netdev->stats.rx_packets++; // We have sucessfuly recevied a packet
        netdev->stats.rx_bytes += (datalen - 4); // Exclude FCS

        if (!skb) {
            if (printk_ratelimit())
                pr_notice("zsonet rx: low on memory - dropped");
            netdev->stats.rx_dropped++;
            // There is no point in processing further
            // give kernel some space
            break;
        }

        void *skb_dest = skb_put(skb, datalen);
        read_from_cb(rxb_cpu, rxb_size, skb_dest, &zdev->read_offset, datalen);

        skb->protocol = eth_type_trans(skb, netdev);
        // NOTE Should this be here? LDD mention CHECKSUM_HW
        // NOTE What to do with FCS checksum
        skb->ip_summed = CHECKSUM_UNNECESSARY;

        // pr_warn("zsonet rx: packet received sucessfully (len = %u)", datalen-4);
        // print_eth_frame(KERN_NOTICE, skb_dest, datalen);

        // Update device read_offset before next loop
        iowrite32(zdev->read_offset, &device->RX.BRDOFF);

        // Send it
        if (netif_rx(skb) != NET_RX_SUCCESS) {
            if (printk_ratelimit())
                pr_notice("zsonet rx: netif_rx dropped");
            break; // If congestion - give kernel some breathing room
        }
    }
}

netdev_tx_t zsonet_tx(struct sk_buff *skb, struct net_device *netdev)
{
    
    struct zsonet_device *zdev = netdev_priv(netdev);
    zsonet_device_ptr device = zdev->zsodev_mm;
    
    netdev_tx_t status = NETDEV_TX_OK;
    unsigned long flags;

    /* Lock the device */
    spin_lock_irqsave(&zdev->lock, flags);

    // Current TX Buffer CPU adderss
    void *cur_txb_cpu = zdev->tx_buffers[zdev->current_tx].cpu_addr;
    uint32_t len  = skb->len;

    // pr_notice("zsonet tx: received packet to send via TX%d.", zdev->current_tx);
    // print_eth_frame(KERN_NOTICE, skb->data, skb->len);

    if (len > ETH_FRAME_LEN) {
        netdev->stats.tx_dropped++;
        if (printk_ratelimit())
            pr_info("zsonet tx: packet dropped - too large (%u bytes).", len);
        
        goto out_unlock;
    }

    if (! zsonet_check_tx_ready(zdev)){
        // NOTE we should do something about it ?
        pr_warn("zsonet tx: no transmission registers are available.");

        netif_stop_queue(netdev);

        status = NETDEV_TX_BUSY;
        goto out_unlock;
    }

    /* Sending of the frame */

    netif_trans_update(netdev);
    // 0-padding if needed
    if (skb->len < ETH_ZLEN) {
        memset(cur_txb_cpu, 0, ETH_ZLEN);
        len = ETH_ZLEN;
    }
    // Copy frame into buffer
    memcpy(cur_txb_cpu, skb->data, skb->len);

    /* Instruct hardware to send the packet out */
    zsonet_hw_tx(device, zdev->current_tx, len);
    
    /* Update the current_tx and remember skb ptr so it can be freed on TX_OK */
    zsonet_mark_tx_used(zdev, skb);

    /* Try not to respond with NETDEV_TX_BUSY */
    if (! zsonet_check_tx_ready(zdev)) {
        netif_stop_queue(netdev);
    }

    // update statistics
    netdev->stats.tx_packets++;
    netdev->stats.tx_bytes += len;

out_unlock:
    spin_unlock_irqrestore(&zdev->lock, flags);
    return status;
}


void zsonet_stats(struct net_device *netdev, struct rtnl_link_stats64 *storage)
{   
    struct zsonet_device *zdev = netdev_priv(netdev);
    zsonet_device_ptr device = zdev->zsodev_mm;

    /* Lock the device for more 'precise' stats.
       The device is slow compared to CPU, so we *should* not
       wait here for long.
    */
    spin_lock(&zdev->lock);

    storage->rx_packets = netdev->stats.rx_packets;
    storage->rx_bytes   = netdev->stats.rx_bytes;
    storage->rx_dropped = netdev->stats.rx_dropped;

    storage->tx_packets = netdev->stats.tx_packets;
    storage->tx_bytes   = netdev->stats.tx_bytes;
    storage->tx_dropped = netdev->stats.tx_dropped;

    storage->rx_missed_errors = ioread32(&device->RX.MFR);

    spin_unlock(&zdev->lock);
}

/* Called when a network interface is made active (IFF_UP) */
int zsonet_open(struct net_device *netdev)
{
    netif_carrier_on(netdev);
    netif_start_queue(netdev);

    return 0;
}

int zsonet_close(struct net_device *netdev)
{
    netif_stop_queue(netdev);
    return 0;
}

const struct net_device_ops zsonet_netdev_ops = {
    .ndo_open = zsonet_open,
    .ndo_stop = zsonet_close,
    .ndo_start_xmit = zsonet_tx,
    .ndo_get_stats64 = zsonet_stats
};