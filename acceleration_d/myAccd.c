/*
 * Columbia University
 * COMS W4118 Fall 2014
 * Homework 3
 *
 */
#include <bionic/errno.h> /* Google does things a little different...*/
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h> /* <-- This is a good place to look! */
#include "../flo-kernel/include/linux/akm8975.h"
#include "acceleration.h"

/* from sensors.c */
#define ID_ACCELERATION   (0)
#define ID_MAGNETIC_FIELD (1)
#define ID_ORIENTATION	  (2)
#define ID_TEMPERATURE	  (3)

#define SENSORS_ACCELERATION   (1<<ID_ACCELERATION)
#define SENSORS_MAGNETIC_FIELD (1<<ID_MAGNETIC_FIELD)
#define SENSORS_ORIENTATION    (1<<ID_ORIENTATION)
#define SENSORS_TEMPERATURE    (1<<ID_TEMPERATURE)


#define __NR_accevt_signal 381


/* set to 1 for a bit of debug output */
#if 0
	#define dbg(fmt, ...) printf("Accelerometer: " fmt, ## __VA_ARGS__)
#else
	#define dbg(fmt, ...)
#endif

static int effective_sensor;
static int g_interval;

/* helper functions which you should use */
static int open_sensors(struct sensors_module_t **hw_module,
			struct sensors_poll_device_t **poll_device);
static void enumerate_sensors(const struct sensors_module_t *sensors);

static int poll_sensor_data(
	struct sensors_poll_device_t *sensors_device,
	struct dev_acceleration *out)
{
	const size_t numEventMax = 16;
	const size_t minBufferSize = numEventMax;
	sensors_event_t buffer[minBufferSize];
	ssize_t count = sensors_device->poll(sensors_device,
			buffer, minBufferSize);
	int i;

	for (i = 0; i < count; ++i) {
		if (buffer[i].sensor != effective_sensor)
			continue;

		/* At this point we should have valid data*/
		/* Scale it and pass it to kernel*/
		dbg("Acceleration: x= %0.2f, y= %0.2f, z= %0.2f\n",
			buffer[i].acceleration.x,
			buffer[i].acceleration.y,
			buffer[i].acceleration.z);

		/* get the int value */
		out->x = (int)(buffer[i].acceleration.x * 100.0);
		out->y = (int)(buffer[i].acceleration.y * 100.0);
		out->z = (int)(buffer[i].acceleration.z * 100.0);
	}
	return 0;
}

static void create_my_daemon(void)
{
	int fd;
	pid_t child;
	uid_t uid = getuid();

	/* it's root only */
	if (uid != 0) {
		dbg("Sorry you are not root\n");
		exit(0);
	}

	child = fork();

	if (child < 0) {
		dbg("You failed to fork - bye bye\n");
		exit(-1);
	}

	if (child > 0) {
		dbg("I am parent - byebye\n");
		exit(0);
	}

	if (setsid() < 0) {
		dbg("Failed to become a session leader\n");
		exit(-1);
	}

	child = fork();

	if (child < 0) {
		dbg("Failed to fork the second time\n");
		exit(-1);
	}

	if (child > 0) {
		dbg("Second parent bye bye\n");
		exit(0);
	}

	if (chdir("/") < 0) {
		dbg("chdir failed\n");
		exit(-1);
	}

	umask(0);

	fd = open("/dev/null", O_RDWR);
	if (fd < 0) {
		dbg("Failed to open NULL device\n");
		exit(-1);
	}
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);

	close(fd);
}

/* entry point: fill in daemon implementation
   where indicated */
int main(int argc, char **argv)
{
	/* local variables */
	struct sensors_module_t *sensors_module = NULL;
	struct sensors_poll_device_t *sensors_device = NULL;
	struct dev_acceleration data;
	char *tmp = NULL;
	long v;

	/* argument check */
	if (argc == 1) {
		/* use default TIME_INTERVAL */
		dbg("Polling Interval: %d\n", g_interval);
		g_interval = TIME_INTERVAL;
	} else if (argc == 2) {
		/* use user defined TIME_INTERVAL */
		v = strtol(argv[1], &tmp, 10);

		if (tmp != NULL && *tmp != '\0') {
			dbg("Invalid input, use default\n");
			g_interval = TIME_INTERVAL;
		} else
			g_interval = (int)v;

	} else {
		dbg("Invalid parameter, use default\n");
		g_interval = TIME_INTERVAL;
	}

	create_my_daemon();

	effective_sensor = -1;

	dbg("Opening sensors...\n");
	if (open_sensors(&sensors_module,
			 &sensors_device) < 0) {
		dbg("open_sensors failed\n");
		return EXIT_FAILURE;
	}
	enumerate_sensors(sensors_module);

	/* Fill in daemon implementation around here */
	dbg("turn me into a daemon!\n");
	while (1) {
		poll_sensor_data(sensors_device, &data);
		syscall(__NR_accevt_signal, &data);

		/* sleep in us, remember to switch */
		usleep(g_interval * 1000);
	}

	return EXIT_SUCCESS;
}

/*                DO NOT MODIFY BELOW THIS LINE                    */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int open_sensors(struct sensors_module_t **mSensorModule,
			struct sensors_poll_device_t **mSensorDevice)
{

	int err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
					(hw_module_t const **)mSensorModule);
	const struct sensor_t *list;
	ssize_t count = (*mSensorModule)->get_sensors_list(
			*mSensorModule, &list);
	size_t i;

	if (err) {
		printf("couldn't load %s module (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorModule)
		return -1;

	err = sensors_open(&((*mSensorModule)->common), mSensorDevice);

	if (err) {
		printf("couldn't open device for module %s (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorDevice)
		return -1;

	for (i = 0 ; i < (size_t)count ; i++) {
		(*mSensorDevice)->setDelay(*mSensorDevice, list[i].handle,
				g_interval * 1000000);
		(*mSensorDevice)->activate(*mSensorDevice, list[i].handle, 1);
	}

	return 0;
}

static void enumerate_sensors(const struct sensors_module_t *sensors)
{
	int nr, s;
	const struct sensor_t *slist = NULL;

	if (!sensors)
		printf("going to fail\n");

	nr = sensors->get_sensors_list((struct sensors_module_t *)sensors,
					&slist);
	if (nr < 1 || slist == NULL) {
		printf("no sensors!\n");
		return;
	}

	for (s = 0; s < nr; s++) {
		printf("%s (%s) v%d\n", slist[s].name, slist[s].vendor,
			slist[s].version);
		printf("\tHandle:%d, type:%d, max:%0.2f, resolution:%0.2f\n",
			slist[s].handle, slist[s].type,
			slist[s].maxRange, slist[s].resolution);

		/* Awful hack to make it work on emulator */
		if (slist[s].type == 1 && slist[s].handle == 0)
			effective_sensor = 0; /*the sensor ID*/

	}
}
