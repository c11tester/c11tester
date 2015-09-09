#include "scfence.h"
#include "action.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"
#include "cyclegraph.h"
#include <sys/time.h>

#include "model.h"
#include "wildcard.h"
#include "inference.h"
#include "inferset.h"
#include "sc_annotation.h"
#include "errno.h"
#include <stdio.h>
#include <algorithm>

scfence_priv *SCFence::priv;

SCFence::SCFence() :
	scgen(new SCGenerator),
	stats((struct sc_statistics *)model_calloc(1, sizeof(struct sc_statistics))),
	execution(NULL)
{
	priv = new scfence_priv();
	weaken = true;
}

SCFence::~SCFence() {
	delete(stats);
}

void SCFence::setExecution(ModelExecution * execution) {
	this->execution=execution;
}

const char * SCFence::name() {
	const char * name = "AUTOMO";
	return name;
}

void SCFence::finish() {
	model_print("SCFence finishes!\n");
}


/******************** SCFence-related Functions (Beginning) ********************/

void SCFence::inspectModelAction(ModelAction *act) {
	if (act == NULL) // Get pass cases like data race detector
		return;
	if (act->get_mo() >= memory_order_relaxed && act->get_mo() <=
		memory_order_seq_cst) {
		return;
	} else if (act->get_mo() == memory_order_normal) {
		// For the purpose of eliminating data races
		act->set_mo(memory_order_relaxed);
	} else { // For wildcards
		Inference *curInfer = getCurInference();
		int wildcardID = get_wildcard_id(act->get_mo());
		memory_order order = (*curInfer)[wildcardID];
		if (order == WILDCARD_NONEXIST) {
			(*curInfer)[wildcardID] = memory_order_relaxed;
			act->set_mo(memory_order_relaxed);
		} else {
			act->set_mo((*curInfer)[wildcardID]);
		}
	}
}


void SCFence::actionAtInstallation() {
	// When this pluggin gets installed, it become the inspect_plugin
	model->set_inspect_plugin(this);
}

void SCFence::analyze(action_list_t *actions) {
	scgen->setActions(actions);
	scgen->setExecution(execution);
	dup_threadlists = scgen->getDupThreadLists();
	if (getTimeout() > 0 && isTimeout()) {
		model_print("Backtrack because we reached the timeout bound.\n");
		routineBacktrack(false);
		return;
	}

	/* Build up the thread lists for general purpose */
	mo_graph = execution->get_mo_graph();

	// First of all check if there's any uninitialzed read bugs
	if (execution->have_bug_reports()) {
		setBuggy(true);
	}

	action_list_t *list = scgen->getSCList();
	bool cyclic = scgen->getCyclic();
	
	struct sc_statistics *s = scgen->getStats();
	stats->nonsccount += s->nonsccount;
	stats->sccount += s->sccount;
	stats->elapsedtime += s->elapsedtime;
	stats->actions += s->actions;

	if (!cyclic && execution->have_bug_reports()) {
		model_print("Be careful. This execution has bugs and still SC\n");
	}

	if (!cyclic && scgen->getPrintAlways()) {
		scgen->print_list(list);
	}

	// Now we find a non-SC execution
	if (cyclic) {
		/******** The Non-SC case (beginning) ********/
		// Only print those that should be fixed
		if (getCurInference()->getShouldFix())
			scgen->print_list(list);
		bool added = addFixes(list, NON_SC);
		if (added) {
			routineAfterAddFixes();
			return;
		} else { // Couldn't imply anymore, backtrack
			routineBacktrack(false);
			return;
		}
		/******** The Non-SC case (end) ********/
	} else {
		/******** The implicit MO case (beginning) ********/
		if (getInferImplicitMO() && execution->too_many_steps() &&
			!execution->is_complete_execution()) {
			// Only print those that should be fixed
			if (getCurInference()->getShouldFix())
				scgen->print_list(list);
			bool added = addFixes(list, IMPLICIT_MO);
			if (added) {
				FENCE_PRINT("Found an implicit mo pattern to fix!\n");
				routineAfterAddFixes();
				return;
			} else {
				// This can be a good execution, so we can't do anything,
				return;
			}
		}
		/******** The implicit MO case (end) ********/

		/******** The checking data races case (beginning) ********/
		bool added = addFixes(list, DATA_RACE);
		if (added) {
			FENCE_PRINT("Found an data race pattern to fix!\n");
			routineAfterAddFixes();
			return;
		}
	}
}

void SCFence::exitModelChecker() {
	model->exit_model_checker();
}

void SCFence::restartModelChecker() {
	model->restart();
	if (!getHasRestarted())
		setHasRestarted(true);
}

bool SCFence::modelCheckerAtExitState() {
	return model->get_exit_flag();
}

void SCFence::actionAtModelCheckingFinish() {
	// Just bail if the model checker is at the exit state
	if (modelCheckerAtExitState())
		return;

	/** Backtrack with a successful inference */
	routineBacktrack(true);
}

