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
#include <linux/time.h>

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/acceleration.h>

/* LIST HEADs */
/*****************************************************************************/
static LIST_HEAD(g_event_list);

/* MUTEX for data structure */
/*****************************************************************************/
static DEFINE_MUTEX(data_mtx);

/* @lfred: just to prevent multi-daemon or TA's test */
static DEFINE_MUTEX(set_mutex);

/* other data */
/*****************************************************************************/
static unsigned int g_lastId = M_ZERO;
static BOOL g_init = M_FALSE;
static struct acc_fifo g_sensor_data;

/*****************************************************************************/
/* Util function to get current time */
/* This is BUGGY - we should use a monotonic time instead */
static unsigned int get_current_time(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	return ts.tv_nsec/1000 + (ts.tv_sec)*1000000;
}

/* Instead of doing modular, we use if statment */
static int modular_inc(int n)
{
	if (n == WINDOW - 1)
		return 0;
	return (n+1);
}

static int modular_dec(int n)
{
	if (n == 0)
		return (WINDOW - 1);
	return (n-1);
}

/* !!! NOT THREAD-SAFE FUNCTION !!! */
/* !!! called with DATA_MTX section ONLY !!!*/
static BOOL init_fifo(void)
{
	if (g_init == M_TRUE)
		return M_TRUE;

	if (g_init == M_FALSE) {
		g_init = M_TRUE;
		g_sensor_data.m_head = -1;
		g_sensor_data.m_tail = 0;
		g_sensor_data.m_capacity = M_FIFO_CAPACITY;

		/* init the previous data */
		g_sensor_data.m_prev.x = 0;
		g_sensor_data.m_prev.y = 0;
		g_sensor_data.m_prev.z = 0;
	}

	return M_TRUE;
}

/* !!! NOT THREAD-SAFE FUNCTION !!! */
/* !!! called with DATA_MTX section ONLY !!!*/
static void enqueue_data(
	struct dev_acceleration *in,
	struct acc_dev_info *out,
	BOOL *p_valid_out) {

	struct acc_dev_info *p_data, *p_temp;
	struct dev_acceleration *p_prev;

	/* default to false */
	*p_valid_out = M_FALSE;

	if (g_init == M_FALSE) {
		PRINTK("FIFO not inited.");
		return;
	}

	if (g_sensor_data.m_head == -1)
		g_sensor_data.m_head++;
	else {
		/* advance the head, tail is coming */
		if (g_sensor_data.m_head == g_sensor_data.m_tail) {

			/* return the aged head */
			p_temp = &(g_sensor_data.m_buf[g_sensor_data.m_head]);
			out->m_x = p_temp->m_x;
			out->m_y = p_temp->m_y;
			out->m_z = p_temp->m_z;
			out->m_timestamp = p_temp->m_timestamp;
			*p_valid_out = M_TRUE;

			/* age the old head */
			g_sensor_data.m_head =
				modular_inc(g_sensor_data.m_head);
		}
	}

	p_data = &(g_sensor_data.m_buf[g_sensor_data.m_tail]);
	p_prev = &(g_sensor_data.m_prev);

	/* store the diff */
	p_data->m_x =
		(in->x > p_prev->x) ?
			in->x - p_prev->x :
			p_prev->x - in->x;
	p_data->m_y =
		(in->y > p_prev->y) ?
			in->y - p_prev->y :
			p_prev->y - in->y;
	p_data->m_z =
		(in->z > p_prev->z) ?
			in->z - p_prev->z :
			p_prev->z - in->z;

	/* save the current data */
	p_prev->x = in->x;
	p_prev->y = in->y;
	p_prev->z = in->z;

	/* set time stamp */
	p_data->m_timestamp = get_current_time();

	/* advance the tail */
	g_sensor_data.m_tail = modular_inc(g_sensor_data.m_tail);
}

