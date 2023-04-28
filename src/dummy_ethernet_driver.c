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

/* Protocol specific headers */
#include <linux/ip.h>
#include <linux/icmp.h>
#define DUMMY_DRV_NAME "dummy_eth"

/* Exported APIs from dummy HW modeule */
extern int asl_hw_tx(struct sk_buff *skb);
extern int32_t asl_request_irq(bool mode, int32_t (*dummy_rx)(struct sk_buff *));

struct dummy_eth_priv
{
        struct net_device *ndev;
};

int32_t dummy_eth_rx(struct sk_buff *skb)
{
        printk(KERN_INFO "Inside function %s \n", __func__);
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
        if (iph->protocol == IPPROTO_ICMP)
        {
                __wsum csum = 0;

                icmph = icmp_hdr(skb);
                if (icmph == NULL)
                {
                        printk(KERN_ERR "DETH: no such ICMP header\n");
                        goto free;
                }
                print_hex_dump(KERN_ERR, "DETH B: ", 0, 16, 1, skb->data, skb->len, 0);
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

                print_hex_dump(KERN_ERR, "DETH A: ", 0, 16, 1, skb->data, skb->len, 0);
                /* Pass frame up. XXX: need to enable hairpin, as same netdev? */
                printk(KERN_ERR "DETH: ping packet came pushing up\n");
                /* Dev is same here no need to assign - skb->dev = dev; */
                skb->protocol = eth_type_trans(skb, skb->dev);
                netif_rx(skb);
        }
        else
        {
                printk(KERN_ERR "DETH: not a ping packet\n");
                goto free;
        }

        return 0;

free:
        dev_kfree_skb(skb);
        return 0;
}

static int dummy_eth_tx(struct sk_buff *skb, struct net_device *ndev)
{
        printk(KERN_INFO "Inside function %s \n", __func__);
        // Handle transmitted packets (dummy implementation)
        if (asl_hw_tx(skb))
                dev_kfree_skb(skb);
        return NETDEV_TX_OK;
}

static int dummy_eth_open(struct net_device *ndev)
{
        printk(KERN_INFO "Inside function %s \n", __func__);
        // Device open handler (dummy implementation)
        netif_start_queue(ndev);
        return 0;
}

static int dummy_eth_stop(struct net_device *ndev)
{
        printk(KERN_INFO "Inside function %s \n", __func__);
        // Device stop handler (dummy implementation)
        netif_stop_queue(ndev);
        return 0;
}

static const struct net_device_ops dummy_eth_ops = {
    .ndo_open = dummy_eth_open,
    .ndo_stop = dummy_eth_stop,
    .ndo_start_xmit = dummy_eth_tx,
};

static void dummy_eth_init(struct net_device *ndev)
{
        printk(KERN_INFO "Inside function %s \n", __func__);
        // Initialize the net_device structure (dummy implementation)
        struct dummy_eth_priv *priv = netdev_priv(ndev);
        ether_setup(ndev);
        ndev->netdev_ops = &dummy_eth_ops;
        priv->ndev = ndev;

        ndev->flags = IFF_NOARP;
}

static void dummy_eth_exit(struct net_device *ndev)
{
        printk(KERN_INFO "Inside function %s \n", __func__);
        // Cleanup the net_device structure (dummy implementation)
        struct dummy_eth_priv *priv = netdev_priv(ndev);
        netif_stop_queue(ndev);
        free_netdev(ndev);
}

static struct net_device *dummy_eth_netdev;

static int __init dummy_eth_init_module(void)
{
        printk(KERN_INFO "Inside function %s \n", __func__);
        // Module initialization (dummy implementation)
        dummy_eth_netdev = alloc_netdev(sizeof(struct dummy_eth_priv), DUMMY_DRV_NAME,
                                        NET_NAME_UNKNOWN, dummy_eth_init);
        if (!dummy_eth_netdev)
        {
                return -ENOMEM;
        }

        if (register_netdev(dummy_eth_netdev))
        {
                free_netdev(dummy_eth_netdev);
                return -ENODEV;
        }

        if ((asl_request_irq(true, &dummy_eth_rx)))
        {
                return -1;
        }

        return 0;
}

static void __exit dummy_eth_cleanup_module(void)
{
        // Module cleanup (dummy implementation)
        printk(KERN_INFO "Inside function %s \n", __func__);
        unregister_netdev(dummy_eth_netdev);
        dummy_eth_exit(dummy_eth_netdev);
}

module_init(dummy_eth_init_module);
module_exit(dummy_eth_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Niladri Chatterjee");
MODULE_DESCRIPTION("Dummy Ethernet driver");
