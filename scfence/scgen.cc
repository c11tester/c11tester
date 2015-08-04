#include "scgen.h"
	
SCGenerator::SCGenerator() :
	execution(NULL),
	actions(NULL),
	cvmap(),
	cyclic(false),
	badrfset(),
	lastwrmap(),
	threadlists(1),
	dup_threadlists(1),
	print_always(false),
	print_buggy(false),
	print_nonsc(false),
	stats(new struct sc_statistics),
	annotationMode(false) {
}

SCGenerator::~SCGenerator() {
}

bool SCGenerator::getCyclic() {
	return cyclic || hasBadRF;
}

SnapVector<action_list_t>* SCGenerator::getDupThreadLists() {
	return &dup_threadlists;
}

struct sc_statistics* SCGenerator::getStats() {
	return stats;
}

void SCGenerator::setExecution(ModelExecution *execution) {
	this->execution = execution;
}

void SCGenerator::setActions(action_list_t *actions) {
	this->actions = actions;
}

void SCGenerator::setPrintAlways(bool val) {
	this->print_always = val;
}

bool SCGenerator::getPrintAlways() {
	return this->print_always;
}

bool SCGenerator::getHasBadRF() {
	return this->hasBadRF;
}

void SCGenerator::setPrintBuggy(bool val) {
	this->print_buggy = val;
}

void SCGenerator::setPrintNonSC(bool val) {
	this->print_nonsc = val;
}

void SCGenerator::setAnnotationMode(bool val) {
	this->annotationMode = val;
}

action_list_t * SCGenerator::getSCList() {
	struct timeval start;
	struct timeval finish;
	gettimeofday(&start, NULL);
	
	/* Build up the thread lists for general purpose */
	int thrdNum;
	buildVectors(&dup_threadlists, &thrdNum, actions);
	
	fastVersion = true;
	action_list_t *list = generateSC(actions);
	if (cyclic) {
		reset(actions);
		delete list;
		fastVersion = false;
		list = generateSC(actions);
	}
	check_rf(list);
	gettimeofday(&finish, NULL);
	stats->elapsedtime+=((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
	update_stats();
	return list;
}

HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4> * SCGenerator::getBadrfset() {
	return &badrfset;
}

HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4 > * SCGenerator::getAnnotatedReadSet() {
	return &annotatedReadSet;
}

void SCGenerator::print_list(action_list_t *list) {
	model_print("---------------------------------------------------------------------\n");
	if (cyclic || hasBadRF)
		model_print("Not SC\n");
	unsigned int hash = 0;

	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->get_seq_number() > 0) {
			if (badrfset.contains(act))
				model_print("BRF ");
			act->print();
			if (badrfset.contains(act)) {
				model_print("Desired Rf: %u \n", badrfset.get(act)->get_seq_number());
			}
		}
		hash = hash ^ (hash << 3) ^ ((*it)->hash());
	}
	model_print("HASH %u\n", hash);
	model_print("---------------------------------------------------------------------\n");
}


action_list_t * SCGenerator::generateSC(action_list_t *list) {
	int numactions=buildVectors(&threadlists, &maxthreads, list);
	stats->actions+=numactions;

	// Analyze which actions we should ignore in the partially SC analysis
	if (annotationMode) {
		collectAnnotatedReads();
		if (annotationError) {
			model_print("Annotation error!\n");
			return NULL;
		}
	}

	computeCV(list);

	action_list_t *sclist = new action_list_t();
	ModelAction **array = (ModelAction **)model_calloc(1, (maxthreads + 1) * sizeof(ModelAction *));
	int * choices = (int *) model_calloc(1, sizeof(int)*numactions);
	int endchoice = 0;
	int currchoice = 0;
	int lastchoice = -1;
	while (true) {
		int numActions = getNextActions(array);
		if (numActions == 0)
			break;
		ModelAction * act=pruneArray(array, numActions);
		if (act == NULL) {
			if (currchoice < endchoice) {
				act = array[choices[currchoice]];
				//check whether there is still another option
				if ((choices[currchoice]+1)<numActions)
					lastchoice=currchoice;
				currchoice++;
			} else {
				act = array[0];
				choices[currchoice]=0;
				if (numActions>1)
					lastchoice=currchoice;
				currchoice++;
			}
		}
		thread_id_t tid = act->get_tid();
		//remove action
		threadlists[id_to_int(tid)].pop_front();
		//add ordering constraints from this choice
		if (updateConstraints(act)) {
			//propagate changes if we have them
			bool prevc=cyclic;
			computeCV(list);
			if (!prevc && cyclic) {
				model_print("ROLLBACK in SC\n");
				//check whether we have another choice
				if (lastchoice != -1) {
					//have to reset everything
					choices[lastchoice]++;
					endchoice=lastchoice+1;
					currchoice=0;
					lastchoice=-1;

					reset(list);
					buildVectors(&threadlists, &maxthreads, list);
					computeCV(list);
					sclist->clear();
					continue;

				}
			}
		}
		//add action to end
		sclist->push_back(act);
	}
	model_free(array);
	return sclist;
}

