#include <time.h>
#include <unistd.h>

#include "action.h"
#include "model.h"

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
	model->switch_to_master(
		new ModelAction(NOOP, std::memory_order_seq_cst, NULL)
	);
	return 0;
}
