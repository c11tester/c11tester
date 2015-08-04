#ifndef TRACE_ANALYSIS_H
#define TRACE_ANALYSIS_H
#include "model.h"


class TraceAnalysis {
 public:
	/** setExecution is called once after installation with a reference to
	 *  the ModelExecution object. */

	virtual void setExecution(ModelExecution * execution) = 0;
	
	/** analyze is called once for each feasible trace with the complete
	 *  action_list object. */

	virtual void analyze(action_list_t *) = 0;

	/** name returns the analysis name string */

	virtual const char * name() = 0;

	/** Each analysis option is passed into the option method.  This
	 *	occurs before installation (i.e., you don't have a
	 *	ModelExecution object yet).  A TraceAnalysis object should
	 *	support the option "help"  */

	virtual bool option(char *) = 0;

	/** The finish method is called once at the end.  This should be
	 *  used to print out results.  */

	virtual void finish() = 0;

	/** This method is used to inspect the normal/abnormal model
	 * action. */
	virtual void inspectModelAction(ModelAction *act) {}

	/** This method will be called by when a plugin is installed by the
	 * model checker. */
	virtual void actionAtInstallation() {}

	/** This method will be called when the model checker finishes the
	 * executions; With this method, the model checker can alter the
	 * state of the plugin and then the plugin can choose whether or not
	 * restart the model checker. */
	virtual void actionAtModelCheckingFinish() {}

	SNAPSHOTALLOC
};
#endif
