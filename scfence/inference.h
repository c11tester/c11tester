#ifndef _INFERENCE_H
#define _INFERENCE_H

#include "fence_common.h"
#include "wildcard.h"
#include "patch.h"
#include "inferlist.h"

class InferenceList;
class Inference;

extern const char* get_mo_str(memory_order order);
extern bool isTheInference(Inference *infer);

class Inference {
	private:
	memory_order *orders;
	int size;

	/** It's initial inference, if not assigned, set it as itself */
	Inference *initialInfer;

	/** Whether this inference will lead to a buggy execution */
	bool buggy;

	/** Whether this inference has been explored */
	bool explored;

	/** Whether this inference is the leaf node in the inference lattice, see
	 * inferset.h for more details */
	bool leaf;

	/** When this inference will have buggy executions, this indicates whether
	 * it has any fixes. */
	bool hasFixes;

	/** When we have a strong enough inference, we also want to weaken specific
	 *  parameters to see if it is possible to be weakened. So we will this
	 *  field to mark if we should fix the inference or not if we get non-SC
	 *  behaviros. By default true. */
	bool shouldFix;

	void resize(int newsize);
	
	/** Return the state of how we update a specific mo; If we have to make an
	 * uncompatible inference or that inference cannot be imposed because it's
	 * not a wildcard, it returns -1; if it is a compatible memory order but the
	 * current memory order is no weaker than mo, it returns 0; otherwise, it
	 * does strengthen the order, and returns 1 */
	int strengthen(const ModelAction *act, memory_order mo);

	public:
	Inference();

	Inference(Inference *infer);
	
	/** return value:
	  * 0 -> mo1 == mo2;
	  * 1 -> mo1 stronger than mo2;
	  * -1 -> mo1 weaker than mo2;
	  * 2 -> mo1 & mo2 are uncomparable.
	 */
	static int compareMemoryOrder(memory_order mo1, memory_order mo2);


	/** Try to calculate the set of inferences that are weaker than this, but
	 *  still stronger than infer */
	InferenceList* getWeakerInferences(Inference *infer);

	static memory_order nextWeakOrder(memory_order mo1, memory_order mo2);

	void getWeakerInferences(InferenceList* list, Inference *tmpRes, Inference *infer1,
		Inference *infer2, SnapVector<int> *strengthened, unsigned idx);

	int getSize() {
		return size;
	}

	memory_order &operator[](int idx);
	
	/** A simple overload, which allows caller to pass two boolean refrence, and
	 * we will set those two variables indicating whether we can update the
	 * order (copatible or not) and have updated the order */
	int strengthen(const ModelAction *act, memory_order mo, bool &canUpdate,
		bool &hasUpdated);

	/** @Return:
		1 -> 'this> infer';
		-1 -> 'this < infer'
		0 -> 'this == infer'
		INFERENCE_INCOMPARABLE(x) -> true means incomparable
	 */
	int compareTo(const Inference *infer) const;

	void setInitialInfer(Inference *val) {
		initialInfer = val;
	}

	Inference* getInitialInfer() {
		return initialInfer;
	}

	void setShouldFix(bool val) {
		shouldFix = val;
	}

	bool getShouldFix() {
		return shouldFix;
	}

	void setHasFixes(bool val) {
		hasFixes = val;
	}

	bool getHasFixes() {
		return hasFixes;
	}

	void setBuggy(bool val) {
		buggy = val;
	}

	bool getBuggy() {
		return buggy;
	}
	
	void setExplored(bool val) {
		explored = val;
	}

	bool isExplored() {
		return explored;
	}

	void setLeaf(bool val) {
		leaf = val;
	}

	bool isLeaf() {
		return leaf;
	}

	unsigned long getHash();
	
	void print();
	void print(bool hasHash);

	~Inference() {
		model_free(orders);
	}

	MEMALLOC
};
#endif
