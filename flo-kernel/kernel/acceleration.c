/*
 * Implementation of system calls of hw3
 */

#include <linux/syscalls.h>
#include <linux/acceleration.h>

SYSCALL_DEFINE1(set_acceleration, struct dev_acceleration __user *, acceleration)
{
	long retval = 0;

	PRINTK("set_acceleration");

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
