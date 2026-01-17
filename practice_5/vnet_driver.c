#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

static void vnet_setup(struct net_device *dev);
static int vnet_napi_poll(struct napi_struct *napi, int budget);
static int vnet_open(struct net_device *dev);
static int vnet_release(struct net_device *dev);
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *dev);

/* Network device operations */
static const struct net_device_ops vnet_netdev_ops = {
  .ndo_open = vnet_open,
  .ndo_stop = vnet_release,
  .ndo_start_xmit = vnet_xmit,
};

/* Private device data */
struct vnet_priv {
    spinlock_t lock;
    struct sk_buff_head rx_queue;
    struct napi_struct napi;
};

/* Global network device pointer */
struct net_device *dev = NULL;

/* Module initialization */
static int __init vnet_init(void) {
    pr_info("VNET init module...\n");
    dev = alloc_netdev(sizeof(struct vnet_priv), "vnet%d", NET_NAME_UNKNOWN, vnet_setup);
    if(!dev) {
        pr_info("VNET: device allocation error\n");
        return -ENOMEM;
    }
    // Is this "if" correct? Yes, it checks if dev is NULL.
    if (register_netdev(dev)) {
        pr_info("VNET: device init error\n");
        free_netdev(dev);
        return -EINVAL;
    }
    return 0;
}

/* Module cleanup */
static void __exit vnet_exit(void) {
    if (dev) unregister_netdev(dev);
    free_netdev(dev);
    pr_info("VNET exit module... Goodbye!\n");
}

module_init(vnet_init);
module_exit(vnet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Petrov");
MODULE_DESCRIPTION("Virtual Ethernet Driver with packet processing + NAPI");

/* Device setup function */
static void vnet_setup(struct net_device *dev) {
    struct vnet_priv *priv = netdev_priv(dev); // Get private data

    pr_info("VNET: vnet_setup");

    ether_setup(dev); // Set up Ethernet device
    dev->netdev_ops = &vnet_netdev_ops; // Assign netdev operations

    /* Initialize private data */
    spin_lock_init(&priv->lock);
    skb_queue_head_init(&priv->rx_queue);
    dev->mtu = 1400;
    eth_hw_addr_random(dev); // generate random MAC address using helper function
    pr_info("VNET: MAC address: %pM\n", dev->dev_addr);
    dev->flags |= IFF_BROADCAST | IFF_MULTICAST; // set device flags: Broadcast enabled, Multicast enabled, ARP enabled
    netif_napi_add(dev, &priv->napi, vnet_napi_poll); // NAPI initialization
    priv->napi.weight = 64; // set NAPI weight (maximum packets to process in one poll)
}

/* NAPI poll function */
static int vnet_napi_poll(struct napi_struct *napi, int budget) {
    struct net_device *netdev = napi->dev;  // Get net_device from napi context
    struct vnet_priv *priv = netdev_priv(netdev);
    struct sk_buff *skb; // Temporary skb pointer
    int packets_processed = 0;

    pr_info("VNET: vnet_napi_poll");

    // Process packets from RX queue
    spin_lock(&priv->lock); // Why here we need to lock? Because we will access rx_queue which is shared resource.
    while (packets_processed < budget && (skb = __skb_dequeue(&priv->rx_queue))) { // We process packets until we reach the budget or the RX queue is empty.
        spin_unlock(&priv->lock); // Why we need to unlock here? Because we will process the packet and we do not need to hold the lock during processing.
        pr_info("VNET: Processing received packet %d of length %u\n", packets_processed, skb->len);

        // Prepare skb for reception
        skb->dev = netdev; // Set device pointer
        skb->protocol = eth_type_trans(skb, netdev); // Set protocol field based on Ethernet header
        skb->ip_summed = CHECKSUM_UNNECESSARY; // No checksum offloading
        pr_info("VNET: Packet protocol after eth_type_trans: 0x%04x\n", ntohs(skb->protocol));
        
        // Update device statistics
        netdev->stats.rx_packets++;
        netdev->stats.rx_bytes += skb->len;
        
        netif_receive_skb(skb); // Pass skb to the network stack

        //kfree_skb(skb); Removed, because netif_receive_skb takes ownership of skb and will free it later.
        packets_processed++;
        spin_lock(&priv->lock);
    }
    // We unlock, because we are done accessing shared resource.
    spin_unlock(&priv->lock);
    // If rx_queue is not full, wake up the tx queue (we are ready to accept more packets)
    if (skb_queue_len(&priv->rx_queue) < 5) {
        pr_info("VNET: netif_wake_queue() called from vnet_napi_poll");
        netif_wake_queue(netdev);
       if (skb_queue_empty(&priv->rx_queue)) {
           pr_info("VNET: RX queue empty after processing\n");
       }
    }
    if (packets_processed < budget) {
        pr_info("VNET: NAPI poll complete, processed %d packets\n", packets_processed);
        napi_complete_done(napi, packets_processed); 
        /*Why do we need this line? Because we need to inform NAPI that we are done processing packets
          And what NAPI will do then? It will stop polling until new packets arrive. 
          So, if we do not call this function, NAPI will continue to poll even when there are no packets to process, wasting CPU resources
        */
    }
    // Return number of processed packets. Why? Because NAPI needs to know how many packets we processed in this poll. Why? Because it uses this information to manage its internal state and scheduling
    return packets_processed;
}

/* Инициализация устройства */
static int vnet_open(struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);

    pr_info("VNET: vnet_open");
    napi_enable(&priv->napi);
    //start tx queue
    netif_start_queue(dev);
    
    pr_info("VNET: Device opened\n");
    return 0;
}

