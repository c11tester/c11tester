#include "patch.h"
#include "inference.h"
#include "inferset.h"

InferenceSet::InferenceSet() {
	discoveredSet = new InferenceList;
	results = new InferenceList;
	candidates = new InferenceList;
}

/** Print the result of inferences  */
void InferenceSet::printResults() {
	results->print("Result");
	stat.print();
}

/** Print candidates of inferences */
void InferenceSet::printCandidates() {
	candidates->print("Candidate");
}

/** When we finish model checking or cannot further strenghen with the
 * inference, we commit the inference to be explored; if it is feasible, we
 * put it in the result list */
void InferenceSet::commitInference(Inference *infer, bool feasible) {
	ASSERT (infer);
	
	infer->setExplored(true);
	FENCE_PRINT("Explored %lu\n", infer->getHash());
	if (feasible) {
		addResult(infer);
	}
}

int InferenceSet::getCandidatesSize() {
	return candidates->getSize();
}

int InferenceSet::getResultsSize() {
	return results->getSize();
}

/** Be careful that if the candidate is not added, it will be deleted in this
 *  function. Therefore, caller of this function should just delete the list when
 *  finishing calling this function. */
bool InferenceSet::addCandidates(Inference *curInfer, InferenceList *inferList) {
	if (!inferList)
		return false;
	// First prune the list of inference
	inferList->pruneCandidates(curInfer);

	ModelList<Inference*> *cands = inferList->getList();

	// For the purpose of debugging, record all those inferences added here
	InferenceList *addedCandidates = new InferenceList;
	FENCE_PRINT("List size: %ld.\n", cands->size());
	bool added = false;

	/******** addCurInference ********/
	// Add the current inference to the set, but specifially it marks it as
	// non-leaf node so that when it gets popped, we just need to commit it as
	// explored
	addCurInference(curInfer);

	ModelList<Inference*>::iterator it;
	for (it = cands->begin(); it != cands->end(); it++) {
		Inference *candidate = *it;
		// Before adding those fixes, set its initial inference
		candidate->setInitialInfer(curInfer->getInitialInfer());
		bool tmpAdded = false;
		/******** addInference ********/
		tmpAdded = addInference(candidate);
		if (tmpAdded) {
			added = true;
			it = cands->erase(it);
			it--;
			addedCandidates->push_back(candidate); 
		}
	}

	// For debugging, print the list of candidates for this iteration
	FENCE_PRINT("Current inference:\n");
	curInfer->print();
	FENCE_PRINT("\n");
	FENCE_PRINT("The added inferences:\n");
	addedCandidates->print("Candidates");
	FENCE_PRINT("\n");
	
	// Clean up the candidates
	inferList->clearList();
	FENCE_PRINT("potential results size: %d.\n", candidates->getSize());
	return added;
}


/** Check if we have stronger or equal inferences in the current result
 * list; if we do, we remove them and add the passed-in parameter infer */
 void InferenceSet::addResult(Inference *infer) {
	ModelList<Inference*> *list = results->getList();
	for (ModelList<Inference*>::iterator it = list->begin(); it !=
		list->end(); it++) {
		Inference *existResult = *it;
		int compVal = existResult->compareTo(infer);
		if (compVal == 0 || compVal == 1) {
			// The existing result is equal or stronger, remove it
			FENCE_PRINT("We are dumping the follwing inference because it's either too weak or the same:\n");
			existResult->print();
			FENCE_PRINT("\n");
			it = list->erase(it);
			it--;
		}
	}
	list->push_back(infer);
 }

/** Get the next available unexplored node; @Return NULL 
 * if we don't have next, meaning that we are done with exploring */
Inference* InferenceSet::getNextInference() {
	Inference *infer = NULL;
	while (candidates->getSize() > 0) {
		infer = candidates->back();
		candidates->pop_back();
		if (!infer->isLeaf()) {
			commitInference(infer, false);
			continue;
		}
		if (infer->getShouldFix() && hasBeenExplored(infer)) {
			// Finish exploring this node
			// Remove the node from the set 
			FENCE_PRINT("Explored inference:\n");
			infer->print();
			FENCE_PRINT("\n");
			continue;
		} else {
			return infer;
		}
	}
	return NULL;
}

/** Add the current inference to the set before adding fixes to it; in
 * this case, fixes will be added afterwards, and infer should've been
 * discovered */
void InferenceSet::addCurInference(Inference *infer) {
	infer->setLeaf(false);
	candidates->push_back(infer);
}

/** Add one weaker node (the stronger one has been explored and known to be SC,
 *  we just want to know if a weaker one might also be SC).
 */
