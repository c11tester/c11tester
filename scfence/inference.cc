#include "fence_common.h"
#include "wildcard.h"
#include "patch.h"
#include "inferlist.h"
#include "inference.h"

/** Forward declaration */
class PatchUnit;
class Patch;
class Inference;
class InferenceList;

bool isTheInference(Inference *infer) {
	for (int i = 0; i < infer->getSize(); i++) {
		memory_order mo1 = (*infer)[i], mo2;
		if (mo1 == WILDCARD_NONEXIST)
			mo1 = relaxed;
		switch (i) {
			case 3:
				mo2 = acquire;
			break;
			case 11:
				mo2 = release;
			break;
			default:
				mo2 = relaxed;
			break;
		}
		if (mo1 != mo2)
			return false;
	}
	return true;
}

const char* get_mo_str(memory_order order) {
	switch (order) {
		case std::memory_order_relaxed: return "relaxed";
		case std::memory_order_acquire: return "acquire";
		case std::memory_order_release: return "release";
		case std::memory_order_acq_rel: return "acq_rel";
		case std::memory_order_seq_cst: return "seq_cst";
		default: 
			//model_print("Weird memory order, a bug or something!\n");
			//model_print("memory_order: %d\n", order);
			return "unknown";
	}
}


void Inference::resize(int newsize) {
	ASSERT (newsize > size && newsize <= MAX_WILDCARD_NUM);
	memory_order *newOrders = (memory_order *) model_malloc((newsize + 1) * sizeof(memory_order*));
	int i;
	for (i = 0; i <= size; i++)
		newOrders[i] = orders[i];
	for (; i <= newsize; i++)
		newOrders[i] = WILDCARD_NONEXIST;
	model_free(orders);
	size = newsize;
	orders = newOrders;
}

/** Return the state of how we update a specific mo; If we have to make an
 * uncompatible inference or that inference cannot be imposed because it's
 * not a wildcard, it returns -1; if it is a compatible memory order but the
 * current memory order is no weaker than mo, it returns 0; otherwise, it
 * does strengthen the order, and returns 1 */
int Inference::strengthen(const ModelAction *act, memory_order mo) {
	memory_order wildcard = act->get_original_mo();
	int wildcardID = get_wildcard_id_zero(wildcard);
	if (!is_wildcard(wildcard)) {
		FENCE_PRINT("We cannot make this update to %s!\n", get_mo_str(mo));
		ACT_PRINT(act);
		return -1;
	}
	if (wildcardID > size)
		resize(wildcardID);
	ASSERT (is_normal_mo(mo));
	//model_print("wildcardID -> order: %d -> %d\n", wildcardID, orders[wildcardID]);
	ASSERT (is_normal_mo_infer(orders[wildcardID]));
	switch (orders[wildcardID]) {
		case memory_order_seq_cst:
			return 0;
		case memory_order_relaxed:
			if (mo == memory_order_relaxed)
				return 0;
			orders[wildcardID] = mo;
			break;
		case memory_order_acquire:
			if (mo == memory_order_acquire || mo == memory_order_relaxed)
				return 0;
			if (mo == memory_order_release)
				orders[wildcardID] = memory_order_acq_rel;
			else if (mo >= memory_order_acq_rel && mo <=
				memory_order_seq_cst)
				orders[wildcardID] = mo;
			break;
		case memory_order_release:
			if (mo == memory_order_release || mo == memory_order_relaxed)
				return 0;
			if (mo == memory_order_acquire)
				orders[wildcardID] = memory_order_acq_rel;
			else if (mo >= memory_order_acq_rel)
				orders[wildcardID] = mo;
			break;
		case memory_order_acq_rel:
			if (mo == memory_order_seq_cst)
				orders[wildcardID] = mo;
			else
				return 0;
			break;
		default:
			orders[wildcardID] = mo;
			break;
	}
	return 1;
}

Inference::Inference() {
	orders = (memory_order *) model_malloc((4 + 1) * sizeof(memory_order*));
	size = 4;
	for (int i = 0; i <= size; i++)
		orders[i] = WILDCARD_NONEXIST;
	initialInfer = this;
	buggy = false;
	hasFixes = false;
	leaf = false;
	explored = false;
	shouldFix = true;
}

