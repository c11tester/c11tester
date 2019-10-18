#include <time.h>
#include <unistd.h>

#include "action.h"
#include "model.h"

extern "C" {
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
}

unsigned int __sleep (unsigned int seconds)
{
	model->switch_to_master(
		new ModelAction(NOOP, std::memory_order_seq_cst, NULL)
		);
	return 0;
}

unsigned int sleep(unsigned int seconds)
{
	return __sleep(seconds);
}

int usleep (useconds_t useconds)
{
	struct timespec ts = {
		.tv_sec = (long int) (useconds / 1000000),
		.tv_nsec = (long int) (useconds % 1000000) * 1000l,
	};
	return nanosleep(&ts, NULL);
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
	if (model) {
		uint64_t time = rqtp->tv_sec * 1000000000 + rqtp->tv_nsec;
		struct timespec currtime;
		clock_gettime(CLOCK_MONOTONIC, &currtime);
		uint64_t lcurrtime = currtime.tv_sec * 1000000000 + currtime.tv_nsec;
		model->switch_to_master(new ModelAction(THREAD_SLEEP, std::memory_order_seq_cst, time, lcurrtime));
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
