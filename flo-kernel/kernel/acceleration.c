/*
 * Implementation of system calls of hw3
 */
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/acceleration.h>

SYSCALL_DEFINE1(set_acceleration,
		struct dev_acceleration __user *, acceleration)
{
	long retval = 0;
	unsigned long sz = sizeof(struct dev_acceleration);
	static struct dev_acceleration s_kData;

	PRINTK("set_acceleration: old value: %d, %d, %d\n",
		s_kData.x, s_kData.y, s_kData.z);

	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param\n");
		return -EINVAL;
	}

	if (!access_ok(VERIFY_READ, acceleration, sz)) {
		PRINTK("Illigal user-space address\n");
		return -EFAULT;
	}

	if (copy_from_user(&s_kData, acceleration, sz) != 0) {
		PRINTK("set_acceleration memory error\n");
		return -EFAULT;
	}

	PRINTK("set_acceleration: new value: %d, %d, %d\n",
		s_kData.x, s_kData.y, s_kData.z);

	return retval;
}

SYSCALL_DEFINE1(accevt_create, struct acc_motion __user *, acceleration)
{
	long retval = 0;
	unsigned long sz = sizeof(struct acc_motion);
	static struct acc_motion s_kData;

	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param\n");
		return -EINVAL;
	}

	if (!access_ok(VERIFY_READ, acceleration, sz)) {
		PRINTK("Illigal user-space address\n");
		return -EFAULT;
	}

	if (copy_from_user(&s_kData, acceleration, sz) != 0) {
		PRINTK("set_acceleration memory error\n");
		return -EFAULT;
	}

	PRINTK("accevt_create\n");

	return retval;
}

SYSCALL_DEFINE1(accevt_wait, int, event_id)
{
	long retval = 0;

	PRINTK("accevt_wait\n");

	return retval;
}

SYSCALL_DEFINE1(accevt_signal, struct dev_acceleration __user *, acceleration)
{
	long retval = 0;
	unsigned long sz = sizeof(struct dev_acceleration);
	static struct dev_acceleration s_kData;

	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param\n");
		return -EINVAL;
	}

	if (!access_ok(VERIFY_READ, acceleration, sz)) {
		PRINTK("Illigal user-space address\n");
		return -EFAULT;
	}

	if (copy_from_user(&s_kData, acceleration, sz) != 0) {
		PRINTK("set_acceleration memory error\n");
		return -EFAULT;
	}

	PRINTK("accevt_signal\n");

	return retval;
}

SYSCALL_DEFINE1(accevt_destroy, int, event_id)
{
	long retval = 0;

	PRINTK("accevt_destroy\n");

	return retval;
}