void SCFence::initializeByFile() {
	char *name = getCandidateFile();
	FILE *fp = fopen(name, "r");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open the input parameter assignment file: %s!\n", name);
		perror(name);
		exit(EXIT_FAILURE);
	}
	Inference *infer = NULL;
	int curNum = 0;
	memory_order mo = memory_order_relaxed;
	char *str = (char *) malloc(sizeof(char) * (30 + 1));
	bool isProcessing = false;
	int ret = 0;
	while (!feof(fp)) {
		// Result #:
		if (!isProcessing) {
			ret = fscanf(fp, "%s", str);
		}
		if (strcmp(str, "Result") == 0 || isProcessing) { // In an inference
			ret = fscanf(fp, "%s", str);
			
			infer = new Inference();
			isProcessing = false;
			while (!feof(fp)) { // Processing a specific inference
				// wildcard # -> memory_order
				ret = fscanf(fp, "%s", str);
					
				if (strcmp(str, "Result") == 0) {
					isProcessing = true;
					break;
				}
				ret = fscanf(fp, "%d", &curNum);
				ret = fscanf(fp, "%s", str);
				if (ret <= 0 && ret != -1) {
					fprintf(stderr, "The input parameter assignment file has wrong format\n");
					perror(name);
					exit(EXIT_FAILURE);
				}
				ret = fscanf(fp, "%s", str);
				 
				if (strcmp(str, "memory_order_relaxed") == 0)
					mo = memory_order_relaxed;
				else if (strcmp(str, "memory_order_acquire") == 0)
					mo = memory_order_acquire;
				else if (strcmp(str, "memory_order_release") == 0)
					mo = memory_order_release;
				else if (strcmp(str, "memory_order_acq_rel") == 0)
					mo = memory_order_acq_rel;
				else if (strcmp(str, "memory_order_seq_cst") == 0)
					mo = memory_order_seq_cst;
				(*infer)[curNum] = mo;
			}

			/******** addInference ********/
			if (!addInference(infer))
				delete infer;
		}
	}
	fclose(fp);

	FENCE_PRINT("candidate size from file: %d\n", setSize());
	Inference *next = getNextInference();
	if (!next) {
		model_print("Wrong with the candidate file\n");
	} else {
		setCurInference(next);
	}
}

char* SCFence::parseOptionHelper(char *opt, int *optIdx) {
	char *res = (char*) model_malloc(1024 * sizeof(char));
	int resIdx = 0;
	while (opt[*optIdx] != '\0' && opt[*optIdx] != '|') {
		res[resIdx++] = opt[(*optIdx)++];
	}
	res[resIdx] = '\0';
	return res;
}

bool SCFence::parseOption(char *opt) {
	int optIdx = 0;
	char *val = NULL;
	while (true) {
		char option = opt[optIdx++];
		val = parseOptionHelper(opt, &optIdx);
		switch (option) {
			case 'f': // Read initial inference from file
				setCandidateFile(val);
				if (strcmp(val, "") == 0) {
					model_print("Need to specify a file that contains initial inference!\n");
					return true;
				}
				break;
			case 'b': // The bound above 
				setImplicitMOReadBound(atoi(val));
				if (getImplicitMOReadBound() <= 0) {
					model_print("Enter valid bound value!\n");
					return true;
				}
				break;
			case 'm': // Infer the modification order from repetitive reads from
					  // the same write
				setInferImplicitMO(true);
				if (strcmp(val, "") != 0) {
					model_print("option m doesn't take any arguments!\n");
					return true;
				}
				break;
			case 'a': // Turn on the annotation mode 
				scgen->setAnnotationMode(true);
				if (strcmp(val, "") != 0) {
					model_print("option a doesn't take any arguments!\n");
					return true;
				}
				break;
			case 't': // The timeout set to force the analysis to backtrack
				model_print("Parsing t option!\n");
				model_print("t value: %s\n", val);
				setTimeout(atoi(val));
				break;
			default:
				model_print("Unknown SCFence option: %c!\n", option);
				return true;
				break;
		}
		if (opt[optIdx] == '|') {
			optIdx++;
		} else {
			break;
		}
	}
	return false;

}

char* SCFence::getInputFileName(char *opt) {
	char *res = NULL;
	if (opt[0] == 'f' &&
		opt[1] == 'i' &&
		opt[2] == 'l' &&
		opt[3] == 'e' &&
		opt[4] == '-') {
		
		res = (char*) model_malloc(1024 * sizeof(char));
		int i = 0, j = 5;
		while (opt[j] != '\0') {
			res[i++] = opt[j++];
		}
		res[i] = '\0';
	}
	return res;
}

int SCFence::getImplicitMOBound(char *opt) {
	char *num = NULL;
	if (opt[0] == 'b' &&
		opt[1] == 'o' &&
		opt[2] == 'u' &&
		opt[3] == 'n' &&
		opt[4] == 'd' &&
		opt[5] == '-') {
		
		num = (char*) model_malloc(1024 * sizeof(char));
		int i = 0, j = 6;
		while (opt[j] != '\0') {
			num[i++] = opt[j++];
		}
		num[i] = '\0';
	}
	if (num) {
		return atoi(num);
	} else {
		return 0;
	}
}

