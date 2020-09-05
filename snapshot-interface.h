/**
 * @file snapshot-interface.h
 * @brief C interface layer on top of snapshotting system
 */

#ifndef __SNAPINTERFACE_H
#define __SNAPINTERFACE_H
#include <ucontext.h>

typedef unsigned int snapshot_id;
typedef void (*VoidFuncPtr)();

void snapshot_system_init(unsigned int numheappages);
void startExecution();
snapshot_id take_snapshot();
void snapshot_roll_back(snapshot_id theSnapShot);


#endif
