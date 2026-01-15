#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

static void vnet_setup(struct net_device *dev);
static int vnet_napi_poll(struct napi_struct *napi, int budget);
static int vnet_open(struct net_device *dev);
static int vnet_release(struct net_device *dev);
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *dev);
static int vnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);


static const struct net_device_ops vnet_netdev_ops = {
  .ndo_open = vnet_open,
  .ndo_stop = vnet_release,
  .ndo_start_xmit = vnet_xmit,
  .ndo_do_ioctl = vnet_ioctl,
};
/* Приватные данные устройства */
struct vnet_priv {
    spinlock_t lock;
    struct sk_buff_head rx_queue;
    int lost_rx_packets;
    int tx_packets;
    int rx_packets;
    struct napi_struct napi;
};

struct net_device *dev = NULL;
/* Инициализация структуры устройства */
static int __init vnet_init(void) {
  pr_info("VNET init module...\n");
  dev = alloc_netdev(sizeof(struct vnet_priv),
                     "vnet%d",
                     NET_NAME_UNKNOWN,
                     vnet_setup);
  if (register_netdev(dev)) {
    pr_info("VNET: device init error\n");
    return 1;
  }
  
  return 0;
}

/* Выход модуля - Очистка всего */
static void __exit vnet_exit(void) {
  /* Удаление устройства и освобождение ресурсов в обратном порядке*/
  if (dev) unregister_netdev(dev);
  free_netdev(dev);
  pr_info("VNET exit module... Goodbye!\n");
}

module_init(vnet_init);
module_exit(vnet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Petrov");
MODULE_DESCRIPTION("Virtual Ethernet Driver with packet processing + NAPI");


static void vnet_setup(struct net_device *dev) {
    pr_info("VNET: vnet_setup");
    struct vnet_priv *priv = netdev_priv(dev);
    ether_setup(dev); // Настройка устройства как Ethernet
    dev->netdev_ops = &vnet_netdev_ops;

    /* Initialize private data */
    spin_lock_init(&priv->lock);
    skb_queue_head_init(&priv->rx_queue);
    priv->lost_rx_packets = 0;
    // set MTU = 1400
    dev->mtu = 1400;
    // generate random MAC address using helper function
    eth_hw_addr_random(dev);
    // pr_info this MAC address
    pr_info("VNET: MAC address: %pM\n", dev->dev_addr);
    // set device flags: Broadcast enabled, Multicast enabled, ARP enabled
    dev->flags |= IFF_BROADCAST | IFF_MULTICAST;
    netif_napi_add(dev, &priv->napi, vnet_napi_poll);
    // napi weight = 64
    priv->napi.weight = 64;
}

// napi poll function
static int vnet_napi_poll(struct napi_struct *napi, int budget) {
    pr_info("VNET: vnet_napi_poll");
    struct net_device *netdev = napi->dev;  /* Get from NAPI context */
    struct vnet_priv *priv = netdev_priv(netdev);
    struct sk_buff *skb;
    int work_done = 0;
    /* Process packets*/
    spin_lock(&priv->lock);
    while (work_done < budget && (skb = skb_dequeue(&priv->rx_queue))) {
        spin_unlock(&priv->lock);
        pr_info("VNET: Processing received packet of length %u\n", skb->len);
        /* Simulate packet reception */
        kfree_skb(skb);
        work_done++;
        spin_lock(&priv->lock);
    }
    spin_unlock(&priv->lock);

    if (skb_queue_empty(&priv->rx_queue)) {
        napi_complete(napi);
        pr_info("VNET: netif_wake_queue() called from vnet_napi_poll");
        netif_wake_queue(netdev);
    }

    return work_done;
}

/* Инициализация устройства */
static int vnet_open(struct net_device *dev)
{
    pr_info("VNET: vnet_open");
    struct vnet_priv *priv = netdev_priv(dev);
    napi_enable(&priv->napi);
    //start tx queue
    netif_start_queue(dev);
    
    pr_info("VNET: Device opened\n");
    return 0;
}

static int vnet_release(struct net_device *dev)
{
    pr_info("VNET: vnet_release");
    // stop tx queue
    pr_info("VNET: netif_stop_queue() called from vnet_release()");
    netif_stop_queue(dev);
    struct vnet_priv *priv = netdev_priv(dev);
    napi_disable(&priv->napi);
    skb_queue_purge(&priv->rx_queue);
    pr_info("VNET: Device released\n");
    return 0;
}

static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
    pr_info("VNET: vnet_xmit");
    struct vnet_priv *priv = netdev_priv(dev);

    //dev->stats.tx_packets++;
    //dev->stats.tx_bytes += skb->len;

    // Lock rx queue
    spin_lock(&priv->lock);
    
    if (skb_queue_len(&priv->rx_queue) >= 5) {
        pr_info("VNET: RX queue full, dropping packet\n");
        spin_unlock(&priv->lock);
        pr_info("VNET: netif_stop_queue() called from vnet_xmit()");
        netif_stop_queue(dev);
        priv->lost_rx_packets++;
        kfree_skb(skb);
        return NETDEV_TX_BUSY;
    }
    pr_info("VNET: Packet queued in RX queue\n");
    // enqueue skb to rx_queue
    
    skb_queue_tail(&priv->rx_queue, skb);
    if (skb_queue_len(&priv->rx_queue) >= 5) {
        pr_info("VNET: rx_queue full!");
    }
    spin_unlock(&priv->lock);
    napi_schedule(&priv->napi);
    
    return NETDEV_TX_OK;
}

static int vnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
  return 0;
}