void SCGenerator::update_stats() {
	if (cyclic) {
		stats->nonsccount++;
	} else {
		stats->sccount++;
	}
}

int SCGenerator::buildVectors(SnapVector<action_list_t> *threadlist, int *maxthread,
	action_list_t *list) {
	*maxthread = 0;
	int numactions = 0;
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		ModelAction *act = *it;
		numactions++;
		int threadid = id_to_int(act->get_tid());
		if (threadid > *maxthread) {
			threadlist->resize(threadid + 1);
			*maxthread = threadid;
		}
		(*threadlist)[threadid].push_back(act);
	}
	return numactions;
}


bool SCGenerator::updateConstraints(ModelAction *act) {
	bool changed = false;
	for (int i = 0; i <= maxthreads; i++) {
		thread_id_t tid = int_to_id(i);
		if (tid == act->get_tid())
			continue;

		action_list_t *list = &threadlists[id_to_int(tid)];
		for (action_list_t::iterator rit = list->begin(); rit != list->end(); rit++) {
			ModelAction *write = *rit;
			if (!write->is_write())
				continue;
			ClockVector *writecv = cvmap.get(write);
			if (writecv->synchronized_since(act))
				break;
			if (write->get_location() == act->get_location()) {
				//write is sc after act
				merge(writecv, write, act);
				changed = true;
				break;
			}
		}
	}
	return changed;
}

void SCGenerator::computeCV(action_list_t *list) {
	bool changed = true;
	bool firsttime = true;
	ModelAction **last_act = (ModelAction **)model_calloc(1, (maxthreads + 1) * sizeof(ModelAction *));

	while (changed) {
		changed = changed&firsttime;
		firsttime = false;
		bool updateFuture = false;

		for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
			ModelAction *act = *it;
			ModelAction *lastact = last_act[id_to_int(act->get_tid())];
			if (act->is_thread_start())
				lastact = execution->get_thread(act)->get_creation();
			last_act[id_to_int(act->get_tid())] = act;
			ClockVector *cv = cvmap.get(act);
			if (cv == NULL) {
				cv = new ClockVector(act->get_cv(), act);
				cvmap.put(act, cv);
			}
			
			if (lastact != NULL) {
				merge(cv, act, lastact);
			}
			if (act->is_thread_join()) {
				Thread *joinedthr = act->get_thread_operand();
				ModelAction *finish = execution->get_last_action(joinedthr->get_id());
				changed |= merge(cv, act, finish);
			}
			if (act->is_read()) {
				if (fastVersion) {
					changed |= processReadFast(act, cv);
				} else if (annotatedReadSet.contains(act)) {
					changed |= processAnnotatedReadSlow(act, cv, &updateFuture);
				} else {
					changed |= processReadSlow(act, cv, &updateFuture);
				}
			}
		}
		/* Reset the last action array */
		if (changed) {
			bzero(last_act, (maxthreads + 1) * sizeof(ModelAction *));
		} else {
			if (!fastVersion) {
				if (!allowNonSC) {
					allowNonSC = true;
					changed = true;
				} else {
					break;
				}
			}
		}
	}
	model_free(last_act);
}

bool SCGenerator::processReadFast(ModelAction *read, ClockVector *cv) {
	bool changed = false;

	/* Merge in the clock vector from the write */
	const ModelAction *write = read->get_reads_from();
	if (!write) { // The case where the write is a promise
		return false;
	}
	ClockVector *writecv = cvmap.get(write);
	changed |= merge(cv, read, write) && (*read < *write);

	for (int i = 0; i <= maxthreads; i++) {
		thread_id_t tid = int_to_id(i);
		action_list_t *list = execution->get_actions_on_obj(read->get_location(), tid);
		if (list == NULL)
			continue;
		for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *write2 = *rit;
			if (!write2->is_write())
				continue;
			if (write2 == write)
				continue;
			if (write2 == read) // If read is a RMW
				continue;

			ClockVector *write2cv = cvmap.get(write2);
			if (write2cv == NULL)
				continue;
			/* write -sc-> write2 &&
				 write -rf-> R =>
				 R -sc-> write2 */
			if (write2cv->synchronized_since(write)) {
				changed |= merge(write2cv, write2, read);

			}

			//looking for earliest write2 in iteration to satisfy this
			/* write2 -sc-> R &&
				 write -rf-> R =>
				 write2 -sc-> write */
			if (cv->synchronized_since(write2)) {
				changed |= writecv == NULL || merge(writecv, write, write2);
				break;
			}
		}
	}
	return changed;
}

