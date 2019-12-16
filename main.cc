/** @file main.cc
 *  @brief Entry point for the model checker.
 */

#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "common.h"
#include "output.h"

#include "datarace.h"

/* global "model" object */
#include "model.h"
#include "params.h"
#include "snapshot-interface.h"
#include "plugins.h"

void param_defaults(struct model_params *params)
{
	params->verbose = !!DBG_ENABLED();
	params->maxexecutions = 10;
	params->traceminsize = 0;
	params->checkthreshold = 500000;
	params->removevisible = false;
	params->nofork = false;
}

static void print_usage(struct model_params *params)
{
	ModelVector<TraceAnalysis *> * registeredanalysis=getRegisteredTraceAnalysis();
	/* Reset defaults before printing */
	param_defaults(params);

	model_print(
		"Copyright (c) 2013 Regents of the University of California. All rights reserved.\n"
		"Distributed under the GPLv2\n"
		"Written by Brian Norris and Brian Demsky\n"
		"\n"
		"Usage: C11TESTER=[MODEL-CHECKER OPTIONS]\n"
		"\n"
		"MODEL-CHECKER OPTIONS can be any of the model-checker options listed below. Arguments\n"
		"provided after the `--' (the PROGRAM ARGS) are passed to the user program.\n"
		"\n"
		"Model-checker options:\n"
		"-h, --help                  Display this help message and exit\n"
		"-v[NUM], --verbose[=NUM]    Print verbose execution information. NUM is optional:\n"
		"                              0 is quiet; 1 shows valid executions; 2 is noisy;\n"
		"                              3 is noisier.\n"
		"                              Default: %d\n"
		"-t, --analysis=NAME         Use Analysis Plugin.\n"
		"-o, --options=NAME          Option for previous analysis plugin.  \n"
		"-x, --maxexec=NUM           Maximum number of executions.\n"
		"                            Default: %u\n"
		"                            -o help for a list of options\n"
		"-n                          No fork\n"
		"-m, --minsize=NUM           Minimum number of actions to keep\n",
		"                            Default: %u\n"
		"-f, --freqfree=NUM          Frequency to free actions\n"
		"                            Default: %u\n"
		"-r, --removevisible         Free visible writes\n",
		params->verbose,
		params->maxexecutions,
		params->traceminsize,
		params->checkthreshold);
	model_print("Analysis plugins:\n");
	for(unsigned int i=0;i<registeredanalysis->size();i++) {
		TraceAnalysis * analysis=(*registeredanalysis)[i];
		model_print("%s\n", analysis->name());
	}
	exit(EXIT_SUCCESS);
}

bool install_plugin(char * name) {
	ModelVector<TraceAnalysis *> * registeredanalysis=getRegisteredTraceAnalysis();
	ModelVector<TraceAnalysis *> * installedanalysis=getInstalledTraceAnalysis();

	for(unsigned int i=0;i<registeredanalysis->size();i++) {
		TraceAnalysis * analysis=(*registeredanalysis)[i];
		if (strcmp(name, analysis->name())==0) {
			installedanalysis->push_back(analysis);
			return false;
		}
	}
	model_print("Analysis %s Not Found\n", name);
	return true;
}

void parse_options(struct model_params *params) {
	const char *shortopts = "hrnt:o:x:v:m:f::";
	const struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"verbose", optional_argument, NULL, 'v'},
		{"analysis", required_argument, NULL, 't'},
		{"options", required_argument, NULL, 'o'},
		{"maxexecutions", required_argument, NULL, 'x'},
		{"minsize", required_argument, NULL, 'm'},
		{"freqfree", required_argument, NULL, 'f'},
		{"removevisible", no_argument, NULL, 'r'},
		{0, 0, 0, 0}	/* Terminator */
	};
	int opt, longindex;
	int tmpoptind = optind;
	bool error = false;
	char * options = getenv("C11TESTER");

	if (options == NULL)
		return;
	int argc = 1;
	int index;
	for(index = 0;options[index]!=0;index++) {
		if (options[index] == ' ')
			argc++;
	}
	argc++;	//first parameter is executable name
	char optcpy[index + 1];
	memcpy(optcpy, options, index+1);
	char * argv[argc + 1];
	argv[0] = NULL;
	argv[1] = optcpy;
	int count = 2;
	for(index = 0;optcpy[index]!=0;index++) {
		if (optcpy[index] == ' ') {
			argv[count++] = &optcpy[index+1];
			optcpy[index] = 0;
		}
	}

	while (!error && (opt = getopt_long(argc, argv, shortopts, longopts, &longindex)) != -1) {
		switch (opt) {
		case 'h':
			print_usage(params);
			break;
		case 'n':
			params->nofork = true;
			break;
		case 'x':
			params->maxexecutions = atoi(optarg);
			break;
		case 'v':
			params->verbose = optarg ? atoi(optarg) : 1;
			break;
		case 't':
			if (install_plugin(optarg))
				error = true;
			break;
		case 'm':
			params->traceminsize = atoi(optarg);
			break;
		case 'f':
			params->checkthreshold = atoi(optarg);
			break;
		case 'r':
			params->removevisible = true;
			break;
		case 'o':
		{
			ModelVector<TraceAnalysis *> * analyses = getInstalledTraceAnalysis();
			if ( analyses->size() == 0 || (*analyses)[analyses->size()-1]->option(optarg))
				error = true;
		}
		break;
		default:	/* '?' */
			error = true;
			break;
		}
	}

	/* Restore (global) optind for potential use by user program */
	optind = tmpoptind;

	if (error)
		print_usage(params);
}

void install_trace_analyses(ModelExecution *execution) {
	ModelVector<TraceAnalysis *> * installedanalysis=getInstalledTraceAnalysis();
	for(unsigned int i=0;i<installedanalysis->size();i++) {
		TraceAnalysis * ta=(*installedanalysis)[i];
		ta->setExecution(execution);
		model->add_trace_analysis(ta);
		/** Call the installation event for each installed plugin */
		ta->actionAtInstallation();
	}
}
