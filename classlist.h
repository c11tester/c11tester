#ifndef CLASSLIST_H
#define CLASSLIST_H
#include "stl-model.h"

class ClockVector;
class CycleGraph;
class CycleNode;
class ModelAction;
class ModelChecker;
class ModelExecution;
class ModelHistory;
class Node;
class NodeStack;
class Scheduler;
class Thread;
class TraceAnalysis;
class Fuzzer;
class FuncNode;
class FuncInst;

struct model_snapshot_members;
struct bug_message;
typedef SnapList<ModelAction *> action_list_t;
#endif
