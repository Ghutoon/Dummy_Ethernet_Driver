#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/kthread.h> //kernel threads
#include <linux/delay.h>

int32_t (*dummy_drv_rx)(struct sk_buff *);

struct task_struct *hw_thread_id;
struct sk_buff_head skb_tx_q;

static void dummy_hw_send_irq(void)
{
        struct sk_buff *skb = skb_dequeue(&skb_tx_q);

        if (skb == NULL)
        {
                printk(KERN_ERR "DHW: unable to dequeue TX skb\n");
                return;
        }

        printk(KERN_ERR "DHW: sending RX interrupt for skb=%p\n", skb);
        if (dummy_drv_rx == NULL)
        {
                printk(KERN_ERR "DHW: ASL interrupt not requested freeing\n");
                dev_kfree_skb(skb);
        }
        else
        {
                dummy_drv_rx(skb);
        }
}

static int dummy_hw_thread(void *arg)
{
        printk(KERN_INFO "DHW thread started: %p ..\n", current);

        while (1)
        {
                set_current_state(TASK_INTERRUPTIBLE);

                if (kthread_should_stop())
                        break;

                /* Going to sleep for 100ms */
                msleep(10);

                /* Do RX here */
                if (!skb_queue_empty(&skb_tx_q))
                        dummy_hw_send_irq();
        }
        printk(KERN_INFO "DHW thread closed\n");
        return 0;
}

int asl_hw_tx(struct sk_buff *skb)
{
        printk(KERN_ERR "DHW: adding skb=%p into HW Q\n", skb);
        /* Insert into tail and pick from head */
        skb_queue_tail(&skb_tx_q, skb);

        return 0;
}
EXPORT_SYMBOL_GPL(asl_hw_tx);

int32_t asl_request_irq(bool mode, int32_t (*rx_fn)(struct sk_buff *skb))
{
        if ((mode == true) && (dummy_drv_rx == NULL))
        {
                printk(KERN_ERR "DHW: ASL register IRQ for RX\n");
                dummy_drv_rx = rx_fn;
        }
        else if ((mode == false) && (dummy_drv_rx != NULL))
        {
                printk(KERN_ERR "DHW: ASL deregister IRQ for RX\n");
                /* To cleanup the TX Q */
                wake_up_process(hw_thread_id);
                /* XXX: Thread may call this - do not assign NULL dummy_drv_rx = NULL; */
        }

        return 0;
}
EXPORT_SYMBOL_GPL(asl_request_irq);

int dummy_hw_init(void)
{
        printk(KERN_ERR "DHW: HW init module\n");
        /* Init path create the thread */
        hw_thread_id = kthread_create(dummy_hw_thread,
                                      NULL, "dummy_hw");
        if (!IS_ERR(hw_thread_id))
        {
                printk(KERN_EMERG "HW thread created..\n");
                wake_up_process(hw_thread_id);
        }
        skb_queue_head_init(&skb_tx_q);

        return 0;
}

void dummy_hw_term(void)
{
        struct sk_buff *skb = NULL;

        printk(KERN_ERR "DHW: HW exit module\n");
        while ((skb = skb_dequeue(&skb_tx_q)))
                dev_kfree_skb(skb);

        /* Exit path should stop thread */
        if (hw_thread_id)
                kthread_stop(hw_thread_id);
}

module_init(dummy_hw_init);
module_exit(dummy_hw_term);
MODULE_LICENSE("GPL");