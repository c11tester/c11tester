#ifndef __PARAMS_H__
#define __PARAMS_H__

/**
 * Model checker parameter structure. Holds run-time configuration options for
 * the model checker.
 */
struct model_params {
	int maxreads;
	bool yieldon;
	bool yieldblock;
	unsigned int fairwindow;
	unsigned int enabledcount;
	unsigned int bound;
	unsigned int uninitvalue;
	int maxexecutions;

	/** @brief Verbosity (0 = quiet; 1 = noisy; 2 = noisier) */
	int verbose;

	/** @brief Command-line argument count to pass to user program */
	int argc;

	/** @brief Command-line arguments to pass to user program */
	char **argv;
};

#endif /* __PARAMS_H__ */