Inference::Inference(Inference *infer) {
	ASSERT (infer->size > 0 && infer->size <= MAX_WILDCARD_NUM);
	orders = (memory_order *) model_malloc((infer->size + 1) * sizeof(memory_order*));
	this->size = infer->size;
	for (int i = 0; i <= size; i++)
		orders[i] = infer->orders[i];

	initialInfer = infer->initialInfer;
	buggy = false;
	hasFixes = false;
	leaf = false;
	explored = false;
	shouldFix = true;
}

/** return value:
  * 0 -> mo1 == mo2;
  * 1 -> mo1 stronger than mo2;
  * -1 -> mo1 weaker than mo2;
  * 2 -> mo1 & mo2 are uncomparable.
 */
int Inference::compareMemoryOrder(memory_order mo1, memory_order mo2) {
	if (mo1 == WILDCARD_NONEXIST)
		mo1 = memory_order_relaxed;
	if (mo2 == WILDCARD_NONEXIST)
		mo2 = memory_order_relaxed;
	if (mo1 == mo2)
		return 0;
	if (mo1 == memory_order_relaxed)
		return -1;
	if (mo1 == memory_order_acquire) {
		if (mo2 == memory_order_relaxed)
			return 1;
		if (mo2 == memory_order_release)
			return 2;
		return -1;
	}
	if (mo1 == memory_order_release) {
		if (mo2 == memory_order_relaxed)
			return 1;
		if (mo2 == memory_order_acquire)
			return 2;
		return -1;
	}
	if (mo1 == memory_order_acq_rel) {
		if (mo2 == memory_order_seq_cst)
			return -1;
		else
			return 1;
	}
	// mo1 now must be SC and mo2 can't be SC
	return 1;
}


/** Try to calculate the set of inferences that are weaker than this, but
 *  still stronger than infer */