bool SCFence::option(char * opt) {
	char *inputFileName = NULL;
	unsigned implicitMOBoundNum = 0;

	if (strcmp(opt, "verbose")==0) {
		scgen->setPrintAlways(true);
		return false;
	} else if (strcmp(opt, "buggy")==0) {
		return false;
	} else if (strcmp(opt, "quiet")==0) {
		scgen->setPrintBuggy(false);
		return false;
	} else if (strcmp(opt, "nonsc")==0) {
		scgen->setPrintNonSC(true);
		return false;
	} else if (strcmp(opt, "time")==0) {
		time=true;
		return false;
	} else if (strcmp(opt, "no-weaken")==0) {
		weaken=false;
		return false;
	} else if (strcmp(opt, "anno")==0) {
		scgen->setAnnotationMode(true);
		return false;
	} else if (strcmp(opt, "implicit-mo")==0) {
		setInferImplicitMO(true);
		return false;
	} else if ((inputFileName = getInputFileName(opt)) != NULL) {
		if (strcmp(inputFileName, "") == 0) {
			model_print("Need to specify a file that contains initial inference!\n");
			return true;
		}
		setCandidateFile(inputFileName);
		initializeByFile();
		return false;
	} else if ((implicitMOBoundNum = getImplicitMOBound(opt)) > 0) {
		setImplicitMOReadBound(implicitMOBoundNum);
		return false;
	} else {
		model_print("file-InputFile -- takes candidate file as argument right after the symbol '-' \n");
		model_print("no-weaken -- turn off the weakening mode (by default ON)\n");
		model_print("anno -- turn on the annotation mode (by default OFF)\n");
		model_print("implicit-mo -- imply implicit modification order, takes no arguments (by default OFF, default bound is %d\n", DEFAULT_REPETITIVE_READ_BOUND);
		model_print("bound-NUM -- specify the bound for the implicit mo implication, takes a number as argument right after the symbol '-'\n");
		model_print("\n");
		return true;
	}
}

/** Check whether a chosen reads-from path is a release sequence */
bool SCFence::isReleaseSequence(path_t *path) {
	ASSERT (path);
	path_t::reverse_iterator rit = path->rbegin(),
		rit_next;
	const ModelAction *read,
		*write,
		*next_read;
	for (; rit != path->rend(); rit++) {
		read = *rit;
		rit_next = rit;
		rit_next++;
		write = read->get_reads_from();
		if (rit_next != path->rend()) {
			next_read = *rit_next;
			if (write != next_read) {
				return false;
			}
		}
	}
	return true;
}

/** Only return those applicable patches in a vector */
SnapVector<Patch*>* SCFence::getAcqRel(const ModelAction *read, const
	ModelAction *readBound,const ModelAction *write, const ModelAction *writeBound) {
	
	SnapVector<Patch*> *patches = new SnapVector<Patch*>;
	/* Also support rel/acq fences synchronization here */
	// Look for the acq fence after the read action
	int readThrdID = read->get_tid(),
		writeThrdID = write->get_tid();
	action_list_t *readThrd = &(*dup_threadlists)[readThrdID],
		*writeThrd = &(*dup_threadlists)[writeThrdID];
	action_list_t::iterator readIter = std::find(readThrd->begin(),
		readThrd->end(), read);
	action_list_t::reverse_iterator writeIter = std::find(writeThrd->rbegin(),
		writeThrd->rend(), write);

	action_list_t *acqFences = new action_list_t,
		*relFences = new action_list_t;
	ModelAction *act = *readIter;
	while (readIter++ != readThrd->end() && act != readBound) {
		if (act->is_fence()) {
			acqFences->push_back(act);
		}
		act = *readIter;
	}
	act = *writeIter;
	while (writeIter++ != writeThrd->rend() && act != writeBound) {
		if (act->is_fence()) {
			relFences->push_back(act);
		}
		act = *writeIter;
	}
	// Now we have a list of rel/acq fences at hand
	int acqFenceSize = acqFences->size(),
		relFenceSize = relFences->size();
	
	Patch *p;
	if (acqFenceSize == 0 && relFenceSize == 0) {
		//return patches;
	 } else if (acqFenceSize > 0 && relFenceSize == 0) {
		for (action_list_t::iterator it = acqFences->begin(); it !=
			acqFences->end(); it++) {
			ModelAction *fence = *it;
			p = new Patch(fence, memory_order_acquire, write, memory_order_release);
			if (p->isApplicable())
				patches->push_back(p);
		}
	} else if (acqFenceSize == 0 && relFenceSize > 0) {
		for (action_list_t::iterator it = relFences->begin(); it !=
			relFences->end(); it++) {
			ModelAction *fence = *it;
			p = new Patch(fence, memory_order_release, read, memory_order_acquire);
			if (p->isApplicable())
				patches->push_back(p);
		}
	} else {
		/* Impose on both relFences and acqFences */
		bool twoFences = false;
		for (action_list_t::iterator it1 = relFences->begin(); it1 !=
			relFences->end(); it1++) {
			ModelAction *relFence = *it1;
			for (action_list_t::iterator it2 = acqFences->begin(); it2 !=
				acqFences->end(); it2++) {
				ModelAction *acqFence = *it2;
				p = new Patch(relFence, memory_order_release, acqFence, memory_order_acquire);
				if (p->isApplicable()) {
					patches->push_back(p);
					twoFences = true;
				}
			}
		}
		if (!twoFences) {
			/* Only impose on the acqFences */
			for (action_list_t::iterator it = acqFences->begin(); it !=
				acqFences->end(); it++) {
				ModelAction *fence = *it;
				p = new Patch(fence, memory_order_acquire, write, memory_order_release);
				if (p->isApplicable())
					patches->push_back(p);
			}
			/* Only impose on the relFences */
			for (action_list_t::iterator it = relFences->begin(); it !=
				relFences->end(); it++) {
				ModelAction *fence = *it;
				p = new Patch(fence, memory_order_release, read, memory_order_acquire);
				if (p->isApplicable())
					patches->push_back(p);
			}
		}
	}

	// If we can find a fix with fences, we don't fix on the operation
	if (patches->size() > 0)
		return patches;
	p = new Patch(read, memory_order_acquire, write,
		memory_order_release);
	if (p->isApplicable())
		patches->push_back(p);
	return patches;
}

