#ifndef __PARAMS_H__
#define __PARAMS_H__

/**
 * Model checker parameter structure. Holds run-time configuration options for
 * the model checker.
 */
struct model_params {
	int maxexecutions;
	bool nofork;
	modelclock_t traceminsize;
	modelclock_t checkthreshold;
	bool removevisible;

	/** @brief Verbosity (0 = quiet; 1 = noisy; 2 = noisier) */
	int verbose;
};

void param_defaults(struct model_params *params);

#endif	/* __PARAMS_H__ */
