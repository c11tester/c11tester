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
class FuncNode;
class FuncInst;
class Predicate;

struct model_snapshot_members;
struct bug_message;
typedef SnapList<ModelAction *> action_list_t;
typedef SnapList<uint32_t> func_id_list_t;
typedef SnapList<FuncInst *> func_inst_list_t;
typedef HSIterator<Predicate *, uintptr_t, 0, model_malloc, model_calloc, model_free> PredSetIter;
typedef HashSet<Predicate *, uintptr_t, 0, model_malloc, model_calloc, model_free> PredSet;

extern volatile int modellock;
#endif