/** Impose the synchronization between the begin and end action, and the paths
 *  are a list of paths that each represent the union of rfUsb. We then
 *  strengthen the current inference by necessity.
 */
bool SCFence::imposeSync(InferenceList *inferList,
	paths_t *paths, const ModelAction *begin, const ModelAction *end) {
	if (!inferList)
		return false;
	bool res = false;
	for (paths_t::iterator i_paths = paths->begin(); i_paths != paths->end(); i_paths++) {
		// Iterator over all the possible paths
		path_t *path = *i_paths;
		InferenceList *cands = new InferenceList;
		// Impose synchronization by path
		if (imposeSync(cands, path, begin, end))
			res = true;
		inferList->append(cands);
	}
	return res;
}

/** Impose the synchronization between the begin and end action, and the path
 *  is the union of rfUsb. We then strengthen the current inference by
 *  necessity.
 */
bool SCFence::imposeSync(InferenceList *inferList,
	path_t *path, const ModelAction *begin, const ModelAction *end) {
	
	bool release_seq = isReleaseSequence(path);
	SnapVector<Patch*> *patches;
	if (release_seq) {
		const ModelAction *relHead = path->front()->get_reads_from(),
			*lastRead = path->back();
		patches = getAcqRel(lastRead, end, relHead, begin);
		if (patches->size() == 0)
			return false;
		inferList->applyPatch(getCurInference(), patches);
		delete patches;
		// Bail now for the release sequence because 
		return true;
	}

	for (path_t::iterator it = path->begin(); it != path->end(); it++) {
		const ModelAction *read = *it,
			*write = read->get_reads_from(),
			*prevRead = NULL, *nextRead;
		
		const ModelAction *readBound = NULL,
			*writeBound = NULL;
		nextRead = *++it;
		if (it == path->end()) {
			nextRead = NULL;
		}
		it--;
		if (prevRead) {
			readBound = prevRead->get_reads_from();
		} else {
			readBound = end;
		}
		if (nextRead) {
			writeBound = nextRead;
		} else {
			writeBound = begin;
		}

		/* Extend to support rel/acq fences synchronization here */
		patches = getAcqRel(read, readBound, write, writeBound);

		if (patches->size() == 0) {
			// We cannot strengthen the inference
			return false;
		}

		inferList->applyPatch(getCurInference(), patches);
		delete patches;
	}
	return true;
}

/** Impose SC orderings to related actions (including fences) such that we
 *  either want to establish mo between act1 & act2 (act1 -mo-> act2) when they
 *  can't establish hb; or that act2 can't read from any actions that are
 *  modification ordered before act1. All these requirements are consistent with
 *  the following strengthening strategy:
 *  1. make both act1 and act2 SC
 *  2. if there are fences in between act1 & act2, and the fences are either in
 *  the threads of either act1 or act2, we can impose SC on the fences or
 *  corresponding actions. 
 */
bool SCFence::imposeSC(action_list_t * actions, InferenceList *inferList, const ModelAction *act1, const ModelAction *act2) {
	if (!inferList) {
		return false;
	}
	action_list_t::iterator act1Iter = std::find(actions->begin(),
		actions->end(), act1);
	action_list_t::iterator act2Iter = std::find(act1Iter,
		actions->end(), act2);

	action_list_t::iterator it = act1Iter;
	it++;
	action_list_t *fences = new action_list_t;
	int size = 0;
	while (it != act2Iter) {
		ModelAction *fence = *it;
		it++;
		if (!fence->is_fence())
			continue;
		if (!is_wildcard(fence->get_original_mo()))
			continue;
		fences->push_back(fence);
		size++;
	}

	Patch *p;
	SnapVector<Patch*> *patches = new SnapVector<Patch*>;
	
	bool twoFences = false;
	// Impose SC on two fences
	for (action_list_t::iterator fit1 = fences->begin(); fit1 != fences->end();
		fit1++) {
		ModelAction *fence1 = *fit1;
		action_list_t::iterator fit2 = fit1;
		fit2++;
		for (; fit2 != fences->end(); fit2++) {
			ModelAction *fence2 = *fit2;
			p = new Patch(fence1, memory_order_seq_cst, fence2, memory_order_seq_cst);
			if (p->isApplicable()) {
				Inference *curInfer = getCurInference();
				memory_order mo1 = (*curInfer)[get_wildcard_id(fence1->get_original_mo())];
				memory_order mo2 = (*curInfer)[get_wildcard_id(fence2->get_original_mo())];
				// We can avoid this by adding two fences
				if (mo1 != memory_order_seq_cst || mo2 != memory_order_seq_cst)
					twoFences = true;
				patches->push_back(p);
			}
		}
	}

	// Just impose SC on one fence
	if (!twoFences) {
		for (action_list_t::iterator fit = fences->begin(); fit != fences->end();
			fit++) {
			ModelAction *fence = *fit;
			model_print("one fence\n");
			fence->print();
			p = new Patch(act1, memory_order_seq_cst, fence, memory_order_seq_cst);
			if (p->isApplicable()) {
				// We can avoid this by adding two fences
				patches->push_back(p);
			}
			p = new Patch(act2, memory_order_seq_cst, fence, memory_order_seq_cst);
			if (p->isApplicable()) {
				// We can avoid this by adding two fences
				patches->push_back(p);
			}
		}

		p = new Patch(act1, memory_order_seq_cst, act2, memory_order_seq_cst);
		if (p->isApplicable()) {
			patches->push_back(p);
		}
	}
	
	if (patches->size() > 0) {
		inferList->applyPatch(getCurInference(), patches);
		return true;
	}
	return false;
}

