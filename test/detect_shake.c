/**
 * The impl. of detecting shakes
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
#define INIT_FRQ			20
#define round(x) ((x) >= 0?(long)((x)+0.5):(long)((x)-0.5))

struct acc_motion {
	unsigned int dlt_x;
	unsigned int dlt_y;
	unsigned int dlt_z;
	unsigned int frq;
};

static void configure_motion(struct acc_motion *ver,
	struct acc_motion *hor, struct acc_motion *shake) {
	/*vertical: high tolerance on y, low tolerance on others*/
	ver->dlt_x = 10;
	ver->dlt_y = 500;
	ver->dlt_z = 10;
	ver->frq = INIT_FRQ;

	/*horizontal: high tolerance on x, low tolerance on others*/
	hor->dlt_x = 500;
	hor->dlt_y = 10;
	hor->dlt_z = 10;
	hor->frq = INIT_FRQ;

	/*shakeup: mid-high tolerance on x,y,z*/
	shake->dlt_x = 600;
	shake->dlt_y = 600;
	shake->dlt_z = 600;
	shake->frq = INIT_FRQ;
}

static inline void print_vshake(void)
{
	printf("Process ID: %d detected a vertical shake event\n", getpid());
}

static inline void print_hshake(void)
{
	printf("Process ID: %d detected a horizontal shake event\n", getpid());
}

static inline void print_shake(void)
{
	printf("Process ID: %d detected a mix shake event\n", getpid());
}

int main(int argc, char **argv)
{
	int retval = 0;

	int ver_steps = 3;
	int hor_steps = 3;
	int shake_steps = 3;
	float weight = 0.8;
	pid_t pid;

	int all_steps = hor_steps + ver_steps + shake_steps;
	struct acc_motion ver;
	struct acc_motion hor;
	struct acc_motion shake;
	unsigned int *evt_id;
	int i;

	configure_motion(&ver, &hor, &shake);

	evt_id = malloc(sizeof(int) * all_steps);
	dbg("Test: evt_id =  %X\n", (unsigned int) evt_id);
	if (evt_id == NULL)
		return -1;

	/* create vertical, horizonal, and shake shakeups */
	dbg("Test: Set up vertical test patern\n");
	for (i = 0; i < ver_steps; i++) {
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
	for (i = ver_steps; i < hor_steps + ver_steps; i++) {
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
	for (i = hor_steps + ver_steps; i < all_steps; i++) {
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

	/* detect events */
	dbg("Test: Fork children to wait on events\n");
	for (i = 0; i < all_steps; i++) {
		pid = fork();
		if (pid == 0) {
			dbg("Test: %d Wait on event %d\n",
					getpid(), evt_id[i]);
			retval = syscall(__NR_accevt_wait, evt_id[i]);

			if (i < ver_steps)
				print_vshake();
			else if (i < hor_steps + ver_steps)
				print_hshake();
			else
				print_shake();
			dbg("Test: Destroy event %d\n", evt_id[i]);
			retval = syscall(__NR_accevt_destroy,
					evt_id[i]);
			return retval;
		} else if (pid < 0) {
			retval = pid;
		}

		/* Fork more child to detect events at a later time
		* Child processes comes in as we iterating the event
		* with a stepping time.
		*/

		usleep(500000*i*weight);
		pid = fork();
		if (pid == 0) {
			dbg("Test: %d Wait on event %d\n",
					getpid(), evt_id[i]);
			retval = syscall(__NR_accevt_wait, evt_id[i]);

			if (i < ver_steps)
				print_vshake();
			else if (i < hor_steps + ver_steps)
				print_hshake();
			else
				print_shake();
			dbg("Test: Destroy event %d\n", evt_id[i]);
			retval = syscall(__NR_accevt_destroy,
					evt_id[i]);
			return retval;
		} else if (pid < 0) {
			retval = pid;
		}

	}

	sleep(60);
	printf("Time up!!\n");
	/* Destroy all event */
	for (i = 0; i < all_steps; i++)
		retval = syscall(__NR_accevt_destroy, evt_id[i]);

	while (wait(NULL) >= 0)
		dbg("Test: %d all children are waited\n", getpid());

	return 0;
}

