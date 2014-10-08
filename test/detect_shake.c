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

struct item_setting {
	unsigned int num;
	unsigned int min;
	unsigned int max;
	unsigned int step;
};

struct test_setting {
	unsigned long proc_num;
	struct item_setting dlt_x;
	struct item_setting dlt_y;
	struct item_setting dlt_z;
	struct item_setting frq;
};

static void configure_default(struct test_setting *set)
{
	set->dlt_x.num = 3;
	set->dlt_x.min = 10;
	set->dlt_x.max = 20;
	set->dlt_x.step =
		(set->dlt_x.max - set->dlt_x.min) / (set->dlt_x.num - 1);
	set->dlt_y.num = 3;
	set->dlt_y.min = 10;
	set->dlt_y.max = 20;
	set->dlt_y.step =
		(set->dlt_y.max - set->dlt_y.min) / (set->dlt_y.num - 1);
	set->dlt_z.num = 3;
	set->dlt_z.min = 10;
	set->dlt_z.max = 20;
	set->dlt_z.step =
		(set->dlt_z.max - set->dlt_z.min) / (set->dlt_z.num - 1);
	set->frq.num = 3;
	set->frq.min = 10;
	set->frq.max = 20;
	set->frq.step =
		(set->frq.max - set->frq.min) / (set->frq.num - 1);

	set->proc_num = 3 * 3 * 3;
}

static int configure_custom(struct test_setting *set, int argc, char **argv)
{
	char *exit;

	set->dlt_x.num = strtol(argv[1], &exit, 10);
	if (set->dlt_x.num > 0) {
		set->dlt_x.min = strtol(argv[2], &exit, 10);
		set->dlt_x.max = strtol(argv[3], &exit, 10);
		if (set->dlt_x.max < set->dlt_x.min) {
			printf("Test: Invalid dlt_max\n");
			return -1;
		} else if (set->dlt_x.max == set->dlt_x.min) {
			printf("Test: dlt_num enforced to 1\n");
			set->dlt_x.num = 1;
		}
	}

	set->dlt_y.num = strtol(argv[4], &exit, 10);
	if (set->dlt_y.num > 0) {
		set->dlt_y.min = strtol(argv[5], &exit, 10);
		set->dlt_y.max = strtol(argv[6], &exit, 10);
		if (set->dlt_y.max < set->dlt_y.min) {
			printf("Test: Invalid dlt_max\n");
			return -1;
		} else if (set->dlt_y.max == set->dlt_y.min) {
			printf("Test: dlt_num enforced to 1\n");
			set->dlt_y.num = 1;
		}
	}

	set->dlt_z.num = strtol(argv[1], &exit, 10);
	if (set->dlt_z.num > 0) {
		set->dlt_z.min = strtol(argv[2], &exit, 10);
		set->dlt_z.max = strtol(argv[3], &exit, 10);
		if (set->dlt_z.max < set->dlt_z.min) {
			printf("Test: Invalid dlt_max\n");
			return -1;
		} else if (set->dlt_z.max == set->dlt_z.min) {
			printf("Test: dlt_num enforced to 1\n");
			set->dlt_z.num = 1;
		}
	}

	if ((set->dlt_x.num + set->dlt_y.num + set->dlt_z.num) == 0) {
		printf("Test: Invalid setting: dlt_num cannot be all zeros\n");
		return -1;
	}

	set->frq.num = strtol(argv[4], &exit, 10);
	if (set->frq.num == 0) {
		printf("Test: Invalid setting: frq must be assigned\n");
		return -1;
	}
	set->frq.min = strtol(argv[5], &exit, 10);
	set->frq.max = strtol(argv[6], &exit, 10);
	if (set->frq.max < set->frq.min) {
		printf("Test: Invalid frq_max\n");
		return -1;
	} else if (set->frq.max == set->frq.min) {
		printf("Test: frq_num enforced to 1\n");
		set->frq.num = 1;
	}

	if (set->dlt_x.num > 1)
		set->dlt_x.step =
			(set->dlt_x.max - set->dlt_x.min) /
			(set->dlt_x.num - 1);
	else
		set->dlt_x.step = 0;
	if (set->dlt_y.num > 1)
		set->dlt_y.step =
			(set->dlt_y.max - set->dlt_y.min) /
			(set->dlt_y.num - 1);
	else
		set->dlt_y.step = 0;
	if (set->dlt_z.num > 1)
		set->dlt_z.step =
			(set->dlt_z.max - set->dlt_z.min) /
			(set->dlt_z.num - 1);
	else
		set->dlt_z.step = 0;
	if (set->frq.num > 1)
		set->frq.step = (set->frq.max - set->frq.min) /
			(set->frq.num - 1);
	else
		set->frq.step = 0;

	set->proc_num = set->dlt_x.num * set->frq.num +
		set->dlt_y.num * set->frq.num +
		set->dlt_z.num * set->frq.num;

	return 0;
}

int main(int argc, char **argv)
{
	int retval = 0;
	struct test_setting setting = {0};

#if 0
	int i = 0;
	for (i = 0; i < argc; i++)
		printf("argv[%d] = %s\n", i, argv[i]);
#endif
	if (argc < 7) {
		dbg("Test: Insufficient input. Use default setting\n");
		configure_default(&setting);
	} else {
		retval = configure_custom(&setting, argc, argv);
		if (retval < 0)
			configure_default(&setting);
	}

	/* Set up environment variables */

	dbg("Test: %d starts to dispatch shake detection\n", getpid());

	/* Fork child to detect */
	{
		pid_t pid;
		int event_id;
		struct acc_motion motion;

		motion.dlt_x = setting.dlt_x.min;
		motion.dlt_y = 0;
		motion.dlt_z = 0;
		motion.frq = setting.frq.min;
		event_id = syscall(__NR_accevt_create, &motion);
		dbg("Test: Created event %d - %d %d %d %d\n", event_id,
				motion.dlt_x, motion.dlt_y,
				motion.dlt_z, motion.frq);
		pid = fork();
		if (pid == 0) {
			dbg("Test: Wait on event %d\n", event_id);
			retval = syscall(__NR_accevt_wait, event_id);
			printf("%d detected a shake\n", getpid());
			dbg("Test: Destroy event %d\n", event_id);
			retval = syscall(__NR_accevt_destroy, event_id);
		} else if (pid > 0) {
			while (wait(NULL) >= 0) ;
		} else {
			retval = pid;
		}
	}

	return retval;
}
