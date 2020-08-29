#include <time.h>
#include <unistd.h>
#include <sys/param.h>

#include "action.h"
#include "model.h"

extern "C" {
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
}

unsigned int sleep(unsigned int seconds)
{
	/* https://code.woboq.org/userspace/glibc/sysdeps/posix/sleep.c.html */
	const unsigned int max
		= (unsigned int) (((unsigned long int) (~((time_t) 0))) >> 1);

	struct timespec ts = { 0, 0 };
	do {
		if (sizeof (ts.tv_sec) <= sizeof (seconds)) {
			/* Since SECONDS is unsigned assigning the value to .tv_sec can
			   overflow it.  In this case we have to wait in steps.  */
			ts.tv_sec += MIN (seconds, max);
			seconds -= (unsigned int) ts.tv_sec;
		} else {
			ts.tv_sec = (time_t) seconds;
			seconds = 0;
		}

		nanosleep(&ts, &ts);
	} while (seconds > 0);

	return 0;
}

int usleep(useconds_t useconds)
{
	/* https://code.woboq.org/userspace/glibc/sysdeps/posix/usleep.c.html */
	struct timespec ts = {
		.tv_sec = (long int) (useconds / 1000000),
		.tv_nsec = (long int) (useconds % 1000000) * 1000l,
	};
	return nanosleep(&ts, NULL);
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	if (model) {
		uint64_t time = rqtp->tv_sec * 1000000000 + rqtp->tv_nsec;
		struct timespec currtime;
		clock_gettime(CLOCK_MONOTONIC, &currtime);
		uint64_t lcurrtime = currtime.tv_sec * 1000000000 + currtime.tv_nsec;
		model->switch_thread(new ModelAction(THREAD_SLEEP, std::memory_order_seq_cst, time, lcurrtime));
		if (rmtp != NULL) {
			clock_gettime(CLOCK_MONOTONIC, &currtime);
			uint64_t lendtime = currtime.tv_sec * 1000000000 + currtime.tv_nsec;
			uint64_t elapsed = lendtime - lcurrtime;
			rmtp->tv_sec = elapsed / 1000000000;
			rmtp->tv_nsec = elapsed - rmtp->tv_sec * 1000000000;
		}
	}

	return 0;
}
