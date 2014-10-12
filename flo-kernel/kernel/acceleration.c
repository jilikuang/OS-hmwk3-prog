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
#include <linux/idr.h>

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

/* a lock to provide synchronization for idr */
static DEFINE_SPINLOCK(g_id_lock);

/* other data */
/*****************************************************************************/
static BOOL g_init = M_FALSE;
static BOOL g_event_init = M_FALSE;
static BOOL is_valid = M_FALSE;
static struct idr g_event_idr;
static struct acc_fifo g_sensor_data;

#ifdef __HW3_KFIFO__
	/* the buffer used in fifo */
	static char g_fifo_buf[M_FIFO_CAPACITY];
#endif


/*****************************************************************************/
/* Util function to get current time */
/* Note: we do NOT use this function in CRITICAL SECTIONS */
static unsigned int get_current_time(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	return ts.tv_nsec/1000 + (ts.tv_sec)*1000000;
}

#ifndef __HW3_KFIFO__
/* Instead of doing modular, we use if statment */
static int modular_inc(int n)
{
	if (n == WINDOW - 1)
		return 0;
	return (n + 1);
}

static int modular_dec(int n)
{
	if (n == 0)
		return (WINDOW - 1);
	return (n - 1);
}
#endif

/* !!! NOT THREAD-SAFE FUNCTION !!! */
/* !!! called with DATA_MTX section ONLY !!!*/
static BOOL init_fifo(void)
{
	if (g_init == M_TRUE)
		return M_TRUE;

	if (g_init == M_FALSE) {

#ifdef __HW3_KFIFO__
		kfifo_init(&g_sensor_data.m_fifo, g_fifo_buf, M_FIFO_CAPACITY);
#else
		g_sensor_data.m_head = -1;
		g_sensor_data.m_tail = 0;
		g_sensor_data.m_capacity = M_FIFO_CAPACITY;
#endif

		/* init the previous data */
		g_sensor_data.m_prev.x = 0;
		g_sensor_data.m_prev.y = 0;
		g_sensor_data.m_prev.z = 0;
	
		/* set up the init flag in the last */	
		g_init = M_TRUE;
	}

	return M_TRUE;
}

/* !!! NOT THREAD-SAFE FUNCTION !!! */
/* !!! called with DATA_MTX section ONLY !!!*/
static void enqueue_data(
	struct dev_acceleration *in,
	struct acc_dev_info *out,
	struct acc_dev_info *tail,
	BOOL *p_valid_out,
	unsigned int ts) {

	struct dev_acceleration *p_prev;

#ifdef __HW3_KFIFO__
	unsigned int r;
	unsigned int sz_dev_info = sizeof(struct acc_dev_info);
	struct acc_dev_info tmp_dev;
#else
	struct acc_dev_info *p_temp, *p_data;
#endif

	/* default to false */
	*p_valid_out = M_FALSE;

	if (g_init == M_FALSE) {
		PRINTK("FIFO not inited.");
		return;
	}

#ifdef __HW3_KFIFO__
	
	if (kfifo_is_full(&g_sensor_data.m_fifo)) {
		r = kfifo_out(
			&g_sensor_data.m_fifo, 
			out, 
			sz_dev_info);
		
		if (r != sz_dev_info) {
			PRINTK("it's a bug");
			*p_valid_out = M_FALSE;		
		} else
			*p_valid_out = M_TRUE;
	}
	
	p_prev = &(g_sensor_data.m_prev);
	 	
	/* calculate the diff */
	tmp_dev.m_x =
		(in->x > p_prev->x) ?
			in->x - p_prev->x :
			p_prev->x - in->x;
	tmp_dev.m_y =
		(in->y > p_prev->y) ?
			in->y - p_prev->y :
			p_prev->y - in->y;
	tmp_dev.m_z =
		(in->z > p_prev->z) ?
			in->z - p_prev->z :
			p_prev->z - in->z;

	/* save the current data */
	p_prev->x = in->x;
	p_prev->y = in->y;
	p_prev->z = in->z;

	/* set time stamp */
	tmp_dev.m_timestamp = ts;
	
	/* put into the queue */
	KPRINTF("kfifo_in: 0x%x, 0x%x, %d\n", &g_sensor_data.m_fifo, &tmp_dev, sz_dev_info);
	r = kfifo_in(
		&g_sensor_data.m_fifo,
		(const void*)&tmp_dev,
		sz_dev_info); 
	
	if (r != sz_dev_info) {
		PRINTK("failed to fifo in\n");
		return;
	}

	/* copy the current tail back to the caller */
	memcpy(tail, &tmp_dev, sizeof(struct acc_dev_info));

#else
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
	p_data->m_timestamp = ts;

	/* advance the tail */
	g_sensor_data.m_tail = modular_inc(g_sensor_data.m_tail);
#endif
}

/* !!! THREAD-SAFE !!! */
/* the function is used to allocate a new ID */
static BOOL alloc_event_id(
	unsigned int *ap_id,
	struct acc_event_info *new_event)
{
	int ret;

