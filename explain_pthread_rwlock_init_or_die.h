#pragma once
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/** initialize a read-write lock object, see pthread_rwlock_init() */
void explain_pthread_rwlock_init_or_die(pthread_rwlock_t *rwlock, pthread_rwlockattr_t *attr)
{
	int err = pthread_rwlock_init(rwlock, attr);
	if(err)
	{
		fprintf(stderr, "pthread_rwlock_init: %s: ", strerror(err));

		switch(err)
		{
			#define msg(err, msg) case err: fprintf(stderr, "%s\n", msg); break
			msg(EAGAIN, "The system lacked the necessary resources (other than memory) to initialize another read-write lock.");
			msg(ENOMEM, "Insufficient memory exists to initialize the read-write lock.");
			msg(EPERM, "The caller does not have the privilege to perform the operation.");
			msg(EBUSY, "The implementation has detected an attempt to reinitialize the object referenced by rwlock, "
				"a previously initialized but not yet destroyed read-write lock.");
			msg(EINVAL, "The value specified by attr is invalid.");
			#undef msg

			default:
				fprintf(stderr, "Nonstandard error code %d returned.\n", err);
		}

		exit(1);
	}
}