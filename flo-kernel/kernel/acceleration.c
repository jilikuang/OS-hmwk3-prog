/*
 * Implementation of system calls of hw3
 */
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/acceleration.h>

static LIST_HEAD(event_list);
static LIST_HEAD(user_list);
static DEFINE_MUTEX(event_list_mu);
static DEFINE_MUTEX(user_list_mu);

/* @lfred: just to prevent multi-daemon or TA's test */
static DEFINE_SEMAPHORE(set_semaphore);
static DEFINE_SEMAPHORE(signal_semaphore);

/* Event ID */
#define EVENT_ID_MIN	(10)
#define EVENT_ID_MAX	(0x0FFFFFF0)
static atomic_t last_event_id = ATOMIC_INIT(EVENT_ID_MIN);

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

	retDown = down_interruptible(&set_semaphore);
	if (retDown != 0) {
		PRINTK("Sorry dude, you received a signal");
		return retDown;
	}

	retCopy = copy_from_user(&s_kData, acceleration, sz);
	up(&set_semaphore);
  
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
	unsigned long sz = sizeof(struct acc_motion);
	struct acc_motion s_kData;
	struct acc_event_info *new_event;

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

	new_event = (struct acc_event_info *)kmalloc(
			sizeof(struct acc_event_info), GFP_ATOMIC);
	if (new_event == NULL)
		return -ENOMEM;

	mutex_lock(&event_list_mu);

	memcpy(&new_event->m_motion, &s_kData, sizeof(struct acc_motion));
	new_event->m_eid = atomic_add_return(1, &last_event_id);
	if (atomic_cmpxchg(&last_event_id, EVENT_ID_MAX, EVENT_ID_MIN)
			== EVENT_ID_MAX)
		PRINTK("The event id is reset\n");
	list_add_tail(&(new_event->m_event_list), &event_list);

	mutex_unlock(&event_list_mu);

	return (long)new_event->m_eid;
}

int check_event_exist(int event_id){
	struct acc_event_info *iter;
	mutex_lock(&event_list_mu);
	list_for_each_entry(iter, &event_list, m_event_list){
		if(iter->m_eid == event_id){
			mutex_unlock(&event_list_mu);
			return 1;
		}
	}
	mutex_unlock(&event_list_mu);
	return 0;
}

struct acc_event_info *get_event(int event_id){
	struct acc_event_info *iter;
	mutex_lock(&event_list_mu);
	list_for_each_entry(iter, &event_list, m_event_list){
		if(iter->m_eid == event_id){
			mutex_unlock(&event_list_mu);
			return iter;
		}
	}
	mutex_unlock(&event_list_mu);
	return NULL;
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
	if (check_event_exist(event_id)){
		struct timeval tv;
		struct acc_user_info *new_user;
		new_user = kmalloc(sizeof(struct acc_user_info),GFP_ATOMIC);
		new_user->m_req_proc = current->pid;
		mutex_init(&new_user->thread_mu);

		/*locking the current process*/
		mutex_lock(&new_user->thread_mu);

		do_gettimeofday(&tv);
		new_user->m_timestamp = tv.tv_usec + (tv.tv_sec)*1000000; 
		new_user->m_activated = 0;

		/* protection while assign this event */
		mutex_lock(&event_list_mu);
		new_user->mp_event = get_event(event_id);
		if(new_user->mp_event==NULL){
			PRINTK("Illigal Event\n");
		}
		mutex_unlock(&event_list_mu);

		/* protection while adding this user in user list */
		mutex_lock(&user_list_mu);
		list_add(&new_user->m_user_list,&user_list);
		mutex_unlock(&user_list_mu);

		/*current process goto sleep*/
		mutex_lock(&new_user->thread_mu);

	}else{
		PRINTK("Illigal Event ID\n");
		return -EFAULT;
	}


	return retval;
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

	retDown = down_interruptible(&signal_semaphore);
	if (retDown != 0) {
		PRINTK("Hey dud, you're interupted.");
		return retDown;
	}
	
	/* TODO: do the real thing here */

	up(&signal_semaphore);

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

