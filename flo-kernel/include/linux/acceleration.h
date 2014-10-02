/* Header of system calls of hw3 */

/*
 * Define time interval (ms)
 */
#define TIME_INTERVAL	(200)

struct dev_acceleration {
	int x;	/* acceleration along X-axis */
	int y;	/* acceleration along Y-axis */
	int z;	/* acceleration along Z-axis */
};

/* Define the noise */
#define NOISE		(10)

/* Define the window */
#define WINDOW		(20)

struct acc_motion {
	unsigned int dlt_x;	/* +/- around X-axis */
	unsigned int dlt_y;	/* +/- around Y-axis */
	unsigned int dlt_z;	/* +/- around Z-axis */
	unsigned int frq;	/* Number of samples that satisfy:
				   sum_each_sample(dlt_x + dlt_y + dlt_z)
				   > NOSIE */
};

#if 1
#define PRINTK	printk
#else
#define PRINTK(...)
#endif