/** A subroutine that calculates the potential fixes for the non-sc pattern (a),
 *  a.k.a old value reading. The whole idea is as follows.
 *	write -isc-> write2 && write2 -isc->read && write -rf-> read!!!
 *	The fix can be two-step:
 *	a. make write -mo-> write2, and we can accomplish this by imposing hb
 *	between write and write2, and if not possible, make write -sc-> write2
 *	b. make write2 -hb-> read, and if not possible, make write2 -sc-> read.
 */
InferenceList* SCFence::getFixesFromPatternA(action_list_t *list, action_list_t::iterator readIter, action_list_t::iterator writeIter) {
	ModelAction *read = *readIter,
		*write = *writeIter;

	InferenceList *candidates = new InferenceList;
	paths_t *paths1, *paths2;

	// Find all writes between the write1 and the read
	action_list_t *write2s = new action_list_t();
	ModelAction *write2;
	action_list_t::iterator findIt = writeIter;
	findIt++;
	do {
		write2 = *findIt;
		if (write2->is_write() && write2->get_location() ==
			write->get_location()) {
			write2s->push_back(write2);
		}
		findIt++;
	} while (findIt != readIter);
					
	// Found all the possible write2s
	FENCE_PRINT("write2s set size: %ld\n", write2s->size());
	for (action_list_t::iterator itWrite2 = write2s->begin();
		itWrite2 != write2s->end(); itWrite2++) {
		InferenceList *tmpCandidates = new InferenceList;
		write2 = *itWrite2;
		// Whether the write and the write2 originally have modification order
		bool isWritesMO = false;
		FENCE_PRINT("write2:\n");
		ACT_PRINT(write2);
		// write1->write2 (write->write2)
		// Whether write -mo-> write2
		if (!mo_graph->checkReachable(write, write2)) {
			paths1 = get_rf_sb_paths(write, write2);
			if (paths1->size() > 0) {
				FENCE_PRINT("From write1 to write2: \n");
				print_rf_sb_paths(paths1, write, write2);
				// FIXME: Need to make sure at least one path is feasible; what
				// if we got empty candidates here, maybe should then impose SC,
				// same in the write2->read
				imposeSync(tmpCandidates, paths1, write, write2);
			} else {
				FENCE_PRINT("Have to impose sc on write1 & write2: \n");
				ACT_PRINT(write);
				ACT_PRINT(write2);
				imposeSC(list, tmpCandidates, write, write2);
			}
		} else {
			if (!write->happens_before(write2)) {
				isWritesMO = true;
			}
			FENCE_PRINT("write1 mo before write2. \n");
		}

		// write2->read (write2->read)
		// now we only need to make write2 -hb-> read 
		if (!write2->happens_before(read)) {
			paths2 = get_rf_sb_paths(write2, read);
			if (paths2->size() > 0) {
				FENCE_PRINT("From write2 to read: \n");
				print_rf_sb_paths(paths2, write2, read);
				imposeSync(tmpCandidates, paths2, write2, read);
			} else {
				FENCE_PRINT("Have to impose sc on write2 & read: \n");
				ACT_PRINT(write2);
				ACT_PRINT(read);
				imposeSC(list, tmpCandidates, write2, read);
				if (isWritesMO) {
					// Also need to make sure write -sc/hb-> write2
					FENCE_PRINT("Also have to impose hb/sc on write & write2: \n");
					ACT_PRINT(write);
					ACT_PRINT(write2);
					paths1 = get_rf_sb_paths(write, write2);
					if (paths1->size() > 0) {
						FENCE_PRINT("Impose hb, from write1 to write2: \n");
						print_rf_sb_paths(paths1, write, write2);
						imposeSync(tmpCandidates, paths1, write, write2);
					} else {
						FENCE_PRINT("Also have to impose sc on write1 & write2: \n");
						ACT_PRINT(write);
						ACT_PRINT(write2);
						imposeSC(list, tmpCandidates, write, write2);
					}
				}
			}
		} else {
			FENCE_PRINT("write2 hb before read. \n");
		}
		candidates->append(tmpCandidates);
	}
	// Return the complete list of candidates
	return candidates;
}

