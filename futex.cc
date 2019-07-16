// futex -*- C++ -*-

// Copyright (C) 2015-2019 Free Software Foundation, Inc.
//
// This is a reimplementation of libstdc++-v3/src/c++11/futex.cc.

#include <bits/atomic_futex.h>
#ifdef _GLIBCXX_HAS_GTHREADS
#if defined(_GLIBCXX_HAVE_LINUX_FUTEX) && ATOMIC_INT_LOCK_FREE > 1
#include <chrono>
#include <climits>
#include <syscall.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <debug/debug.h>

#include "model.h"
#include "execution.h"
#include "mutex.h"
#include <condition_variable>

// Constants for the wait/wake futex syscall operations
const unsigned futex_wait_op = 0;
const unsigned futex_wake_op = 1;

namespace std _GLIBCXX_VISIBILITY(default)
{
	_GLIBCXX_BEGIN_NAMESPACE_VERSION

	bool
	__atomic_futex_unsigned_base::_M_futex_wait_until(unsigned *__addr,
																										unsigned __val,
																										bool __has_timeout, chrono::seconds __s, chrono::nanoseconds __ns)
	{
		// do nothing if the two values are not equal
		if ( *__addr != __val ) {
			return true;
		}

		// if a timeout is specified, return immedialy. Letting the scheduler decide how long this thread will wait.
		if (__has_timeout) {
			return true;
		}

		ModelExecution *execution = model->get_execution();

		cdsc::snapcondition_variable *v = new cdsc::snapcondition_variable();
		cdsc::snapmutex *m = new cdsc::snapmutex();

		execution->getCondMap()->put( (pthread_cond_t *) __addr, v);
		execution->getMutexMap()->put( (pthread_mutex_t *) __addr, m);

		v->wait(*m);
		return true;
	}

	void
	__atomic_futex_unsigned_base::_M_futex_notify_all(unsigned* __addr)
	{
		// INT_MAX wakes all the waiters at the address __addr
		ModelExecution *execution = model->get_execution();
		cdsc::condition_variable *v = execution->getCondMap()->get( (pthread_cond_t *) __addr);

		if (v == NULL)
			return;// do nothing

		v->notify_all();
	}

	_GLIBCXX_END_NAMESPACE_VERSION
}
#endif	// defined(_GLIBCXX_HAVE_LINUX_FUTEX) && ATOMIC_INT_LOCK_FREE > 1
#endif	// _GLIBCXX_HAS_GTHREADS
