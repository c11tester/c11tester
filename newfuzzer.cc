#include "newfuzzer.h"
#include "threads-model.h"
#include "model.h"
#include "action.h"

int NewFuzzer::selectWrite(ModelAction *read, SnapVector<ModelAction *> * rf_set)
{
	int random_index = random() % rf_set->size();
	return random_index;
}
