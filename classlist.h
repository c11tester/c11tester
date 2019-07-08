#ifndef CLASSLIST_H
#define CLASSLIST_H
#include <inttypes.h>
#include "stl-model.h"

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

struct model_snapshot_members;
struct bug_message;
typedef SnapList<ModelAction *> action_list_t;
typedef SnapList<uint32_t> func_id_list_t;
typedef SnapList<FuncInst *> func_inst_list_t;

extern volatile int forklock;
#endif
