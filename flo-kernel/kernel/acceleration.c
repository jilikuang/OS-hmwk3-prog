/*
 * Implementation of system calls of hw3
 */
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/acceleration.h>

SYSCALL_DEFINE1(set_acceleration, struct dev_acceleration __user *, acceleration)
{
	long retval = 0;
	unsigned long sz = sizeof(struct dev_acceleration);
	static struct dev_acceleration s_kData;

	PRINTK("set_acceleration: old value: %d, %d, %d", 
		s_kData.x, s_kData.y, s_kData.z);

	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param");
		return -EINVAL;
	}

	if (copy_from_user(&s_kData, acceleration, sz) != 0) {
		PRINTK("set_acceleration memory error");
		return -EFAULT;
	}

	PRINTK("set_acceleration: new value: %d, %d, %d", 
		s_kData.x, s_kData.y, s_kData.z);

	return retval;
}

SYSCALL_DEFINE1(accevt_create, struct acc_motion __user *, acceleration)
{
	long retval = 0;

	PRINTK("accevt_create");

	return retval;
}

SYSCALL_DEFINE1(accevt_wait, int, event_id)
{
	long retval = 0;

	PRINTK("accevt_wait");

	return retval;
}

SYSCALL_DEFINE1(accevt_signal, struct dev_acceleration __user *, acceleration)
{
	long retval = 0;

	PRINTK("accevt_signal");

	return retval;
}

SYSCALL_DEFINE1(accevt_destroy, int, event_id)
{
	long retval = 0;

	PRINTK("accevt_destroy");

	return retval;
}