bool SCGenerator::processReadSlow(ModelAction *read, ClockVector *cv, bool *updateFuture) {
	bool changed = false;
	
	/* Merge in the clock vector from the write */
	const ModelAction *write = read->get_reads_from();
	ClockVector *writecv = cvmap.get(write);
	if ((*write < *read) || ! *updateFuture) {
		bool status = merge(cv, read, write) && (*read < *write);
		changed |= status;
		*updateFuture = status;
	}

	for (int i = 0; i <= maxthreads; i++) {
		thread_id_t tid = int_to_id(i);
		action_list_t *list = execution->get_actions_on_obj(read->get_location(), tid);
		if (list == NULL)
			continue;
		for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *write2 = *rit;
			if (!write2->is_write())
				continue;
			if (write2 == write)
				continue;
			if (write2 == read) // If read is a RMW
				continue;

			ClockVector *write2cv = cvmap.get(write2);
			if (write2cv == NULL)
				continue;

			/* write -sc-> write2 &&
				 write -rf-> R =>
				 R -sc-> write2 */
			if (write2cv->synchronized_since(write)) {
				if ((*read < *write2) || ! *updateFuture) {
					bool status = merge(write2cv, write2, read);
					changed |= status;
					*updateFuture |= status && (*write2 < *read);
				}
			}

			//looking for earliest write2 in iteration to satisfy this
			/* write2 -sc-> R &&
				 write -rf-> R =>
				 write2 -sc-> write */
			if (cv->synchronized_since(write2)) {
				if ((*write2 < *write) || ! *updateFuture) {
					bool status = writecv == NULL || merge(writecv, write, write2);
					changed |= status;
					*updateFuture |= status && (*write < *write2);
				}
				break;
			}
		}
	}
	return changed;
}

bool SCGenerator::processAnnotatedReadSlow(ModelAction *read, ClockVector *cv, bool *updateFuture) {
	bool changed = false;
	
	/* Merge in the clock vector from the write */
	const ModelAction *write = read->get_reads_from();
	if ((*write < *read) || ! *updateFuture) {
		bool status = merge(cv, read, write) && (*read < *write);
		changed |= status;
		*updateFuture = status;
	}
	return changed;
}

int SCGenerator::getNextActions(ModelAction **array) {
	int count=0;

	for (int t = 0; t <= maxthreads; t++) {
		action_list_t *tlt = &threadlists[t];
		if (tlt->empty())
			continue;
		ModelAction *act = tlt->front();
		ClockVector *cv = cvmap.get(act);
		
		/* Find the earliest in SC ordering */
		for (int i = 0; i <= maxthreads; i++) {
			if ( i == t )
				continue;
			action_list_t *threadlist = &threadlists[i];
			if (threadlist->empty())
				continue;
			ModelAction *first = threadlist->front();
			if (cv->synchronized_since(first)) {
				act = NULL;
				break;
			}
		}
		if (act != NULL) {
			array[count++]=act;
		}
	}
	if (count != 0)
		return count;
	for (int t = 0; t <= maxthreads; t++) {
		action_list_t *tlt = &threadlists[t];
		if (tlt->empty())
			continue;
		ModelAction *act = tlt->front();
		ClockVector *cv = act->get_cv();
		
		/* Find the earliest in SC ordering */
		for (int i = 0; i <= maxthreads; i++) {
			if ( i == t )
				continue;
			action_list_t *threadlist = &threadlists[i];
			if (threadlist->empty())
				continue;
			ModelAction *first = threadlist->front();
			if (cv->synchronized_since(first)) {
				act = NULL;
				break;
			}
		}
		if (act != NULL) {
			array[count++]=act;
		}
	}

	ASSERT(count==0 || cyclic);

	return count;
}

bool SCGenerator::merge(ClockVector *cv, const ModelAction *act, const ModelAction *act2) {
	ClockVector *cv2 = cvmap.get(act2);
	if (cv2 == NULL)
		return true;

	if (cv2->getClock(act->get_tid()) >= act->get_seq_number() && act->get_seq_number() != 0) {
		cyclic = true;
		//refuse to introduce cycles into clock vectors
		return false;
	}
	if (fastVersion) {
		bool status = cv->merge(cv2);
		return status;
	} else {
		bool merged;
		if (allowNonSC) {
			merged = cv->merge(cv2);
			if (merged)
				allowNonSC = false;
			return merged;
		} else {
			if (act2->happens_before(act) ||
				(act->is_seqcst() && act2->is_seqcst() && *act2 < *act)) {
				return cv->merge(cv2);
			} else {
				return false;
			}
		}
	}

}