/** To fix this pattern, we have two options:
 *	1. impose futureWrite -hb-> read so that the SC analysis will order
 *	in a way that the the write is ordered before the read;
 *	2. impose read -hb->futureWrite so that the reads-from edge from futureWrite
 *	to the read is not allowed.
*/
InferenceList* SCFence::getFixesFromPatternB(action_list_t *list, action_list_t::iterator readIter, action_list_t::iterator writeIter) {
	InferenceList *res = new InferenceList,
		*candidates = new InferenceList;
	
	ModelAction *read = *readIter,
		*write = *writeIter;
	// Fixing one direction (read -> futureWrite)
	paths_t *paths1 = get_rf_sb_paths(read, write);
	if (paths1->size() > 0) {
		FENCE_PRINT("From read to future write: \n");
		print_rf_sb_paths(paths1, read, write);
		imposeSync(res, paths1, read, write);
	}

	// Fixing the other direction (futureWrite -> read) for one edge case
	paths_t *paths2 = get_rf_sb_paths(write, read);
	FENCE_PRINT("From future write to read (edge case): \n");
	print_rf_sb_paths(paths2, write, read);
	imposeSync(candidates, paths2, write, read);

	// Append the candidates to the res list
	res->append(candidates);
	return res;
}

bool SCFence::addFixesNonSC(action_list_t *list) {
	bool added = false;
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		InferenceList *candidates = NULL;
		ModelAction	*act = *it;

		// Save the iterator of the read and the write
		action_list_t::iterator readIter = it, writeIter;
		if (act->get_seq_number() > 0) {
			// Annotated reads will be ignored
			if (scgen->getBadrfset()->contains(act) &&
				!scgen->getAnnotatedReadSet()->contains(act)) {
				const ModelAction *write = act->get_reads_from();
				// Check reading old or future value
				writeIter = std::find(list->begin(),
					list->end(), write);
				action_list_t::iterator findIt = std::find(list->begin(),
					readIter, write);
				bool readOldVal = findIt != readIter ? true : false;

				if (readOldVal) { // Pattern (a) read old value
					FENCE_PRINT("Running through pattern (a)!\n");
					candidates = getFixesFromPatternA(list, readIter, writeIter);
					// Add candidates pattern (a)
					
					added = addCandidates(candidates);
				} else { // Pattern (b) read future value
					// act->read, write->futureWrite
					FENCE_PRINT("Running through pattern (b)!\n");
					candidates = getFixesFromPatternB(list, readIter, writeIter);
					// Add candidates pattern (b)
					added = addCandidates(candidates);
				}
				// Just eliminate the first cycle we see in the execution
				break;
			}
		}
	}
	return added;
}


/** Return false to indicate there's no fixes for this execution. This can
 * happen for specific reason such as it's a user-specified assertion failure */
bool SCFence::addFixesBuggyExecution(action_list_t *list) {
	InferenceList *candidates = new InferenceList;
	bool foundFix = false;
	bool added = false;
	for (action_list_t::reverse_iterator rit = list->rbegin(); rit !=
		list->rend(); rit++) {
		ModelAction *uninitRead = *rit;
		if (uninitRead->get_seq_number() > 0) {
			if (!uninitRead->is_read() || 
				!uninitRead->get_reads_from()->is_uninitialized())
				continue;
			for (action_list_t::iterator it = list->begin(); it !=
				list->end(); it++) {
				ModelAction *write = *it;
				if (write->same_var(uninitRead)) {
					// Now we can try to impose sync write hb-> uninitRead
					paths_t *paths1 = get_rf_sb_paths(write, uninitRead);
					if (paths1->size() > 0) {
						FENCE_PRINT("Running through pattern (b') (unint read)!\n");
						print_rf_sb_paths(paths1, write, uninitRead);
						imposeSync(candidates, paths1, write, uninitRead);
						added = addCandidates(candidates);
						if (added) {
							foundFix = true;
							break;
						}
					}
				}
			}
		}
		if (foundFix)
			break;
	}

	return added;
}

/** Return false to indicate there's no implied mo for this execution. The idea
 * is that it counts the number of reads in the middle of write1 and write2, if
 * that number exceeds a specific number, then the analysis thinks that the
 * program is stuck in an infinite loop because write1 does not establish proper
 * synchronization with write2 such that the reads can read from write1 for
 * ever. */
bool SCFence::addFixesImplicitMO(action_list_t *list) {
	bool added = false;
	InferenceList *candidates = new InferenceList;
	for (action_list_t::reverse_iterator rit2 = list->rbegin(); rit2 !=
		list->rend(); rit2++) {
		ModelAction *write2 = *rit2;
		if (!write2->is_write())
			continue;
		action_list_t::reverse_iterator rit1 = rit2;
		rit1++;
		for (; rit1 != list->rend(); rit1++) {
			ModelAction *write1 = *rit1;
			if (!write1->is_write() || write1->get_location() !=
				write2->get_location())
				continue;
			int readCnt = 0;
			action_list_t::reverse_iterator ritRead = rit2;
			ritRead++;
			for (; ritRead != rit1; ritRead++) {
				ModelAction *read = *ritRead;
				if (!read->is_read() || read->get_location() !=
					write1->get_location())
					continue;
				readCnt++;
			}
			if (readCnt > getImplicitMOReadBound()) {
				// Found it, make write1 --hb-> write2
				bool isMO = execution->get_mo_graph()->checkReachable(write1, write2);
				if (isMO) // Only impose mo when it doesn't have mo impsed
					break;
				FENCE_PRINT("Running through pattern (c) -- implicit mo!\n");
				FENCE_PRINT("Read count between the two writes: %d\n", readCnt);
				FENCE_PRINT("implicitMOReadBound: %d\n",
					getImplicitMOReadBound());
				ACT_PRINT(write1);
				ACT_PRINT(write2);
				paths_t *paths1 = get_rf_sb_paths(write1, write2);
				if (paths1->size() > 0) {
					FENCE_PRINT("From write1 to write2: \n");
					print_rf_sb_paths(paths1, write1, write2);
					imposeSync(candidates, paths1, write1, write2);
					// Add the candidates as potential results
					added = addCandidates(candidates);
					if (added)
						return true;
				} else {
					FENCE_PRINT("Cannot establish hb between write1 & write2: \n");
					ACT_PRINT(write1);
					ACT_PRINT(write2);
				}
			}
			break;
		}
	}
	return false;
}