	/* init the idr if not init before */
	spin_lock(&g_id_lock);
	if (g_event_init == M_FALSE) {
		idr_init(&g_event_idr);
		g_event_init = M_TRUE;
	}
	spin_unlock(&g_id_lock);

	/* do the id allocation */ 
	do {
		/* we got no memory - sorry */
		if (idr_pre_get(&g_event_idr, GFP_KERNEL) == 0)
			return M_FALSE;

		/* associate event with id */
		spin_lock(&g_id_lock);
		ret = idr_get_new(&g_event_idr, new_event, ap_id);
		spin_unlock(&g_id_lock);

	} while (ret == -EAGAIN);

	if (ret == 0) {
		PRINTK("event id allocated: %d\n", *ap_id);
		return M_TRUE;
	} else {
		PRINTK("event id allocation failed: %d\n", ret);
		return M_FALSE;
	}
}

static void free_event_id(unsigned int id) {
	
	spin_lock(&g_id_lock);
	idr_remove(&g_event_idr, id);
	spin_unlock(&g_id_lock);
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
	INIT_LIST_HEAD(&new_event->m_wait_list);
	INIT_LIST_HEAD(&new_event->m_event_list);

	/* required by spec: capping with WINDOW size */
	if (new_event->m_motion.frq > WINDOW)
		new_event->m_motion.frq = WINDOW;

	/* allocate id before entering the critical section */
	if (alloc_event_id(&(new_event->m_eid), new_event) == M_FALSE) {
		kfree(new_event);
		return -ENOMEM;
	} 

	/* CRITICAL section: init event with lock */
	ret = mutex_lock_interruptible(&data_mtx);

	if (ret != 0) {
		PRINTK("Failed to obtain event list lock - bye");
		free_event_id(new_event->m_eid);
		kfree(new_event);
		return ret;
	}

	/* init with lock */
	list_add_tail(&(new_event->m_event_list), &g_event_list);
	ret = new_event->m_eid;

	/* unlock */
	mutex_unlock(&data_mtx);

	if (ret < 0) {
		free_event_id(new_event->m_eid);
		kfree(new_event);
	}

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
	} else
		/* 2nd phase of cleaning: wait has to free itself */
		retval = new_user->m_ret_val;

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
	struct dev_acceleration data;
	struct acc_dev_info aged_head;
	struct acc_dev_info current_tail;
	
	struct acc_event_info *evt;
	struct acc_user_info *task, *next_task;
	struct acc_dev_info *p_data;
	struct acc_motion *p_mot;

	long retval = 0;
	int retDown = 0;
	unsigned long sz = sizeof(struct dev_acceleration);
	unsigned int ts = get_current_time();	
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
	
	/* init aged head & current tail */
	memset(&aged_head, 0, sizeof(struct acc_dev_info));
	memset(&current_tail, 0, sizeof(struct acc_dev_info));
	
	retDown = mutex_lock_interruptible(&data_mtx);	
	if (retDown != 0) {
		PRINTK("Hey dud, you're interupted.");
		return retDown;
	}

	/* CRITICAL SECTION */
	/*********************************************************************/
	/* init the fifo: it's okay to call multi times */
	init_fifo();
	
	/* step 1. put data into the Q */
	enqueue_data(&data, &aged_head, &current_tail, &is_valid, ts);

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

			/* new algo using aged_head and is_valid	*/
			/* we don not to iterate everything		*/
			tmp = aged_head.m_x + aged_head.m_y + aged_head.m_z; 

			/* decrement */
			if (is_valid &&
				aged_head.m_timestamp > task->m_timestamp &&
				tmp > NOISE &&
				aged_head.m_x >= p_mot->dlt_x &&
				aged_head.m_y >= p_mot->dlt_y &&
				aged_head.m_z >= p_mot->dlt_z)
				task->m_validCnt--;

			/* increment */
			p_data = &current_tail;
			tmp = p_data->m_x + p_data->m_y + p_data->m_z; 
			if (tmp > NOISE &&
				p_data->m_x >= p_mot->dlt_x &&
				p_data->m_y >= p_mot->dlt_y &&
				p_data->m_z >= p_mot->dlt_z)
				task->m_validCnt++;

			/* remove from the list && wake up the task */
			if (task->m_validCnt >= p_mot->frq) {
				list_del(&(task->m_user_list));
				task->m_activated = M_FALSE;
				up(&(task->m_thrd_sema));
			}
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

	/* Need to do this first - because idr is not reliable here */
	evt = check_event_exist(event_id);
	if (evt == NULL) {
		mutex_unlock(&data_mtx);
		PRINTK("event id not found.");
		return -EINVAL;
	}

	/* iterate user and wake them up */
	/* the user pointer will be free @ wait function */
	list_for_each_entry_safe(iter, next, &(evt->m_wait_list), m_user_list) {
		iter->m_ret_val = -EINVAL;
		list_del(&(iter->m_user_list));
		up(&(iter->m_thrd_sema));
	}

	mutex_unlock(&data_mtx);

	/* should we do it here ? */	
	free_event_id(evt->m_eid);
	kfree(evt);

	return retval;
}

