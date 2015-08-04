#ifndef _SCGEN_H
#define _SCGEN_H

#include "hashtable.h"
#include "memoryorder.h"
#include "action.h"
#include "scanalysis.h"
#include "model.h"
#include "execution.h"
#include "threads-model.h"
#include "clockvector.h"
#include "sc_annotation.h"

#include <sys/time.h>

typedef struct SCGeneratorOption {
	bool print_always;
	bool print_buggy;
	bool print_nonsc;
	bool annotationMode;
} SCGeneratorOption;


class SCGenerator {
public:
	SCGenerator();
	~SCGenerator();

	bool getCyclic();
	SnapVector<action_list_t>* getDupThreadLists();

	struct sc_statistics* getStats();

	void setExecution(ModelExecution *execution);
	void setActions(action_list_t *actions);
	void setPrintAlways(bool val);
	bool getPrintAlways();
	bool getHasBadRF();

	void setAnnotationMode(bool val);

	void setPrintBuggy(bool val);
	
	void setPrintNonSC(bool val);

	action_list_t * getSCList();
	
	HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4> *getBadrfset();

	HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4 > *getAnnotatedReadSet();

	void print_list(action_list_t *list);

	SNAPSHOTALLOC
private:
	/********************** SC-related stuff (beginning) **********************/
	ModelExecution *execution;

	action_list_t *actions;

	bool fastVersion;
	bool allowNonSC;

	action_list_t * generateSC(action_list_t *list);

	void update_stats();

	int buildVectors(SnapVector<action_list_t> *threadlist, int *maxthread,
		action_list_t *list);

	bool updateConstraints(ModelAction *act);

	void computeCV(action_list_t *list);

	bool processReadFast(ModelAction *read, ClockVector *cv);

	bool processReadSlow(ModelAction *read, ClockVector *cv, bool *updateFuture);

	bool processAnnotatedReadSlow(ModelAction *read, ClockVector *cv, bool *updateFuture);

	int getNextActions(ModelAction **array);

	bool merge(ClockVector *cv, const ModelAction *act, const ModelAction *act2);

	void check_rf(action_list_t *list);
	void check_rf1(action_list_t *list);
	
	void reset(action_list_t *list);
	
	ModelAction* pruneArray(ModelAction **array, int count);

	/** This routine is operated based on the built threadlists */
	void collectAnnotatedReads();

	int maxthreads;
	HashTable<const ModelAction *, ClockVector *, uintptr_t, 4 > cvmap;
	bool cyclic;
	HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4 > badrfset;
	HashTable<void *, const ModelAction *, uintptr_t, 4 > lastwrmap;
	SnapVector<action_list_t> threadlists;
	SnapVector<action_list_t> dup_threadlists;
	bool print_always;
	bool print_buggy;
	bool print_nonsc;
	bool hasBadRF;

	struct sc_statistics *stats;

	/** The set of read actions that are annotated to be special and will
	 *  receive special treatment */
	HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4 > annotatedReadSet;
	int annotatedReadSetSize;
	bool annotationMode;
	bool annotationError;

	/** A set of actions that should be ignored in the partially SC analysis */
	HashTable<const ModelAction*, const ModelAction*, uintptr_t, 4> ignoredActions;
};
#endif
