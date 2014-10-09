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
#define INIT_FRQ		20
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))


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

#if 0
static void configure_default(struct test_setting *set)
{
	set->dlt_x.num = 3;
	set->dlt_x.min = 2;
	set->dlt_x.max = 4;
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
#endif

static void configure_motion(struct acc_motion *ver, 
	struct acc_motion *hor, struct acc_motion *shake){
	/*vertical: high tolerance on y, low tolerance on others*/
	ver->dlt_x = 1;
	ver->dlt_y = 5;
	ver->dlt_z = 1;
	ver->frq = INIT_FRQ;

	/*horizontal: high tolerance on x, low tolerance on others*/
	hor->dlt_x = 5;
	hor->dlt_y = 1;
	hor->dlt_z = 1;
	hor->frq = INIT_FRQ;

	/*shakeup: mid-high tolerance on x,y,z*/
 	shake->dlt_x = 2;
	shake->dlt_y = 2;
	shake->dlt_z = 2;
	shake->frq = INIT_FRQ;
}

#if 0
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
#endif

int main(int argc, char **argv)
{
	int retval = 0;

#if 0
	struct test_setting setting = {0};
	
	int i = 0;
	for (i = 0; i < argc; i++)
		printf("argv[%d] = %s\n", i, argv[i]);

	if (argc < 7) {
		dbg("Test: Insufficient input. Use default setting\n");
		configure_default(&setting);
	} else {
		retval = configure_custom(&setting, argc, argv);
		if (retval < 0)
			configure_default(&setting);
	}
#endif

	/* Set up environment variables */

	dbg("Test: %d starts to dispatch shake detection\n", getpid());

	/* Fork child to detect */
	{
#if 0
		int i, j;
		int dlt, frq;
		pid_t pid;
		unsigned int *evt_id;
		struct acc_motion motion;


		/* evt_id_arr = malloc(sizeof(int) * setting.proc_num); */
		evt_id = malloc(sizeof(int) * setting.dlt_x.num *
				setting.frq.num);
		dbg("Test: evt_id =  %X\n", (unsigned int) evt_id);
		if (evt_id == NULL)
			return -1;

		/* X */
		dbg("Test: Set up X-axis test patern\n");
		for (i = 0; i < setting.dlt_x.num; i++) {
			dlt = setting.dlt_x.min + i * setting.dlt_x.step;
			dbg("Test: dlt_x = %d\n", dlt);
			for (j = 0; j < setting.frq.num; j++) {
				frq = setting.frq.min + j * setting.frq.step;
				dbg("Test: frq = %d\n", frq);
				motion.dlt_x = dlt;
				motion.dlt_y = 0;
				motion.dlt_z = 0;
				motion.frq = frq;
				dbg("Test: check %d\n", i*setting.frq.num+j);
				evt_id[i * setting.frq.num + j] =
					syscall(__NR_accevt_create, &motion);
				dbg("Test: Created event %d - %d %d %d %d\n",
					evt_id[i * setting.frq.num + j],
					motion.dlt_x, motion.dlt_y,
					motion.dlt_z, motion.frq);
			}
		}

		dbg("Test: Fork children to wait on events\n");
		for (i = 0; i < setting.dlt_x.num * setting.frq.num; i++) {
			pid = fork();
			if (pid == 0) {
				dbg("Test: %d Wait on event %d\n",
						getpid(), evt_id[i]);
				retval = syscall(__NR_accevt_wait, evt_id[i]);
				printf("%d detected a shake\n", getpid());
				dbg("Test: Destroy event %d\n", evt_id[i]);
				retval = syscall(__NR_accevt_destroy, evt_id[i]);
				return retval;
			} else if (pid < 0) {
				retval = pid;
			}
		}
#else
		int ver_steps = 2;
		int hor_steps = 2;
		int shake_steps = 2;
		float weight = 0.5;
		pid_t pid;

		int all_steps = hor_steps + ver_steps +shake_steps;
		struct acc_motion ver;
		struct acc_motion hor; 
		struct acc_motion shake;
		configure_motion(&ver,&hor,&shake);
		unsigned int *evt_id;
		int i;

		evt_id = malloc(sizeof(int) * all_steps);
		dbg("Test: evt_id =  %X\n", (unsigned int) evt_id);
		if (evt_id == NULL)
			return -1;

		/* create vertical, horizonal, and shake shakeups */
		dbg("Test: Set up vertical test patern\n");
		for(i = 0; i < ver_steps; i++){
			evt_id[i] = syscall(__NR_accevt_create, &ver);
			dbg("Test: Created event %d - %d %d %d %d\n",
			evt_id[i],
			ver.dlt_x, ver.dlt_y,
			ver.dlt_z, ver.frq);
			ver.dlt_x = round(ver.dlt_x * weight);
			ver.dlt_y = round(ver.dlt_y * weight);
			ver.dlt_z = round(ver.dlt_z * weight);
			ver.frq = round(ver.frq * weight);
		}

		dbg("Test: Set up horizonal test patern\n");
		for(i = ver_steps; i < hor_steps + ver_steps; i++){
			evt_id[i] = syscall(__NR_accevt_create, &hor);
			dbg("Test: Created event %d - %d %d %d %d\n",
			evt_id[i],
			hor.dlt_x, hor.dlt_y,
			hor.dlt_z, hor.frq);
			hor.dlt_x = round(hor.dlt_x * weight);
			hor.dlt_y = round(hor.dlt_y * weight);
			hor.dlt_z = round(hor.dlt_z * weight);
			hor.frq = round(hor.frq * weight);
		}

		dbg("Test: Set up shake test patern\n");
		for(i = hor_steps + ver_steps; i < all_steps; i++){
			evt_id[i] = syscall(__NR_accevt_create, &shake);
			dbg("Test: Created event %d - %d %d %d %d\n",
			evt_id[i],
			shake.dlt_x, shake.dlt_y,
			shake.dlt_z, shake.frq);
			shake.dlt_x = round(shake.dlt_x * weight);
			shake.dlt_y = round(shake.dlt_y * weight);
			shake.dlt_z = round(shake.dlt_z * weight);
			shake.frq = round(shake.frq * weight);
		}

		/* detect events - each event now has 2 processes */
		dbg("Test: Fork children to wait on events\n");
		for (i = 0; i < all_steps; i++) {
			pid = fork();
			if (pid == 0) {
				dbg("Test: %d Wait on event %d\n",
						getpid(), evt_id[i]);
				retval = syscall(__NR_accevt_wait, evt_id[i]);
				retval = syscall(__NR_accevt_wait, evt_id[i]);

				if(i < ver_steps){
					printf("Process ID: %d detected a vertical shake event\n", getpid());
				}else if( i < hor_steps + ver_steps){
					printf("Process ID: %d detected a horizontal shake event\n", getpid());
				}else if( i < all_steps){
					printf("Process ID: %d detected a mix shake event\n", getpid());
				}
				dbg("Test: Destroy event %d\n", evt_id[i]);
				retval = syscall(__NR_accevt_destroy, evt_id[i]);
				return retval;
			} else if (pid < 0) {
				retval = pid;
			}
		}

#endif
		while (wait(NULL) >= 0)
			dbg("Test: %d all children are waited\n", getpid());

	}

	return retval;
}

