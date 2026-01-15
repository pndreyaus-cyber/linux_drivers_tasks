#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

static void vnet_setup(struct net_device *dev);
static int vnet_napi_poll(struct napi_struct *napi, int budget);

/* Инициализация устройства */
static int vnet_open(struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);
    napi_enable(&priv->napi);
    //start tx queue
    netif_start_queue(dev);
    
    pr_info("VNET: Device opened\n");
    return 0;
}

static int vnet_release(struct net_device *dev)
{
    // stop tx queue
    netif_stop_queue(dev);
    struct vnet_priv *priv = netdev_priv(dev);
    napi_disable(&priv->napi);
    skb_queue_purge(&priv->rx_queue);
    pr_info("VNET: Device released\n");
    return 0;
}

static int vnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
  return 0;
}

static int vnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
  return 0;
}

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
  unregister_netdev(dev);
  free_netdev(dev);
  pr_info("VNET exit module... Goodbye!\n");
}

module_init(vnet_init);
module_exit(vnet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Petrov");
MODULE_DESCRIPTION("Virtual Ethernet Driver with packet processing + NAPI");


static void vnet_setup(struct net_device *dev) {
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

    netif_napi_add(dev, &priv->napi, vnet_napi_poll, 64);
}

// napi poll function
static int vnet_napi_poll(struct napi_struct *napi, int budget) {
    return 0;
}