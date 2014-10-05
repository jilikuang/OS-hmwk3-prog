/*
 * Implementation of system calls of hw3
 */
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/acceleration.h>

static list_head event_list = LIST_HEAD(event_list);
static list_head user_list = LIST_HEAD(user_list);
static DECLARE_MUTEX(event_list_sem);
static DECLARE_MUTEX(user_list_sem);

/* @lfred: just to prevent multi-daemon or TA's test */
static DECLARE_MUTEX(set_mutex);
static DECLARE_MUTEX(signal_mutex);

SYSCALL_DEFINE1(set_acceleration,
		struct dev_acceleration __user *, acceleration)
{
	/* @lfred: static data to hold everything */
	static struct dev_acceleration s_kData;
	
	/* local frame */
	long retval = 0;
	unsigned long sz = sizeof(struct dev_acceleration);
	unsigned long retCopy = 0;
	int retDown = 0;

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

	retDown = down_interruptible(&set_mutex);
	if (retDown != 0) {
		PRINTK("Sorry dude, you received a signal");
		return retDown;
	}

	retCopy = copy_from_user(&s_kData, acceleration, sz);
	up(&set_mutex);
  
	if (retCopy != 0) {
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


/* Block a process on an event.
 * It takes the event_id as parameter. The event_id requires verification.
 * Return 0 on success and the appropriate error on failure.
 * system call number 380
 */

SYSCALL_DEFINE1(accevt_wait, int, event_id)
{

	long retval = 0;

	PRINTK("accevt_wait\n");


	/* Check event_id validation */
	if (check_event_exsist(event_list, event_id)){
		struct acc_user_info *new_user;
		new_user = kmalloc(sizeof(acc_user_info),GFP_ATOMIC);
		new_user->m_req_proc = current->tid;
		sema_init(new_user->thread_sem, 2);
		struct timeval *tv;
		do_gettimeofday(tv);
		new_user->m_timestamp = tv->usec + (tv->sec)*1000000; 
		new_user->m_activated = 0;

		/* get semaphore for this event */
		if (down_interruptible(&event_list_sem)) {
			new_user->mp_event = get_event(event_id);
			if(new_user->mp_event==NULL){
				PRINTK("Illigal Event\n");
			}
		}
		up(&event_list_sem);

		if(down_interruptible(&user_list_sem)){
		list_add(user_list,m_user_list);
		}
		up(&user_list_sem);

	}else{
		PRINTK("Illigal Event ID\n");
		return -EFAULT;
	}
	/* Two semaphore for current process to goto sleep
	*  Notice I init current user semaphore if event is valid,
	*  and to delibertly waste one 
	*/
	if (down_interruptible(&new_user->mp_event)) {
	}

	if (down_interruptible(&new_user->mp_event)) {
	}

	return retval;
}

int check_event_exsist(event_list, event_id){
	struct acc_user_info *iter;
	list_for_each_entry(iter, user_list_sem, event_id){
		if(iter->event_id == event_id){
			return 1;
		}
	}
	return 0;
}

int get_event(event_list, event_id){
	struct acc_user_info *iter;
	list_for_each_entry(iter, user_list_sem, event_id){
		if(iter->event_id == event_id){
			return iter;
		}
	}
	return null;
}


/* The acc_signal system call
 * takes sensor data from user, stores the data in the kernel,
 * generates a motion calculation, and notify all open events whose
 * baseline is surpassed.  All processes waiting on a given event 
 * are unblocked.
 * Return 0 success and the appropriate error on failure.
 * system call number 381
 */

SYSCALL_DEFINE1(accevt_signal, struct dev_acceleration __user *, acceleration)
{
	/* @lfred: it's just not impl */
	struct dev_acceleration data;
	
	long retval = 0;
	int retDown = 0;
	unsigned long sz = sizeof(struct dev_acceleration);
	
	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param\n");
		return -EINVAL;
	}

	if (!access_ok(VERIFY_READ, acceleration, sz)) {
		PRINTK("Illigal user-space address\n");
		return -EFAULT;
	}

	if (copy_from_user(&data, acceleration, sz) != 0) {
		PRINTK("dude - failed to copy. 88\n");
		return -EFAULT;
	}

	retDown = down_interruptible(&signal_mutex);
	if (retDown != 0) {
		PRINTK("Hey dud, you're interupted.");
		return retDown;
	}
	
	/* TODO: do the real thing here */

	up(&signal_mutex);

	if (retval != 0) {
		PRINTK("set_acceleration memory error\n");
		return -EFAULT;
	}

	PRINTK("accevt_signal: %ld\n", retval);
	return retval;
}

SYSCALL_DEFINE1(accevt_destroy, int, event_id)
{
	long retval = 0;

	PRINTK("accevt_destroy\n");

	return retval;
}