void InferenceSet::addWeakerInference(Inference *curInfer) {
	Inference *initialInfer = curInfer->getInitialInfer();
	model_print("Before adding weaker inferece, candidates size=%d\n",
		candidates->getSize());
	ModelList<Inference*> *list = discoveredSet->getList();

	// An array of strengthened wildcards
	SnapVector<int> *strengthened = new SnapVector<int>;
	model_print("Strengthened wildcards\n");
	for (int i = 1; i <= curInfer->getSize(); i++) {
		memory_order mo1 = (*curInfer)[i],
			mo2 = (*initialInfer)[i];
		int compVal = Inference::compareMemoryOrder(mo1, mo2);
		if (!(compVal == 0 || compVal == 1)) {
			model_print("assert failure\n");
			model_print("compVal=%d\n", compVal);
			ASSERT (false);
		}
		if (compVal == 0) // Same
			continue;
		model_print("wildcard %d -> %s (%s)\n", i, get_mo_str(mo1),
			get_mo_str(mo2));
		strengthened->push_back(i);
	}

	for (unsigned i = 0; i < strengthened->size(); i++) {
		int w = (*strengthened)[i]; // The wildcard
		memory_order mo1 = (*curInfer)[w];
		memory_order mo2 = (*initialInfer)[w];
		memory_order weakerMO = Inference::nextWeakOrder(mo1, mo2);
		Inference *weakerInfer1 = new Inference(curInfer);
		Inference *weakerInfer2 = NULL;
		if (mo1 == memory_order_acq_rel) {
			if (mo2 == memory_order_acquire) {
				(*weakerInfer1)[w] = memory_order_acquire;
			} else if (mo2 == memory_order_release) {
				(*weakerInfer1)[w] = memory_order_release;
			} else { // relaxed
				(*weakerInfer1)[w] = memory_order_acquire;
				weakerInfer2 = new Inference(curInfer);
				(*weakerInfer2)[w] = memory_order_release;
			}
		} else {
			if (mo2 != weakerMO)
				(*weakerInfer1)[w] = weakerMO;
		}

		weakerInfer1->setShouldFix(false);
		weakerInfer1->setLeaf(true);
		if (weakerInfer2) {
			weakerInfer2->setShouldFix(false);
			weakerInfer2->setLeaf(true);
		}
		
		bool foundIt = false;
		for (ModelList<Inference*>::iterator it = list->begin(); it !=
			list->end(); it++) {
			Inference *discoveredInfer = *it;
			// When we already have an equal inferences in the candidates list
			int compVal = discoveredInfer->compareTo(weakerInfer1);
			if (compVal == 0 && !discoveredInfer->isLeaf()) {
				foundIt = true;
				break;
			}
		}
		if (!foundIt) {
			addInference(weakerInfer1);
		}
		if (!weakerInfer2)
			continue;
		foundIt = false;
		for (ModelList<Inference*>::iterator it = list->begin(); it !=
			list->end(); it++) {
			Inference *discoveredInfer = *it;
			// When we already have an equal inferences in the candidates list
			int compVal = discoveredInfer->compareTo(weakerInfer2);
			if (compVal == 0 && !discoveredInfer->isLeaf()) {
				foundIt = true;
				break;
			}
		}
		if (!foundIt) {
			addInference(weakerInfer2);
		}
	}

	model_print("After adding weaker inferece, candidates size=%d\n",
		candidates->getSize());
}

/** Add one possible node that represents a fix for the current inference;
 * @Return true if the node to add has not been explored yet
 */
bool InferenceSet::addInference(Inference *infer) {
	if (!hasBeenDiscovered(infer)) {
		// We haven't discovered this inference yet

		// Newly added nodes are leaf by default
		infer->setLeaf(true);
		candidates->push_back(infer);
		discoveredSet->push_back(infer);
		FENCE_PRINT("Discovered a parameter assignment with hashcode %lu\n", infer->getHash());
		return true;
	} else {
		stat.notAddedAtFirstPlace++;
		return false;
	}
}

/** Return false if we haven't discovered that inference yet. Basically we
 * search the candidates list */
bool InferenceSet::hasBeenDiscovered(Inference *infer) {
	ModelList<Inference*> *list = discoveredSet->getList();
	for (ModelList<Inference*>::iterator it = list->begin(); it !=
		list->end(); it++) {
		Inference *discoveredInfer = *it;
		// When we already have an equal inferences in the candidates list
		int compVal = discoveredInfer->compareTo(infer);
		if (compVal == 0) {
			FENCE_PRINT("%lu has beend discovered.\n",
				infer->getHash());
			return true;
		}
		// Or the discoveredInfer is explored and infer is strong than it is
		if (compVal == -1 && discoveredInfer->isLeaf() &&
			discoveredInfer->isExplored()) {
			return true;
		}
	}
	return false;
}

/** Return true if we have explored this inference yet. Basically we
 * search the candidates list */
bool InferenceSet::hasBeenExplored(Inference *infer) {
	ModelList<Inference*> *list = discoveredSet->getList();
	for (ModelList<Inference*>::iterator it = list->begin(); it !=
		list->end(); it++) {
		Inference *discoveredInfer = *it;
		if (!discoveredInfer->isExplored())
			continue;
		// When we already have an equal inferences in the candidates list
		int compVal = discoveredInfer->compareTo(infer);
		if (compVal == 0 || compVal == -1) {
			return true;
		}
	}
	return false;
}