void SCGenerator::check_rf1(action_list_t *list) {
	bool hasBadRF1 = false;
	HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4 > badrfset1;
	HashTable<void *, const ModelAction *, uintptr_t, 4 > lastwrmap1;
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->is_read()) {
			if (act->get_reads_from() != lastwrmap1.get(act->get_location())) {
				badrfset1.put(act, lastwrmap1.get(act->get_location()));
				hasBadRF1 = true;
			}
		}
		if (act->is_write())
			lastwrmap1.put(act->get_location(), act);
	}
	if (cyclic != hasBadRF1 && !annotationMode) {
		if (cyclic)
			model_print("Assert failure & non-SC\n");
		else
			model_print("Assert failure & SC\n");
		if (fastVersion) {
			model_print("Fast\n");
		} else {
			model_print("Slow\n");
		}
		print_list(list);
	}
	if (!annotationMode) {
		ASSERT (cyclic == hasBadRF1);
	}
}

void SCGenerator::check_rf(action_list_t *list) {
	hasBadRF = false;
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->is_read()) {
			const ModelAction *write = act->get_reads_from();
			if (write && write != lastwrmap.get(act->get_location())) {
				badrfset.put(act, lastwrmap.get(act->get_location()));
				hasBadRF = true;
			}
		}
		if (act->is_write())
			lastwrmap.put(act->get_location(), act);
	}
	if (cyclic != hasBadRF && !annotationMode) {
		if (cyclic)
			model_print("Assert failure & non-SC\n");
		else
			model_print("Assert failure & SC\n");
		if (fastVersion) {
			model_print("Fast\n");
		} else {
			model_print("Slow\n");
		}
		print_list(list);
	}
	if (!annotationMode) {
		ASSERT (cyclic == hasBadRF);
	}
}

void SCGenerator::reset(action_list_t *list) {
	for (int t = 0; t <= maxthreads; t++) {
		action_list_t *tlt = &threadlists[t];
		tlt->clear();
	}
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		ModelAction *act = *it;
		delete cvmap.get(act);
		cvmap.put(act, NULL);
	}

	cyclic=false;	
}

ModelAction* SCGenerator::pruneArray(ModelAction **array, int count) {
	/* No choice */
	if (count == 1)
		return array[0];

	/* Choose first non-write action */
	ModelAction *nonwrite=NULL;
	for(int i=0;i<count;i++) {
		if (!array[i]->is_write())
			if (nonwrite==NULL || nonwrite->get_seq_number() > array[i]->get_seq_number())
				nonwrite = array[i];
	}
	if (nonwrite != NULL)
		return nonwrite;
	
	/* Look for non-conflicting action */
	ModelAction *nonconflict=NULL;
	for(int a=0;a<count;a++) {
		ModelAction *act=array[a];
		for (int i = 0; i <= maxthreads && act != NULL; i++) {
			thread_id_t tid = int_to_id(i);
			if (tid == act->get_tid())
				continue;
			
			action_list_t *list = &threadlists[id_to_int(tid)];
			for (action_list_t::iterator rit = list->begin(); rit != list->end(); rit++) {
				ModelAction *write = *rit;
				if (!write->is_write())
					continue;
				ClockVector *writecv = cvmap.get(write);
				if (writecv->synchronized_since(act))
					break;
				if (write->get_location() == act->get_location()) {
					//write is sc after act
					act = NULL;
					break;
				}
			}
		}
		if (act != NULL) {
			if (nonconflict == NULL || nonconflict->get_seq_number() > act->get_seq_number())
				nonconflict=act;
		}
	}
	return nonconflict;
}

/** This routine is operated based on the built threadlists */
void SCGenerator::collectAnnotatedReads() {
	for (unsigned i = 1; i < threadlists.size(); i++) {
		action_list_t *list = &threadlists.at(i);
		for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
			ModelAction *act = *it;
			if (!IS_SC_ANNO(act))
				continue;
			if (!IS_ANNO_BEGIN(act)) {
				model_print("SC annotation should begin with a BEGIN annotation\n");
				annotationError = true;
				return;
			}
			act = *++it;
			while (!IS_ANNO_END(act) && it != list->end()) {
				// Look for the actions to keep in this loop
				ModelAction *nextAct = *++it;
				if (!IS_ANNO_KEEP(nextAct)) { // Annotated reads
					act->print();
					annotatedReadSet.put(act, act);
					annotatedReadSetSize++;
					if (IS_ANNO_END(nextAct))
						break;
				}
			}
			if (it == list->end()) {
				model_print("SC annotation should end with a END annotation\n");
				annotationError = true;
				return;
			}
		}
	}
}