static int vnet_release(struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);

    pr_info("VNET: vnet_release");
    // stop tx queue
    pr_info("VNET: netif_stop_queue() called from vnet_release()");
    netif_stop_queue(dev);
    napi_disable(&priv->napi);
    skb_queue_purge(&priv->rx_queue);
    pr_info("VNET: Device released\n");
    return 0;
}

/*This function is used to transmit a packet*/
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);

    pr_info("VNET: vnet_xmit");

    // Update TX statistics
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += skb->len;

    // Lock rx queue. Why? Because we will access shared resource rx_queue. But rx_queue contains received packets, why do we need to lock it when we are transmitting? Because in this virtual driver, we simulate packet transmission by enqueueing the skb into the RX queue for processing by NAPI.
    spin_lock(&priv->lock);
    if (skb_queue_len(&priv->rx_queue) >= 10) { // RX queue full
        pr_info("VNET: RX queue full, dropping packet\n");
        // Do i need to unlock here? Yes Why? Because we locked before. Do not i need to lock after? No, because we are returning.
        spin_unlock(&priv->lock);
        pr_info("VNET: netif_stop_queue() called from vnet_xmit()");
        netif_stop_queue(dev); 
        /*Stop tx or rx? -- tx queue
          Why? Because we cannot accept more packets for transmission until some are processed
          Do not we need to wake it up later? Yes, we wake it up in NAPI poll when we have space in RX queue
        */
        dev -> stats.tx_dropped++;
        
        kfree_skb(skb);
        return NETDEV_TX_BUSY; // Indicate that the transmission was not successful due to full RX queue
    } else{
        pr_info("VNET: Packet queued in RX queue\n");
        // enqueue skb to rx_queue
        // Do we need to unlock here? No, because we will unlock after enqueueing.
        __skb_queue_tail(&priv->rx_queue, skb);
        if (skb_queue_len(&priv->rx_queue) >= 10) {
            pr_info("VNET: rx_queue full!");
        }
    
        spin_unlock(&priv->lock);
        napi_schedule(&priv->napi);
        // We do not free skb here, because it is now owned by rx_queue and will be freed after processing in NAPI poll.
        return NETDEV_TX_OK;
    }
}
