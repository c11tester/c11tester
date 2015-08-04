#ifndef _INFERLIST_H
#define _INFERLIST_H

#include "fence_common.h"
#include "patch.h"
#include "inference.h"

class Patch;
class PatchUnit;
class Inference;

/** This class represents that the list of inferences that can fix the problem
 */
class InferenceList {
	private:
	ModelList<Inference*> *list;

	public:
	InferenceList();
	int getSize();	
	Inference* back();

	/** We should not call this function too often because we want a nicer
	 *  abstraction of the list of inferences. So far, it will only be called in
	 *  the functions in InferenceSet */
	ModelList<Inference*>* getList();	
	void push_back(Inference *infer);
	void pop_front();
	void pop_back();
	bool applyPatch(Inference *curInfer, Inference *newInfer, Patch *patch);

	void applyPatch(Inference *curInfer, Patch* patch);

	void applyPatch(Inference *curInfer, SnapVector<Patch*> *patches);
	
	/** Append another list to this list */
	bool append(InferenceList *inferList);

	/** Only choose the weakest existing candidates & they must be stronger than the
	 * current inference */
	void pruneCandidates(Inference *curInfer);
	
	void clearAll();

	void clearList();

	static void clearAll(ModelList<Inference*> *l);

	static void clearAll(InferenceList *inferList);
	
	static void print(ModelList<Inference*> *inferList, const char *msg);

	static void print(InferenceList *inferList, const char *msg);

	void print();

	void print(const char *msg);

	MEMALLOC

};


#endif
