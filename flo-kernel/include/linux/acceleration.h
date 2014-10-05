/* Header of system calls of hw3 */
/* definitions */
/*****************************************************************************/
/* Define time interval (ms) */
#define TIME_INTERVAL	(200)

/* Define the noise */
#define NOISE		(10)

/* Define the window */
#define WINDOW		(20)

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
				   > NOSIE */
};

/* Self-defined data structure */
struct acc_dev_info {
	unsigned int m_x;
	unsigned int m_y;
	unsigned int m_z;
	unsigned int m_timestamp;
};

struct acc_event_info {
	unsigned int m_eid;
	struct acc_motion m_motion;
	struct list_head m_event_list;
};

struct acc_user_info {
	long m_req_proc;
	unsigned long m_timestamp;
	unsigned int m_activated;
	struct acc_event_info *mp_event;
	struct list_head m_user_list;
	struct mutex thread_mu;
};