bool SCFence::checkDataRace(action_list_t *list, ModelAction **act1, 
	ModelAction **act2) {

	SnapList<action_list_t*> *opList = new SnapList<action_list_t*>;
	/** Collect the operations per location */
	for (action_list_t::iterator iter = list->begin(); iter != list->end();
		iter++) {
		ModelAction *act = *iter;
		if (act->get_original_mo() != memory_order_normal)
			continue;
		bool foundIt = false;
		for (SnapList<action_list_t*>::iterator listIter = opList->begin();
			listIter != opList->end(); listIter++) {
			action_list_t *list = *listIter;
			ModelAction *listAct = *(list->begin());
			if (listAct->get_location() != act->get_location())
				continue;
			foundIt = true;
			list->push_back(act);
		}
		if (!foundIt) {
			action_list_t *newList = new action_list_t;
			newList->push_back(act);
			opList->push_back(newList);
		}
	}

	if (opList->size() == 0)
		return false;
	/** Now check if any two operations (same loc) establish hb */
	for (SnapList<action_list_t*>::iterator listIter = opList->begin();
		listIter != opList->end(); listIter++) {
		action_list_t *list = *listIter;
		action_list_t::iterator it1, it2;
		for (it1 = list->begin(); it1 != list->end(); it1++) {
			ModelAction *raceAct1 = *it1;
			it2 = it1;
			it2++;
			for (; it2 != list->end(); it2++) {
				ModelAction *raceAct2 = *it2;
				if (!raceAct1->happens_before(raceAct2) &&
					!raceAct2->happens_before(raceAct1)) {
					*act1 = raceAct1;
					*act2 = raceAct2;
					return true;
				}
			}
		}
	}
	return false;
}

ModelAction* SCFence::sbPrevAction(ModelAction *act) {
	int idx = id_to_int(act->get_tid());
	// Retrieves the thread list of the action
	action_list_t *list = &(*scgen->getDupThreadLists())[idx];
	action_list_t::iterator iter = std::find(list->begin(),
		list->end(), act);
	return *--iter;
}

ModelAction* SCFence::sbNextAction(ModelAction *act) {
	int idx = id_to_int(act->get_tid());
	// Retrieves the thread list of the action
	action_list_t *list = &(*scgen->getDupThreadLists())[idx];
	action_list_t::iterator iter = std::find(list->begin(),
		list->end(), act);
	return *++iter;
}

bool SCFence::addFixesDataRace(action_list_t *list) {
	ModelAction *act1, *act2;
	bool hasRace = checkDataRace(list, &act1, &act2);
	if (hasRace) {
		InferenceList *candidates1 = new InferenceList,
			*candidates2 = new InferenceList;
		paths_t *paths1, *paths2;
		model_print("Fixing data race: \n");
		paths1 = get_rf_sb_paths(sbNextAction(act1), sbPrevAction(act2));
		paths2 = get_rf_sb_paths(sbNextAction(act2), sbPrevAction(act1));
		bool added = false;
		if (paths1->size() > 0) {
			model_print("paths1: \n");
			print_rf_sb_paths(paths1, act1, act2);
			imposeSync(candidates1, paths1, act1, act2);
			bool added = addCandidates(candidates1);
			if (paths2->size() > 0) {
				model_print("paths2: \n");
				print_rf_sb_paths(paths2, act2, act1);
				imposeSync(candidates2, paths2, act2, act1);
				added |= addCandidates(candidates2);
			}
		}
		return added;
	}
	return false;
}

bool SCFence::addFixes(action_list_t *list, fix_type_t type) {
	// First check whether this is a later weakened inference
	if (!getCurInference()->getShouldFix())
		return false;
	bool added = false;
	switch(type) {
		case BUGGY_EXECUTION:
			added = addFixesBuggyExecution(list);
			break;
		case IMPLICIT_MO:
			added = addFixesImplicitMO(list);
			break;
		case NON_SC:
			added = addFixesNonSC(list);
			break;
		case DATA_RACE:
			added = addFixesDataRace(list);
			break;
		default:
			break;
	}
	if (added && isBuggy()) {
		// If this is a buggy inference and we have got fixes for it, set the
		// falg
		setHasFixes(true);
	}
	return added;
}


