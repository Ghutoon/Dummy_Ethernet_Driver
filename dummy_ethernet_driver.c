#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <linux/net_tstamp.h>
#include <net/rtnetlink.h>
#include <linux/u64_stats_sync.h>
#include <linux/ip.h>
#include <linux/icmp.h>

#define DRV_NAME "dummy-eth"
extern int asl_hw_tx(struct sk_buff *skb);
extern int32_t asl_request_irq(bool mode, int32_t (*dummy_rx)(struct sk_buff *));

static int dummy_open(struct net_device *dev)
{
        printk(KERN_ERR "DETH: %s() - called\n", __func__);
        /*Open Handler Function
        Starts to accept buffer
        */
        netif_start_queue(dev);

        return 0;
}

static int dummy_close(struct net_device *dev)
{
        printk(KERN_ERR "DETH: %s() - called\n", __func__);
        netif_stop_queue(dev);

        return 0;
}

int32_t dummy_eth_rx(struct sk_buff *skb)
{
        struct iphdr *iph = NULL;
        struct icmphdr *icmph = NULL;
        int32_t addr = 0;
        char eth_addr[ETH_ALEN];

        if (skb == NULL)
        {
                printk(KERN_ERR "DETH: skb is null\n");
                return -EINVAL;
        }

        /* Mangle the packet to send ICMP/ping reply */
        iph = ip_hdr(skb);
        if (iph && iph->protocol == IPPROTO_ICMP)
        {
                __wsum csum = 0;

                icmph = icmp_hdr(skb);
                if (icmph == NULL)
                {
                        printk(KERN_ERR "DUMMY_ETHERNET_DRIVER_LOG: no such ICMP header\n");
                        goto free;
                }
                print_hex_dump(KERN_ERR, "DUMMY_ETHERNET_DRIVER_LOG B: ", 0, 16, 1, skb->data, skb->len, 0);
                /* Alter MAC addresses */
                memcpy(eth_addr, skb->data, ETH_ALEN);
                memmove(skb->data, skb->data + ETH_ALEN, ETH_ALEN);
                memcpy(skb->data + ETH_ALEN, eth_addr, ETH_ALEN);
                /* Alter IP addresses */
                addr = iph->daddr;
                iph->daddr = iph->saddr;
                iph->saddr = addr;
                /* ICMP echo reply */
                icmph->type = ICMP_ECHOREPLY;
                /* FIXME: Recalculate ICMP header checksum */
                icmph->checksum = 0;
                csum = csum_partial((u8 *)icmph, ntohs(iph->tot_len) - (iph->ihl * 4), csum);
                icmph->checksum = csum_fold(csum);

                print_hex_dump(KERN_ERR, "DUMMY_ETHERNET_DRIVER_LOG A: ", 0, 16, 1, skb->data, skb->len, 0);
                /* Pass frame up. XXX: need to enable hairpin, as same netdev? */
                printk(KERN_ERR "DUMMY_ETHERNET_DRIVER_LOG: ping packet came pushing up\n");
                /* Dev is same here no need to assign - skb->dev = dev; */
                skb->protocol = eth_type_trans(skb, skb->dev);
                netif_rx(skb);
        }
        else
        {
                printk(KERN_ERR "DUMMY_ETHERNET_DRIVER_LOG: not a ping packet\n");
                goto free;
        }

        return 0;

free:
        dev_kfree_skb(skb);
        return 0;
}

static netdev_tx_t dummy_xmit(struct sk_buff *skb, struct net_device *dev)
{
        /* Call HW xmit function */
        if (asl_hw_tx(skb))
                dev_kfree_skb(skb);

        return NETDEV_TX_OK;
}

static const struct net_device_ops dummy_netdev_ops = {
    .ndo_start_xmit = dummy_xmit,
    .ndo_validate_addr = eth_validate_addr,
    .ndo_set_mac_address = eth_mac_addr,
    .ndo_open = dummy_open,
    .ndo_stop = dummy_close,
};

static void dummy_setup(struct net_device *dev)
{
        printk(KERN_ERR "DUMMY_ETHERNET_DRIVER_LOG:  %s() - called\n", __func__);
        ether_setup(dev);

        /* Initialize the device structure. */
        dev->netdev_ops = &dummy_netdev_ops;
        dev->needs_free_netdev = true;
}
struct net_device *ndev;
static int __init dummy_ethernet_driver_module_init(void)
{
        struct net_device *ndev;
        printk(KERN_INFO "DUMMY_ETHERNET_DRIVER_LOG : Inside function %s\n", __func__);

        ndev = netdev_alloc(0, DRV_NAME, NET_NAME_UNKNOWN, dummy_setup);
        if (!ndev)
                return -ENOMEM;
        int err;

        if (err = register_netdevice(ndev) < 0)
                goto err;

        if ((err = asl_request_irq(true, &dummy_eth_rx)))
        {
                return err;
        }

err:
        free_netdev(ndev);
        return 0;
}

static int __init dummy_ethernet_driver_module_exit(void)
{
        printk(KERN_INFO "DUMMY_ETHERNET_DRIVER_LOG : Inside function %s\n", __func__);
        unregister_netdev(ndev);
        return 0;
}

module_init(dummy_ethernet_driver_module_init);
module_exit(dummy_ethernet_driver_module_exit);