/* !!! NOT THREAD-SAFE !!! */
/* the function is used to allocate a new ID */
/* NOTE: use with lock hold */
static BOOL alloc_event_id(unsigned int *ap_id)
{
	unsigned int startingId = g_lastId;
	struct acc_event_info *iter;
	BOOL found = M_FALSE;

	while (true) {

		list_for_each_entry(iter, &g_event_list, m_event_list) {
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
static struct acc_event_info *check_event_exist(int event_id)
{
	struct acc_event_info *iter;

	if (list_empty(&g_event_list))
		return NULL;

	list_for_each_entry(iter, &g_event_list, m_event_list) {
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
	PRINTK("Received motion: %d %d %d %d\n",
			s_kData.dlt_x, s_kData.dlt_y,
			s_kData.dlt_z, s_kData.frq);

	/* extra-safe check */
	if (s_kData.frq == 0) {
		PRINTK("Undefined behavior - 0 frequency !?\n");
		return -EINVAL;
	}

	/* create the user info memory */
	new_event = kmalloc(szMotion, GFP_KERNEL);

	if (new_event == NULL) {
		PRINTK("No new_event!!!!\n");
		return -ENOMEM;
	}

	/* init without lock */
	memcpy(&new_event->m_motion, &s_kData, sizeof(struct acc_motion));
	INIT_LIST_HEAD(&new_event->m_event_list);
	INIT_LIST_HEAD(&new_event->m_wait_list);

	/* required by spec: capping with WINDOW size */
	if (new_event->m_motion.frq > WINDOW)
		new_event->m_motion.frq = WINDOW;

	/* CRITICAL section: init event with lock */
	ret = mutex_lock_interruptible(&data_mtx);

	if (ret != 0) {
		PRINTK("Failed to obtain event list lock - bye");
		kfree(new_event);
		return ret;
	}

	/* init with lock */
	if (alloc_event_id(&(new_event->m_eid)) == M_TRUE) {
		list_add_tail(&(new_event->m_event_list), &g_event_list);
		ret = new_event->m_eid;
	} else {
		PRINTK("alloc_event_id != M_TRUE!!!!\n");
		kfree(new_event);
		ret = -ENOMEM;
	}

	mutex_unlock(&data_mtx);

	if (ret < 0)
		kfree(new_event);

	PRINTK("accevt_create return: %ld\n", ret);

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

	/* @lfred: jobs needed for accevt_wait		*/
	/* 1. check if the event is correct.		*/
	/* 2. init the user object			*/
	/* 3. link the user object into right place	*/
	/* 4. start waiting				*/
	/* 5. when waking up, do the cleaning		*/

	new_user = kmalloc(sizeof(sz_user), GFP_KERNEL);
	if (new_user == NULL)
		return -ENOMEM;

	/* init the user struct */
	new_user->m_req_proc	= current->pid;
	new_user->m_timestamp	= get_current_time();
	new_user->m_activated	= M_TRUE;
	new_user->m_ret_val	= 0;
	new_user->m_validCnt	= 0;
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

	/* WAKE UP */
	/*********************************************************************/
	/* clean up myself if interrupted .*/
	if (semRet != 0) {
		/* return the semaphore */
		up(&new_user->m_thrd_sema);

		/* must NOT use interruptible here - clean up */
		mutex_lock(&data_mtx);
		list_del(&new_user->m_user_list);
		mutex_unlock(&data_mtx);

		retval = semRet;
	} else {
		/* 2nd phase of cleaning: wait has to free itself */
		retval = new_user->m_ret_val;
	}

	kfree(new_user);

	PRINTK("accevt_wait return: %ld\n", retval);
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
	struct acc_dev_info aged_head;
	struct acc_event_info *evt;
	struct acc_user_info *task, *next_task;
	struct acc_dev_info *p_data;
	struct acc_motion *p_mot;

	BOOL is_valid;
	long retval = 0;
	int retDown = 0;
	/* int matchCount, i = 0 */
	unsigned long sz = sizeof(struct dev_acceleration);
	unsigned int tmp;

	if (acceleration == NULL) {
		PRINTK("set_acceleration NULL pointer param\n");
		return -EINVAL;
	}

	if (copy_from_user(&data, acceleration, sz) != 0) {
		PRINTK("dude - failed to copy. 88\n");
		return -EFAULT;
	}

	PRINTK("accevt_signal\n");
	PRINTK("Received data: %d %d %d\n", data.x, data.y, data.z);

	aged_head.m_x = 0;
	aged_head.m_y = 0;
	aged_head.m_z = 0;
	aged_head.m_timestamp = 0;

	retDown = mutex_lock_interruptible(&data_mtx);

	if (retDown != 0) {
		PRINTK("Hey dud, you're interupted.");
		return retDown;
	}

	/* CRITICAL SECTION */
	/*********************************************************************/
	/* init the fifo: it's okay to call multi times */
	init_fifo();

	/* TODO:	do the real thing here	*/
	/*		do cleanup as well	*/

	/* step 1. put data into the Q */
	enqueue_data(&data, &aged_head, &is_valid);

	/* step 2. check if any event is activated */
	/* TODO: implement the algorithm */
	/* for each event and each user, scan the queue */

	list_for_each_entry(evt, &g_event_list, m_event_list) {

		p_mot = &(evt->m_motion);

		/* iterate the task list */
		list_for_each_entry_safe(
			task, next_task, &(evt->m_wait_list), m_user_list) {

			if (task->m_activated == M_FALSE)
				continue;

			/* TODO:					*/
			/* new algo using aged_head and is_valid	*/
			/* we don not to iterate everything		*/
#ifdef W4118_NAIVE_METHOD
			/* reset match counter */
			matchCount = 0;

			/* iterate the buffer */
			for (i =  g_sensor_data.m_head;
				i != g_sensor_data.m_tail;
				i = modular_inc(i)) {
				p_data = &(g_sensor_data.m_buf[i]);

				/* check timestamp validity */
				if (p_data->m_timestamp < task->m_timestamp)
					continue;

				/* match the count */
				if (p_data->m_x	+ p_data->m_y
					+ p_data->m_z < NOISE)
					continue;

				/* do the real comparison */
				if (p_data->m_x >= p_mot->dlt_x &&
					p_data->m_y >= p_mot->dlt_y &&
					p_data->m_z >= p_mot->dlt_z)
					matchCount++;
			}

			/* remove from the list && wake up the task */
			if (matchCount >= p_mot->frq) {
				list_del(&(task->m_user_list));
				task->m_activated = M_FALSE;
				up(&(task->m_thrd_sema));
			}
#else
			p_data = &(g_sensor_data.m_buf[modular_dec(
						g_sensor_data.m_tail)]);

			tmp = aged_head.m_x + aged_head.m_y + aged_head.m_z;

			if (is_valid &&
				tmp > NOISE &&
				aged_head.m_timestamp > task->m_timestamp &&
				aged_head.m_x >= p_mot->dlt_x &&
				aged_head.m_y >= p_mot->dlt_y &&
				aged_head.m_z >= p_mot->dlt_z) {

				task->m_validCnt--;
			}

			tmp = p_data->m_x + p_data->m_y + p_data->m_z;
			if (tmp > NOISE &&
				p_data->m_x >= p_mot->dlt_x &&
				p_data->m_y >= p_mot->dlt_y &&
				p_data->m_z >= p_mot->dlt_z) {

				task->m_validCnt++;
			}

			/* remove from the list && wake up the task */
			if (task->m_validCnt >= p_mot->frq) {
				list_del(&(task->m_user_list));
				task->m_activated = M_FALSE;
				up(&(task->m_thrd_sema));
			}

#endif

		}
	}

	/* EXIT CRITICAL SECTION */
	/*********************************************************************/
	mutex_unlock(&data_mtx);

	PRINTK("accevt_signal return: %ld\n", retval);
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
		PRINTK("event id not found.");
		return -EINVAL;
	}

	/* remove from evt list */
	list_del(&(evt->m_event_list));

	/* iterate user and wake them up */
	/* the user pointer will be free @ wait function */
	list_for_each_entry_safe(iter, next, &(evt->m_wait_list), m_user_list) {
		iter->m_ret_val = -EINVAL;
		list_del(&(iter->m_user_list));
		up(&(iter->m_thrd_sema));
	}

	mutex_unlock(&data_mtx);
	kfree(evt);

	return retval;
}