bool SCFence::routineBacktrack(bool feasible) {
	model_print("Backtrack routine:\n");
	
	/******** commitCurInference ********/
	Inference *curInfer = getCurInference();
	commitInference(curInfer, feasible);
	if (feasible) {
		if (!isBuggy()) {
			model_print("Found one result!\n");
		} else if (!hasFixes()) { // Buggy and have no fixes
			model_print("Found one buggy candidate!\n");
		}
		curInfer->print();
		// Try to weaken this inference
		if (weaken && !isBuggy()) {
			getSet()->addWeakerInference(curInfer);
		}
		
	} else {
		if (curInfer->getShouldFix()) {
			model_print("Reach an infeasible inference!\n");
		} else {
			model_print("Get an unweakenable inference!\n");
			curInfer->print(true);
		}
	}

	/******** getNextInference ********/
	Inference *next = getNextInference();

	if (next) {
		/******** setCurInference ********/
		setCurInference(next);
		/******** restartModelChecker ********/
		restartModelChecker();
		return true;
	} else {
		// Finish exploring the whole process
		model_print("We are done with the whole process!\n");
		model_print("The results are as the following:\n");
		printResults();
		printCandidates();
				
		/******** exitModelChecker ********/
		exitModelChecker();

		return false;
	}
}

void SCFence::routineAfterAddFixes() {
	model_print("Add fixes routine begin:\n");
	
	/******** getNextInference ********/
	Inference *next = getNextInference();
	//ASSERT (next);

	if (next) {
		/******** setCurInference ********/
		setCurInference(next);
		/******** restartModelChecker ********/
		restartModelChecker();
		
		model_print("Add fixes routine end:\n");
		model_print("Restart checking with the follwing inference:\n");
		getCurInference()->print();
		model_print("Checking...\n");
	} else {
		routineBacktrack(false);
	}
}



/** This function finds all the paths that is a union of reads-from &
 * sequence-before relationship between act1 & act2. */
paths_t * SCFence::get_rf_sb_paths(const ModelAction *act1, const ModelAction *act2) {
	int idx1 = id_to_int(act1->get_tid()),
		idx2 = id_to_int(act2->get_tid());
	// Retrieves the two lists of actions of thread1 & thread2
	action_list_t *list1 = &(*dup_threadlists)[idx1],
		*list2 = &(*dup_threadlists)[idx2];
	if (list1->size() == 0 || list2->size() == 0) {
		return new paths_t();
	}

	// The container for all possible results
	paths_t *paths = new paths_t();
	// A stack that records all current possible paths
	paths_t *stack = new paths_t();
	path_t *path;
	// Initialize the stack with loads sb-ordered before act2
	for (action_list_t::iterator it2 = list2->begin(); it2 != list2->end(); it2++) {
		ModelAction *act = *it2;
		// Add read action not sb after the act2 (including act2)
		if (act->get_seq_number() > act2->get_seq_number())
			break;
		if (!act->is_read())
			continue;
		// Each start with a possible path
		path = new path_t();
		path->push_front(act);
		stack->push_back(path);
	}
	while (stack->size() > 0) {
		path = stack->back();
		stack->pop_back();
		// The latest read in the temporary path
		ModelAction *read = path->front();
		const ModelAction *write = read->get_reads_from();
		// If the read is uninitialized, don't do it
		if (write->get_seq_number() == 0) {
			delete path;
			continue;
		}
		/** In case of cyclic sbUrf, make sure the write appears in a new thread
		 * or in an existing thread that is sequence-before the added read
		 * actions
		 */
		bool loop = false;
		for (path_t::iterator p_it = path->begin(); p_it != path->end();
			p_it++) {
			ModelAction *addedRead = *p_it;
			if (id_to_int(write->get_tid()) == id_to_int(addedRead->get_tid())) {
				// In the same thread
				if (write->get_seq_number() >= addedRead->get_seq_number()) {
					// Have a loop
					delete path;
					loop = true;
					break;
				}
			}
		}
		// Not a useful rfUsb path (loop)
		if (loop) {
			continue;
		}

		unsigned write_seqnum = write->get_seq_number();
		// We then check if we got a valid path 
		if (id_to_int(write->get_tid()) == idx1 &&
			write_seqnum >= act1->get_seq_number()) {
			// Find a path
			paths->push_back(path);
			continue;
		}
		// Extend the path with the latest read
		int idx = id_to_int(write->get_tid());
		action_list_t *list = &(*dup_threadlists)[idx];
		for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
			ModelAction *act = *it;
			if (act->get_seq_number() > write_seqnum) // Could be RMW
				break;
			if (!act->is_read())
				continue;
			path_t *new_path = new path_t(*path);
			new_path->push_front(act);
			stack->push_back(new_path);
		}
		delete path;
	}
	return paths;
}

void SCFence::print_rf_sb_paths(paths_t *paths, const ModelAction *start, const ModelAction *end) {
	FENCE_PRINT("Starting from:\n");
	ACT_PRINT(start);
	FENCE_PRINT("\n");
	for (paths_t::iterator paths_i = paths->begin(); paths_i !=
		paths->end(); paths_i++) {
		path_t *path = *paths_i;
		FENCE_PRINT("Path %ld, size (%ld):\n", distance(paths->begin(), paths_i),
			path->size());
		path_t::iterator it = path->begin(), i_next;
		for (; it != path->end(); it++) {
			i_next = it;
			i_next++;
			const ModelAction *read = *it,
				*write = read->get_reads_from(),
				*next_read = (i_next != path->end()) ? *i_next : NULL;
			ACT_PRINT(write);
			if (next_read == NULL || next_read->get_reads_from() != read) {
				// Not the same RMW, also print the read operation
				ACT_PRINT(read);
			}
		}
		// Output a linebreak at the end of the path
		FENCE_PRINT("\n");
	}
	FENCE_PRINT("Ending with:\n");
	ACT_PRINT(end);
}

/******************** SCFence-related Functions (End) ********************/
