#include "inferlist.h"

InferenceList::InferenceList() {
	list = new ModelList<Inference*>;
}

int InferenceList::getSize() {
	return list->size();
}

void InferenceList::pop_back() {
	list->pop_back();
}

Inference* InferenceList::back() {
	return list->back();
}

void InferenceList::push_back(Inference *infer) {
	list->push_back(infer);
}

void InferenceList::pop_front() {
	list->pop_front();
}

/** We should not call this function too often because we want a nicer
 *  abstraction of the list of inferences. So far, it will only be called in
 *  the functions in InferenceSet */
ModelList<Inference*>* InferenceList::getList() {
	return list;
}

bool InferenceList::applyPatch(Inference *curInfer, Inference *newInfer, Patch *patch) {
	bool canUpdate = true,
		hasUpdated = false,
		updateState = false;
	for (int i = 0; i < patch->getSize(); i++) {
		canUpdate = true;
		hasUpdated = false;
		PatchUnit *unit = patch->get(i);
		newInfer->strengthen(unit->getAct(), unit->getMO(), canUpdate, hasUpdated);
		if (!canUpdate) {
			// This is not a feasible patch, bail
			break;
		} else if (hasUpdated) {
			updateState = true;
		}
	}
	if (updateState) {
		return true;
	} else {
		return false;
	}
}

void InferenceList::applyPatch(Inference *curInfer, Patch* patch) {
	if (list->empty()) {
		Inference *newInfer = new Inference(curInfer);
		if (!applyPatch(curInfer, newInfer, patch)) {
			delete newInfer;
		} else {
			list->push_back(newInfer);
		}
	} else {
		ModelList<Inference*> *newList = new ModelList<Inference*>;
		for (ModelList<Inference*>::iterator it = list->begin(); it !=
			list->end(); it++) {
			Inference *oldInfer = *it;
			Inference *newInfer = new Inference(oldInfer);
			if (!applyPatch(curInfer, newInfer, patch)) {
				delete newInfer;
			} else {
				newList->push_back(newInfer);
			}
		}
		// Clean the old list
		for (ModelList<Inference*>::iterator it = list->begin(); it !=
			list->end(); it++) {
			delete *it;
		}
		delete list;
		list = newList;
	}	
}

void InferenceList::applyPatch(Inference *curInfer, SnapVector<Patch*> *patches) {
	if (list->empty()) {
		for (unsigned i = 0; i < patches->size(); i++) {
			Inference *newInfer = new Inference(curInfer);
			Patch *patch = (*patches)[i];
			if (!applyPatch(curInfer, newInfer, patch)) {
				delete newInfer;
			} else {
				list->push_back(newInfer);
			}
		}
	} else {
		ModelList<Inference*> *newList = new ModelList<Inference*>;
		for (ModelList<Inference*>::iterator it = list->begin(); it !=
			list->end(); it++) {
			Inference *oldInfer = *it;
			for (unsigned i = 0; i < patches->size(); i++) {
				Inference *newInfer = new Inference(oldInfer);
				Patch *patch = (*patches)[i];
				if (!applyPatch(curInfer, newInfer, patch)) {
					delete newInfer;
				} else {
					newList->push_back(newInfer);
				}
			}
		}
		// Clean the old list
		for (ModelList<Inference*>::iterator it = list->begin(); it !=
			list->end(); it++) {
			delete *it;
		}
		delete list;
		list = newList;
	}
}

/** Append another list to this list */
bool InferenceList::append(InferenceList *inferList) {
	if (!inferList)
		return false;
	ModelList<Inference*> *l = inferList->list;
	list->insert(list->end(), l->begin(), l->end());
	return true;
}

/** Only choose the weakest existing candidates & they must be stronger than the
 * current inference */
void InferenceList::pruneCandidates(Inference *curInfer) {
	ModelList<Inference*> *newCandidates = new ModelList<Inference*>(),
		*candidates = list;

	ModelList<Inference*>::iterator it1, it2;
	int compVal;
	for (it1 = candidates->begin(); it1 != candidates->end(); it1++) {
		Inference *cand = *it1;
		compVal = cand->compareTo(curInfer);
		if (compVal == 0) {
			// If as strong as curInfer, bail
			delete cand;
			continue;
		}
		// Check if the cand is any stronger than those in the newCandidates
		for (it2 = newCandidates->begin(); it2 != newCandidates->end(); it2++) {
			Inference *addedInfer = *it2;
			compVal = addedInfer->compareTo(cand);
			if (compVal == 0 || compVal == 1) { // Added inference is stronger
				delete addedInfer;
				it2 = newCandidates->erase(it2);
				it2--;
			}
		}
		// Now push the cand to the list
		newCandidates->push_back(cand);
	}
	delete candidates;
	list = newCandidates;
}

void InferenceList::clearAll() {
	clearAll(list);
}

void InferenceList::clearList() {
	delete list;
}

void InferenceList::clearAll(ModelList<Inference*> *l) {
	for (ModelList<Inference*>::iterator it = l->begin(); it !=
		l->end(); it++) {
		Inference *infer = *it;
		delete infer;
	}
	delete l;
}

void InferenceList::clearAll(InferenceList *inferList) {
	clearAll(inferList->list);
}

void InferenceList::print(InferenceList *inferList, const char *msg) {
	print(inferList->getList(), msg);
}

void InferenceList::print(ModelList<Inference*> *inferList, const char *msg) {
	for (ModelList<Inference*>::iterator it = inferList->begin(); it !=
		inferList->end(); it++) {
		int idx = distance(inferList->begin(), it);
		Inference *infer = *it;
		model_print("%s %d:\n", msg, idx);
		infer->print();
		model_print("\n");
	}
}

void InferenceList::print() {
	print(list, "Inference");
}

void InferenceList::print(const char *msg) {
	print(list, msg);
}
