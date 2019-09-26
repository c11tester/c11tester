#ifndef CLASSLIST_H
#define CLASSLIST_H
#include <inttypes.h>
#include "stl-model.h"
#include "hashset.h"

class ClockVector;
class CycleGraph;
class CycleNode;
class ModelAction;
class ModelChecker;
class ModelExecution;
class ModelHistory;
class Scheduler;
class Thread;
class TraceAnalysis;
class Fuzzer;
class NewFuzzer;
class FuncNode;
class FuncInst;
class Predicate;

struct model_snapshot_members;
struct bug_message;
typedef SnapList<ModelAction *> action_list_t;
typedef SnapList<uint32_t> func_id_list_t;
typedef SnapList<FuncInst *> func_inst_list_t;
typedef HashTable<FuncInst *, ModelAction *, uintptr_t, 0> inst_act_map_t;

typedef HashSet<Predicate *, uintptr_t, 0, model_malloc, model_calloc, model_free> PredSet;
typedef HSIterator<Predicate *, uintptr_t, 0, model_malloc, model_calloc, model_free> PredSetIter;

typedef HashSet<uint64_t, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free> value_set_t;
typedef HSIterator<uint64_t, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free> value_set_iter;
typedef HashSet<void *, uintptr_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free> loc_set_t;
typedef HSIterator<void *, uintptr_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free> loc_set_iter;

extern volatile int modellock;
#endif
