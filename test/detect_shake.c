/**
 * The implemenation of detecting shakes
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <sys/wait.h>

#if 1
#define dbg	printf
#else
#define dbg(...)
#endif

#define __NR_accevt_create	379
#define __NR_accevt_wait	380
#define __NR_accevt_destroy	382

struct acc_motion {
	unsigned int dlt_x;
	unsigned int dlt_y;
	unsigned int dlt_z;
	unsigned int frq;
};

int main(int argc, char **argv)
{
	int retval = 0;
	char *exit;
	unsigned int dlt_num, frq_num;
	unsigned int dlt_min, dlt_max;
	unsigned int frq_min, frq_max;
	unsigned int dlt_stp, frq_stp;
	unsigned long proc_num;
#if 0
	int i = 0;
	for (i = 0; i < argc; i++)
		printf("argv[%d] = %s\n", i, argv[i]);
#endif
	if (argc < 7) {
		printf("Too few arguments\n");
		return -1;
	}

	/* Set up environment variables */
	dlt_num = strtol(argv[1], &exit, 10);
	dlt_min = strtol(argv[2], &exit, 10);
	dlt_max = strtol(argv[3], &exit, 10);
	if (dlt_max < dlt_min) {
		printf("Invalid dlt_max\n");
		return -1;
	} else if (dlt_max == dlt_min) {
		printf("dlt_num enforced to 1\n");
		dlt_num = 1;
	}
	frq_num = strtol(argv[4], &exit, 10);
	frq_min = strtol(argv[5], &exit, 10);
	frq_max = strtol(argv[6], &exit, 10);
	if (frq_max < frq_min) {
		printf("Invalid frq_max\n");
		return -1;
	} else if (frq_max == frq_min) {
		printf("frq_num enforced to 1\n");
		frq_num = 1;
	}

	dbg("%d %d %d / %d %d %d\n",
			dlt_num, dlt_min, dlt_max,
			frq_num, frq_min, frq_max);

	if (dlt_num > 1)
		dlt_stp = (dlt_max - dlt_min) / (dlt_num - 1);
	else
		dlt_stp = 0;
	dbg("%d\n", dlt_stp);
	if (frq_num > 1)
		frq_stp = (frq_max - frq_min) / (frq_num - 1);
	else
		frq_stp = 0;
	dbg("%d\n", frq_stp);
	proc_num = dlt_num * frq_num;
	dbg("%ld\n", proc_num);

	dbg("%d starts to dispatch shake detection\n", getpid());

	/* Fork child to detect */
	{
		pid_t pid;
		int event_id;
		struct acc_motion motion;

		motion.dlt_x = dlt_max;
		motion.dlt_y = 0;
		motion.dlt_z = 0;
		motion.frq = frq_max;
		event_id = syscall(__NR_accevt_create, &motion);
		pid = fork();
		if (pid == 0) {
			retval = syscall(__NR_accevt_wait, event_id);
			printf("%d detected a shake\n", getpid());
			return retval;
		} else if (pid > 0) {
			while (wait(NULL) > 0) ;
		} else {
			retval = pid;
		}
	}

	return retval;
}
