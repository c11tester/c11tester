#ifndef _PATCH_H
#define _PATCH_H

#include "fence_common.h"
#include "inference.h"

class PatchUnit;
class Patch;

class PatchUnit {
	private:
	const ModelAction *act;
	memory_order mo;

	public:
	PatchUnit(const ModelAction *act, memory_order mo) {
		this->act= act;
		this->mo = mo;
	}

	const ModelAction* getAct() {
		return act;
	}

	memory_order getMO() {
		return mo;
	}

	SNAPSHOTALLOC
};

class Patch {
	private:
	SnapVector<PatchUnit*> *units;

	public:
	Patch(const ModelAction *act, memory_order mo);

	Patch(const ModelAction *act1, memory_order mo1, const ModelAction *act2,
		memory_order mo2);

	Patch();

	bool canStrengthen(Inference *curInfer);

	bool isApplicable();

	void addPatchUnit(const ModelAction *act, memory_order mo);

	int getSize();

	PatchUnit* get(int i);

	void print();

	SNAPSHOTALLOC
};

#endif
