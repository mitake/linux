/* toy. nis: non idle sleeper */

#include <linux/init.h>
#include <linux/threads.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/irqflags.h>
#include <linux/time.h>
#include <linux/proc_fs.h>

static DEFINE_PER_CPU(struct task_struct *, nis);
static DEFINE_PER_CPU(int, sleep_jiffies);

struct nis_priv_data {
	int cpu;
};

static DEFINE_PER_CPU(struct nis_priv_data, nis_priv_data);

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

static int nis_open(struct inode *inode, struct file *file)
{
	struct proc_dir_entry *pde;

	pde = PDE(inode);
	file->private_data = pde->data;

	return 0;
}

static ssize_t nis_read(struct file *file, char __user *buf, size_t size,
			loff_t *ppos)
{
	int ret;
	struct nis_priv_data *data = file->private_data;
	char ratio_s[4];

	if (size < 4) {
		printk(KERN_INFO "too short buf\n");
		return -ENOMEM;
	}

	memset(ratio_s, 0, 4);
	ret = sprintf(buf, "%d",
		(int)(100 * (float)(per_cpu(sleep_jiffies, data->cpu) / HZ)));

	return ret;
}

static ssize_t nis_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int _ratio, new_sj;
	struct nis_priv_data *data = file->private_data;

	sscanf(buf, "%d", &_ratio);
	if (!(0 <= _ratio || _ratio <= 100)) {
		printk(KERN_INFO "invalid value: %d\n", _ratio);
		return -EINVAL;
	}

	new_sj = HZ * (float)(_ratio / 100.0);
	per_cpu(sleep_jiffies, data->cpu) = new_sj;

	printk(KERN_INFO "new sleep jiffies set: %d (CPU%d)\n",
		per_cpu(sleep_jiffies, data->cpu), data->cpu);

	return count;
}

static const struct file_operations nis_fops = {
	.open		= nis_open,
	.write		= nis_write,
	.read		= nis_read,
};

static int init_nis(void)
{
	int i;
	struct task_struct *per_cpu_nis;

	struct proc_dir_entry *nis_dir;
	char nis_file_name[5];
	struct nis_priv_data *priv;

	for_each_possible_cpu(i) {
		per_cpu_nis = kthread_create(nis_fn, NULL, "nis/%d", i);
		kthread_bind(per_cpu_nis, i);
		wake_up_process(per_cpu_nis);

		per_cpu(nis, i) = per_cpu_nis;
		per_cpu(sleep_jiffies, i) = 10;

		printk(KERN_INFO "created non idle sleeper for CPU%d\n", i);
	}

	nis_dir = proc_mkdir("nis", NULL);
	memset(nis_file_name, 0, 5);
	memcpy(nis_file_name, "nis", 3);

	for_each_possible_cpu(i) {
		priv = &__get_cpu_var(nis_priv_data);
		priv->cpu = i;
		nis_file_name[3] = '0' + i;

		proc_create_data(nis_file_name, S_IRUSR | S_IWUSR, nis_dir,
				&nis_fops, priv);
	}

	return 0;
}

__initcall(init_nis);
