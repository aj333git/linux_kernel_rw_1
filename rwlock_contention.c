#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rwlock.h>
#include <linux/ktime.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("example");
MODULE_DESCRIPTION("rwlock contention experiment");

#define NUM_READERS 4

static DEFINE_RWLOCK(test_lock);

static struct task_struct *readers[NUM_READERS];
static struct task_struct *writer;

static unsigned long shared_value;


static int reader_fn(void *arg)
{
	long id = (long)arg;

	while (!kthread_should_stop()) {

		read_lock(&test_lock);

		/*
		 * Simulate work while holding the
		 * read lock.
		 */
		pr_info("reader-%ld: value=%lu\n",
			id, shared_value);

		/*
		 * Small delay so contention is easier
		 * to observe.
		 */
		udelay(100);

		read_unlock(&test_lock);

		msleep(10);
	}

	return 0;
}


static int writer_fn(void *arg)
{
	unsigned long count = 0;

	while (!kthread_should_stop()) {
		ktime_t start;
		ktime_t end;
		s64 wait_ns;

		start = ktime_get();

		write_lock(&test_lock);

		end = ktime_get();

		wait_ns = ktime_to_ns(ktime_sub(end, start));

		shared_value++;

		pr_info("writer: acquired lock, wait=%lld ns value=%lu\n",
			wait_ns,
			shared_value);

		write_unlock(&test_lock);

		count++;

		msleep(100);
	}

	return 0;
}


static int __init rwlock_contention_init(void)
{
	int i;

	pr_info("rwlock_contention: loading\n");

	shared_value = 0;

	for (i = 0; i < NUM_READERS; i++) {
		readers[i] =
			kthread_run(reader_fn,
				    (void *)(long)i,
				    "rw-reader-%d",
				    i);

		if (IS_ERR(readers[i])) {
			pr_err("failed to create reader %d\n", i);
			readers[i] = NULL;
		}
	}

	writer = kthread_run(writer_fn,
			     NULL,
			     "rw-writer");

	if (IS_ERR(writer)) {
		pr_err("failed to create writer\n");
		writer = NULL;
	}

	return 0;
}


static void __exit rwlock_contention_exit(void)
{
	int i;

	pr_info("rwlock_contention: unloading\n");

	if (writer)
		kthread_stop(writer);

	for (i = 0; i < NUM_READERS; i++) {
		if (readers[i])
			kthread_stop(readers[i]);
	}

	pr_info("rwlock_contention: unloaded\n");
}


module_init(rwlock_contention_init);
module_exit(rwlock_contention_exit);

