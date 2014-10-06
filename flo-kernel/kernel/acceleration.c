/*
 * Implementation of system calls of hw3
 */
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kfifo.h>

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/acceleration.h>

/* LIST HEADs */
/*****************************************************************************/
static LIST_HEAD(event_list);
static LIST_HEAD(user_list);

/* MUTEX for data structure */
/*****************************************************************************/
static DEFINE_MUTEX(data_mtx);

/* @lfred: just to prevent multi-daemon or TA's test */
static DEFINE_MUTEX(set_mutex);

/* other data */
/*****************************************************************************/
static unsigned int g_lastId = 0;
//static BOOL g_init = M_FALSE;
//DECLARE_KFIFO(g_dataq, WINDOW); 

/*****************************************************************************/
/* Util function to get current time */
/* This is BUGGY - we should use a monotonic time instead */
static unsigned int get_current_time(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_usec + (tv.tv_sec)*1000000;
}

#if 0
/* !!! THREAD-SAFE FUNCTION !!! */
static void init_fifo(void)
{
	if (g_init == M_TRUE)
		return;

	if (mutex_lock_interruptible(&data_mtx) != 0)
		return;

	if (g_init == M_FALSE) {
		g_init = M_TRUE;
		INIT_KFIFO(&g_dataq);
	}

	mutex_unlock(&data_mtx);
} 
#endif

/* !!! NOT THREAD-SAFE !!! */
/* the function is used to allocate a new ID */
/* NOTE: use with lock hold */
static BOOL alloc_event_id(unsigned int *ap_id)
{
	unsigned int startingId = g_lastId;
	struct acc_event_info *iter;	
	BOOL found = M_FALSE; 

	while (true) {
		
		list_for_each_entry(iter, &event_list, m_event_list) {
			if (iter->m_eid == g_lastId) {
				found = M_TRUE;
				break;
			}
		}

		if (found == M_TRUE) {
			/* the lastId is used */
			g_lastId++;
			found = M_FALSE;

			if (g_lastId == startingId)
				return M_FALSE;
		} else {
			/* the lastId is not used */
			break;
		}
	}

	*ap_id = g_lastId;
	g_lastId++;
	return M_TRUE;
}

/* !!! NOT THREAD-SAFE !!! */
/* You have to acquire data_mtx to call this func */
struct acc_event_info *check_event_exist(int event_id){
	struct acc_event_info *iter;
	
	if (list_empty(&event_list))
		return NULL;

	list_for_each_entry(iter, &event_list, m_event_list) {
		if (iter->m_eid == event_id)
			return iter;
	}

	return NULL;
}

/*****************************************************************************/
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

	retDown = mutex_lock_interruptible(&set_mutex);
	if (retDown != 0) {
		PRINTK("Sorry dude, you received a signal");
		return retDown;
	}

	retCopy = copy_from_user(&s_kData, acceleration, sz);
	mutex_unlock(&set_mutex);
  
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
	unsigned long szMotion = sizeof(struct acc_event_info);
	long ret = 0;

	struct acc_motion s_kData;
	struct acc_event_info *new_event;

	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param\n");
		return -EINVAL;
	}

	if (copy_from_user(&s_kData, acceleration, sz) != 0) {
		PRINTK("set_acceleration memory error\n");
		return -EFAULT;
	}

	PRINTK("accevt_create\n");

	/* create the user info memory */
	new_event = kmalloc(szMotion, GFP_ATOMIC);

	if (new_event == NULL)
		return -ENOMEM;

	/* init without lock */
	memcpy(&new_event->m_motion, &s_kData, sizeof(struct acc_motion));	
	INIT_LIST_HEAD(&new_event->m_event_list);
	INIT_LIST_HEAD(&new_event->m_wait_list);

	/* CRITICAL section: init event with lock */	
	ret = mutex_lock_interruptible(&data_mtx);
	
	if (ret != 0) {
		PRINTK("Failed to obtain event list lock - bye");
		return ret;
	}

	/* init with lock */
	if (alloc_event_id(&(new_event->m_eid)) == M_TRUE) {
		list_add_tail(&(new_event->m_event_list), &event_list);
		ret = new_event->m_eid;
	} else {
		ret = -ENOMEM;
	}

	mutex_unlock(&data_mtx);

	return ret;
}


/*
 * Block a process on an event.
 * It takes the event_id as parameter. The event_id requires verification.
 * Return 0 on success and the appropriate error on failure.
 * system call number 380
 */
