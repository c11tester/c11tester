#include "patch.h"
#include "inference.h"

Patch::Patch(const ModelAction *act, memory_order mo) {
	PatchUnit *unit = new PatchUnit(act, mo);
	units = new SnapVector<PatchUnit*>;
	units->push_back(unit);
}

Patch::Patch(const ModelAction *act1, memory_order mo1, const ModelAction *act2, memory_order mo2) {
	units = new SnapVector<PatchUnit*>;
	PatchUnit *unit = new PatchUnit(act1, mo1);
	units->push_back(unit);
	unit = new PatchUnit(act2, mo2);
	units->push_back(unit);
}

Patch::Patch() {
	units = new SnapVector<PatchUnit*>;
}

bool Patch::canStrengthen(Inference *curInfer) {
	if (!isApplicable())
		return false;
	bool res = false;
	for (unsigned i = 0; i < units->size(); i++) {
		PatchUnit *u = (*units)[i];
		memory_order wildcard = u->getAct()->get_original_mo();
		memory_order curMO = (*curInfer)[get_wildcard_id(wildcard)];
		if (u->getMO() != curMO)
			res = true;
	}
	return res;
}

bool Patch::isApplicable() {
	for (unsigned i = 0; i < units->size(); i++) {
		PatchUnit *u = (*units)[i];
		memory_order wildcard = u->getAct()->get_original_mo();
		if (is_wildcard(wildcard))
			continue;
		int compVal = Inference::compareMemoryOrder(wildcard, u->getMO());
		if (compVal == 2 || compVal == -1)
			return false;
	}
	return true;
}

void Patch::addPatchUnit(const ModelAction *act, memory_order mo) {
	PatchUnit *unit = new PatchUnit(act, mo);
	units->push_back(unit);
}

int Patch::getSize() {
	return units->size();
}

PatchUnit* Patch::get(int i) {
	return (*units)[i];
}

void Patch::print() {
	for (unsigned i = 0; i < units->size(); i++) {
		PatchUnit *u = (*units)[i];
		model_print("wildcard %d -> %s\n",
			get_wildcard_id_zero(u->getAct()->get_original_mo()),
			get_mo_str(u->getMO()));
	}
}
