/**
 * The implemenation of detecting shakes
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

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

static void report_shake(char *type)
{
	if (type == NULL)
		printf("%d detected a shake\n", (int)getpid());
	else
		printf("%d detected a %s shake\n", (int)getpid(), type);
}

int main(int argc, char **argv)
{
	int retval = 0;
	int event_id;
	char *exit;
	struct acc_motion motion;

	if (argc < 5) {
		printf("Insufficient arguments\n");
		return -1;
	}

	motion.dlt_x = (unsigned int)strtol(argv[1], &exit, 10);
	motion.dlt_y = (unsigned int)strtol(argv[2], &exit, 10);
	motion.dlt_z = (unsigned int)strtol(argv[3], &exit, 10);
	motion.frq   = (unsigned int)strtol(argv[4], &exit, 10);

	dbg("(x, y, z, frq) = (%d, %d, %d, %d)\n",
			motion.dlt_x, motion.dlt_y,
			motion.dlt_z, motion.frq);

	event_id = syscall(__NR_accevt_create, &motion);
	dbg("event %d is created\n", event_id);
	if (event_id < 0)
		retval = event_id;
	else
		while (1) {
			retval = syscall(__NR_accevt_wait, event_id);
			if (retval < 0)
				break;
			if (argc < 6)
				report_shake(NULL);
			else
				report_shake(argv[5]);
		}

	return retval;
}
