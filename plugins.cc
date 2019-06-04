#include "plugins.h"

ModelVector<TraceAnalysis *> * registered_analysis;
ModelVector<TraceAnalysis *> * installed_analysis;

void register_plugins() {
	registered_analysis=new ModelVector<TraceAnalysis *>();
	installed_analysis=new ModelVector<TraceAnalysis *>();
}

ModelVector<TraceAnalysis *> * getRegisteredTraceAnalysis() {
	return registered_analysis;
}

ModelVector<TraceAnalysis *> * getInstalledTraceAnalysis() {
	return installed_analysis;
}
