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
	params->uninitvalue = 0;
	params->maxexecutions = 10;
	params->nofork = false;
}

static void print_usage(const char *program_name, struct model_params *params)
{
	ModelVector<TraceAnalysis *> * registeredanalysis=getRegisteredTraceAnalysis();
	/* Reset defaults before printing */
	param_defaults(params);

	model_print(
		"Copyright (c) 2013 Regents of the University of California. All rights reserved.\n"
		"Distributed under the GPLv2\n"
		"Written by Brian Norris and Brian Demsky\n"
		"\n"
		"Usage: %s [MODEL-CHECKER OPTIONS] -- [PROGRAM ARGS]\n"
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
		"-u, --uninitialized=VALUE   Return VALUE any load which may read from an\n"
		"                              uninitialized atomic.\n"
		"                              Default: %u\n"
		"-t, --analysis=NAME         Use Analysis Plugin.\n"
		"-o, --options=NAME          Option for previous analysis plugin.  \n"
		"-x, --maxexec=NUM           Maximum number of executions.\n"
		"                            Default: %u\n"
		"                            -o help for a list of options\n"
		"-n                          No fork\n"
		" --                         Program arguments follow.\n\n",
		program_name,
		params->verbose,
		params->uninitvalue,
		params->maxexecutions);
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

static void parse_options(struct model_params *params, int argc, char **argv)
{
	const char *shortopts = "hnt:o:u:x:v::";
	const struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"verbose", optional_argument, NULL, 'v'},
		{"uninitialized", required_argument, NULL, 'u'},
		{"analysis", required_argument, NULL, 't'},
		{"options", required_argument, NULL, 'o'},
		{"maxexecutions", required_argument, NULL, 'x'},
		{0, 0, 0, 0}	/* Terminator */
	};
	int opt, longindex;
	bool error = false;
	while (!error && (opt = getopt_long(argc, argv, shortopts, longopts, &longindex)) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0], params);
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
		case 'u':
			params->uninitvalue = atoi(optarg);
			break;
		case 't':
			if (install_plugin(optarg))
				error = true;
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

	/* Pass remaining arguments to user program */
	params->argc = argc - (optind - 1);
	params->argv = argv + (optind - 1);

	/* Reset program name */
	params->argv[0] = argv[0];

	/* Reset (global) optind for potential use by user program */
	optind = 1;

	if (error)
		print_usage(argv[0], params);
}

int main_argc;
char **main_argv;

static void install_trace_analyses(ModelExecution *execution)
{
	ModelVector<TraceAnalysis *> * installedanalysis=getInstalledTraceAnalysis();
	for(unsigned int i=0;i<installedanalysis->size();i++) {
		TraceAnalysis * ta=(*installedanalysis)[i];
		ta->setExecution(execution);
		model->add_trace_analysis(ta);
		/** Call the installation event for each installed plugin */
		ta->actionAtInstallation();
	}
}

/** The model_main function contains the main model checking loop. */
static void model_main()
{
	snapshot_record(0);
	model->run();
	delete model;

	DEBUG("Exiting\n");
}

/**
 * Main function.  Just initializes snapshotting library and the
 * snapshotting library calls the model_main function.
 */
int main(int argc, char **argv)
{
	main_argc = argc;
	main_argv = argv;

	/*
	 * If this printf statement is removed, CDSChecker will fail on an
	 * assert on some versions of glibc.  The first time printf is
	 * called, it allocated internal buffers.  We can't easily snapshot
	 * libc since we also use it.
	 */

	printf("CDSChecker\n"
				 "Copyright (c) 2013 Regents of the University of California. All rights reserved.\n"
				 "Distributed under the GPLv2\n"
				 "Written by Brian Norris and Brian Demsky\n\n");

	/* Configure output redirection for the model-checker */
	redirect_output();

	//Initialize snapshotting library and model checker object
	if (!model) {
		snapshot_system_init(10000, 1024, 1024, 40000);
		model = new ModelChecker();
	}

	register_plugins();

	//Parse command line options
	model_params *params = model->getParams();
	parse_options(params, main_argc, main_argv);

	//Initialize race detector
	initRaceDetector();

	snapshot_stack_init();
	install_trace_analyses(model->get_execution());

	//Start everything up
	modelchecker_started = true;
	startExecution(&model_main);
}
