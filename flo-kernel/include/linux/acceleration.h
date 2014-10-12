#ifndef __ACC_H__
#define __ACC_H__

/* Header of system calls of hw3 */
/* definitions */
/*****************************************************************************/
/* BOOL defines */
#define BOOL		int
#define M_TRUE		(1)
#define	M_FALSE		(0)
#define	M_ZERO		(0)

/* Define time interval (ms) */
#define TIME_INTERVAL	(200)

/* Define the noise */
#define NOISE		(10)

/* Define the window */
#define WINDOW		(1024)
#define M_FIFO_CAPACITY	(WINDOW * sizeof(struct acc_dev_info))

/* event limitations */
#define EVENT_ID_MIN	(10)
#define EVENT_ID_MAX	(0x0FFFFFF0)

#if 1
	#define PRINTK	printk
#else
	#define PRINTK(...)
#endif

/* structs */
/*****************************************************************************/
struct dev_acceleration {
	int x;	/* acceleration along X-axis */
	int y;	/* acceleration along Y-axis */
	int z;	/* acceleration along Z-axis */
};

struct acc_motion {
	unsigned int dlt_x;	/* +/- around X-axis */
	unsigned int dlt_y;	/* +/- around Y-axis */
	unsigned int dlt_z;	/* +/- around Z-axis */
	unsigned int frq;	/* Number of samples that satisfy:
				   sum_each_sample(dlt_x + dlt_y + dlt_z)
				   > NOISE */
};

/* !!! THE SIZE HAS TO BE 16 BYTEs HERE		*/
/* Self-defined data structure			*/
/* data stored in the kfifo			*/
struct acc_dev_info {
	unsigned int m_x;
	unsigned int m_y;
	unsigned int m_z;
	unsigned int m_timestamp;
};

/* created when acc_create is called */
struct acc_event_info {
	unsigned int m_eid;
	struct acc_motion m_motion;

	struct list_head m_event_list;
	struct list_head m_wait_list;
};

/* created when acc_wait is called */
struct acc_user_info {

	long m_req_proc;		/* pid for waiting task */
	unsigned long m_timestamp;	/* the time we start to check */
	BOOL m_activated;
	long m_ret_val;			/* return value from signal */
	unsigned int m_validCnt;	/* accounting info */

	/* control locals */
	struct semaphore m_thrd_sema;
	struct list_head m_user_list;
};

struct acc_fifo {
#ifdef __HW3_KFIFO__
	struct kfifo m_fifo;
#else
	struct acc_dev_info m_buf[WINDOW];
#endif
	struct dev_acceleration  m_prev;

	int m_head;	/* current head */
	int m_tail;	/* next to enqueue */
	int m_capacity;
};

#endif /* __ACC_H__ */