InferenceList* Inference::getWeakerInferences(Inference *infer) {
	// An array of strengthened wildcards
	SnapVector<int> *strengthened = new SnapVector<int>;
	model_print("Strengthened wildcards\n");
	for (int i = 1; i <= size; i++) {
		memory_order mo1 = orders[i],
			mo2 = (*infer)[i];
		int compVal = compareMemoryOrder(mo1, mo2);
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

	// Got the strengthened wildcards, find out weaker inferences
	// First get a volatile copy of this inference
	Inference *tmpRes = new Inference(this);
	InferenceList *res = new InferenceList;
	if (strengthened->size() == 0)
		return res;
	getWeakerInferences(res, tmpRes, this, infer, strengthened, 0);
	res->pop_front();
	res->pop_back();
	InferenceList::print(res, "Weakened");
	return res;
}


// seq_cst -> acq_rel -> acquire -> release -> relaxed
memory_order Inference::nextWeakOrder(memory_order mo1, memory_order mo2) {
	memory_order res;
	switch (mo1) {
		case memory_order_seq_cst:
			res = memory_order_acq_rel;
			break;
		case memory_order_acq_rel:
			res = memory_order_acquire;
			break;
		case memory_order_acquire:
			res = memory_order_relaxed;
			break;
		case memory_order_release:
			res = memory_order_relaxed;
			break;
		case memory_order_relaxed:
			res = memory_order_relaxed;
			break;
		default: 
			res = memory_order_relaxed;
			break;
	}
	int compVal = compareMemoryOrder(res, mo2);
	if (compVal == 2 || compVal == -1) // Incomparable
		res = mo2;
	return res;
}

void Inference::getWeakerInferences(InferenceList* list, Inference *tmpRes,
	Inference *infer1, Inference *infer2, SnapVector<int> *strengthened, unsigned idx) {
	if (idx == strengthened->size()) { // Ready to produce one weakened result
		Inference *res = new Inference(tmpRes);
		//model_print("Weakened inference:\n");
		//res->print();
		res->setShouldFix(false);
		list->push_back(res);
		return;
	}

	int w = (*strengthened)[idx]; // The wildcard
	memory_order mo1 = (*infer1)[w];
	memory_order mo2 = (*infer2)[w];
	if (mo2 == WILDCARD_NONEXIST)
		mo2 = memory_order_relaxed;
	memory_order weakenedMO = mo1;
	do {
		(*tmpRes)[w] = weakenedMO;
		getWeakerInferences(list, tmpRes, infer1, infer2,
			strengthened, idx + 1);
		if (weakenedMO == memory_order_acq_rel) {
			(*tmpRes)[w] = memory_order_release;
			getWeakerInferences(list, tmpRes, infer1, infer2,
				strengthened, idx + 1);
		}
		weakenedMO = nextWeakOrder(weakenedMO, mo2);
		model_print("weakendedMO=%d\n", weakenedMO);
		model_print("mo2=%d\n", mo2);
	} while (weakenedMO != mo2);
	(*tmpRes)[w] = weakenedMO;
	getWeakerInferences(list, tmpRes, infer1, infer2,
		strengthened, idx + 1);
}

memory_order& Inference::operator[](int idx) {
	if (idx > 0 && idx <= size)
		return orders[idx];
	else {
		resize(idx);
		orders[idx] = WILDCARD_NONEXIST;
		return orders[idx];
	}
}

/** A simple overload, which allows caller to pass two boolean refrence, and
 * we will set those two variables indicating whether we can update the
 * order (copatible or not) and have updated the order */
int Inference::strengthen(const ModelAction *act, memory_order mo, bool &canUpdate, bool &hasUpdated) {
	int res = strengthen(act, mo);
	if (res == -1)
		canUpdate = false;
	if (res == 1)
		hasUpdated = true;

	return res;
}

/** @Return:
	1 -> 'this> infer';
	-1 -> 'this < infer'
	0 -> 'this == infer'
	INFERENCE_INCOMPARABLE(x) -> true means incomparable
 */
int Inference::compareTo(const Inference *infer) const {
	int result = size == infer->size ? 0 : (size > infer->size) ? 1 : -1;
	int smallerSize = size > infer->size ? infer->size : size;
	int subResult;

	for (int i = 0; i <= smallerSize; i++) {
		memory_order mo1 = orders[i],
			mo2 = infer->orders[i];
		if ((mo1 == memory_order_acquire && mo2 == memory_order_release) ||
			(mo1 == memory_order_release && mo2 == memory_order_acquire)) {
			// Incomparable
			return -2;
		} else {
			if ((mo1 == WILDCARD_NONEXIST && mo2 != WILDCARD_NONEXIST)
				|| (mo1 == WILDCARD_NONEXIST && mo2 == memory_order_relaxed)
				|| (mo1 == memory_order_relaxed && mo2 == WILDCARD_NONEXIST)
				)
				subResult = 1;
			else if (mo1 != WILDCARD_NONEXIST && mo2 == WILDCARD_NONEXIST)
				subResult = -1;
			else
				subResult = mo1 > mo2 ? 1 : (mo1 == mo2) ? 0 : -1;

			if ((subResult > 0 && result < 0) || (subResult < 0 && result > 0)) {
				return -2;
			}
			if (subResult != 0)
				result = subResult;
		}
	}
	return result;
}

unsigned long Inference::getHash() {
	unsigned long hash = 0;
	for (int i = 1; i <= size; i++) {
		memory_order mo = orders[i];
		if (mo == WILDCARD_NONEXIST) {
			mo = memory_order_relaxed;
		}
		hash *= 37;
		hash += (mo + 4096);
	}
	return hash;
}


void Inference::print(bool hasHash) {
	ASSERT(size > 0 && size <= MAX_WILDCARD_NUM);
	for (int i = 1; i <= size; i++) {
		memory_order mo = orders[i];
		if (mo != WILDCARD_NONEXIST) {
			// Print the wildcard inference result
			FENCE_PRINT("wildcard %d -> memory_order_%s\n", i, get_mo_str(mo));
		}
	}
	if (hasHash)
		FENCE_PRINT("Hash: %lu\n", getHash());
}

void Inference::print() {
	print(false);
}