SYSCALL_DEFINE1(accevt_wait, int, event_id)
{
	int semRet;
	long retval = 0;
	unsigned int sz_user = sizeof(struct acc_user_info);
	struct acc_user_info *new_user = NULL;
	struct acc_event_info *evt = NULL;

	PRINTK("accevt_wait\n");

	/* @lfred: jobs needed for accevt_wait 		*/
	/* 1. check if the event is correct.		*/
	/* 2. init the user object			*/
	/* 3. link the user object into right place	*/
	/* 4. start waiting 				*/
	/* 5. when waking up, do the cleaning 		*/

	new_user = kmalloc(sizeof(sz_user), GFP_ATOMIC);
	if (new_user == NULL)
		return -ENOMEM;

	/* init the user struct */
	new_user->m_req_proc = current->pid;
	new_user->m_timestamp = get_current_time();
	new_user->m_activated = M_TRUE;
	sema_init(&new_user->m_thrd_sema, 1);
	semRet = down_interruptible(&new_user->m_thrd_sema);
	
	if (semRet != 0) {
		kfree(new_user);
		return semRet;
	}

	/* @lfred: add to the data structure */	
	semRet = mutex_lock_interruptible(&data_mtx);

	if (semRet != 0) {
		up(&new_user->m_thrd_sema);
		kfree(new_user);
		return semRet;
	}

	/* Check event_id validation */
	evt = check_event_exist(event_id);

	if (evt == NULL) {
		mutex_unlock(&data_mtx);
		up(&new_user->m_thrd_sema);
		kfree(new_user);

		PRINTK("Illigal Event ID\n");
		return -EFAULT;
	}
	
	/* add to the data structure */
	list_add_tail(&new_user->m_user_list, &evt->m_wait_list);	
	mutex_unlock(&data_mtx);
		
	/* start waiting - signal will do the clean-up for normal case. */
	semRet = down_interruptible(&new_user->m_thrd_sema);

	/* clean up myself if interrupted .*/
	if (semRet != 0) {
		/* return the semaphore */
		up(&new_user->m_thrd_sema);
		
		/* must NOT use interruptible here - clean up */ 
		mutex_lock(&data_mtx);
		list_del(&new_user->m_user_list);
		mutex_unlock(&data_mtx);

		retval = semRet;
	}

	/* 2nd phase of cleaning: wait has to free itself */
	kfree(new_user);
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
	struct acc_dev_info d_info;
	
	long retval = 0;
	int retDown = 0;
	unsigned long sz = sizeof(struct dev_acceleration);
	
	/* at any case, we need to call this */
	//init_fifo();

	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param\n");
		return -EINVAL;
	}
	
	if (copy_from_user(&data, acceleration, sz) != 0) {
		PRINTK("dude - failed to copy. 88\n");
		return -EFAULT;
	}

	retDown = mutex_lock_interruptible(&data_mtx);
	
	if (retDown != 0) {
		PRINTK("Hey dud, you're interupted.");
		return retDown;
	}
	
	/* TODO: 	do the real thing here 	*/
	/*		do cleanup as well 	*/

	/* step 1. put data into the Q */
	d_info.m_x = data.x;
	d_info.m_y = data.y;
	d_info.m_z = data.z;
	d_info.m_timestamp = get_current_time();

	//kfifo_in(&g_dataq, &d_info, sizeof(struct acc_dev_info));

	/* step 2. check if any event is activated */
	mutex_unlock(&data_mtx);

	PRINTK("accevt_signal: %ld\n", retval);
	return retval;
}

SYSCALL_DEFINE1(accevt_destroy, int, event_id)
{
	long retval = 0;
	struct acc_event_info *evt = NULL;
	struct acc_user_info *iter, *next;

	PRINTK("accevt_destroy\n");

	/* require mutex */
	retval = mutex_lock_interruptible(&data_mtx);

	if (retval != 0) {
		PRINTK("accevt_destroy interrupted\n");
		return retval;
	}

	evt = check_event_exist(event_id);

	if (evt == NULL) {
		mutex_unlock(&data_mtx);
		PRINTK ("event id not found.");
		return -EINVAL;
	}

	/* remove from evt list */
	list_del(&(evt->m_event_list));

	/* iterate user and wake them up */
	/* the user pointer will be free @ wait function */
	list_for_each_entry_safe(iter, next, &(evt->m_wait_list), m_user_list) {
		list_del(&(iter->m_user_list));
		up(&(iter->m_thrd_sema));
	}

	mutex_unlock(&data_mtx);

	kfree(evt);

	return retval;
}
