#ifndef ZSONET_NETWORK_DEVICE_DRIVER_H
#define ZSONET_NETWORK_DEVICE_DRIVER_H

#include "zsonet.h"
#include "zsonet-hw.h"
#include "zsonet-driver-common.h"

#include <linux/netdevice.h>

void zsonet_rx(struct net_device *netdev);
netdev_tx_t zsonet_tx(struct sk_buff *skb, struct net_device *netdev);

#endif /* ZSONET_NETWORK_DEVICE_DRIVER_H */