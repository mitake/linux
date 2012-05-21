/* toy. nis: non idle sleeper */

#include <linux/init.h>
#include <linux/threads.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/irqflags.h>
#include <linux/time.h>

static DEFINE_PER_CPU(struct task_struct *, nis);
static DEFINE_PER_CPU(int, sleep_jiffies);

static int nis_fn(void *data)
{
	int sj, cpu = raw_smp_processor_id();

	printk(KERN_INFO "%s starts on CPU%d\n", current->comm, cpu);

	while (1) {
		sj = __get_cpu_var(sleep_jiffies);

		preempt_disable();
		while (sj--)
			safe_halt();

		preempt_enable();

		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ - __get_cpu_var(sleep_jiffies));
	}

	return 0;
}

static int init_nis(void)
{
	int i;
	struct task_struct *per_cpu_nis;

	for_each_possible_cpu(i) {
		per_cpu_nis = kthread_create(nis_fn, NULL, "nis/%d", i);
		kthread_bind(per_cpu_nis, i);
		wake_up_process(per_cpu_nis);

		per_cpu(nis, i) = per_cpu_nis;
		per_cpu(sleep_jiffies, i) = 10;

		printk(KERN_INFO "created non idle sleeper for CPU%d\n", i);
	}

	return 0;
}

__initcall(init_nis);
