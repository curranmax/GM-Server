
#include "args.h"
#include "FSO.h"
#include "coarse_align.h"
#include "gm_server.h"
#include "gm_network_controller.h"
#include "dom_timing_test.h"
#include "link_mode.h"

#include "dom_fetcher.h"

#include <stdlib.h>
#include <iostream>
#include <dirent.h>
#include <string.h>
#include <time.h>

bool endsWith(const std::string &full_string,const std::string &suffix) {
	return full_string.length() >= suffix.length() && (0 == full_string.compare(full_string.length() - suffix.length(),suffix.length(),suffix));
}

void makeFSOs(Args* args,std::vector<FSO*> &fsos) {
	// Find all files in directory
	// Only works on Linux for now
	const std::string &fso_dir = args->fso_dir;
	DIR *dir;
	struct dirent *f_struct;
	dir = opendir(fso_dir.c_str());

	std::string file_type = ".data";

	if(dir != NULL) {
		while((f_struct = readdir(dir))) {
			// Check if filename ends in ".data"
			if(endsWith(f_struct->d_name,file_type)) {
				FSO* temp = new FSO(fso_dir + f_struct->d_name,args);
				fsos.push_back(temp);
			}
		}
		closedir(dir);
	} else {
		// Error directory not found
	}
}

int main(int argc, char const *argv[]) {
	srand(time(NULL));

	Args* args = new Args(argc,argv);

	// If both set, or both not set then error
	// if(!((args->coarse_alignment && !args->gm_server && !args->gm_network_controller) || (!args->coarse_alignment && args->gm_server && !args->gm_network_controller) || (!args->coarse_alignment && !args->gm_server && args->gm_network_controller))) {
	// 	std::cerr << "Invalid setting!  Please choose exactly one of: Coarse Alignment, GM Server, or GM Network Controller" << std::endl;
	// 	exit(0);
	// }

	// Read in FSO Data
	std::vector<FSO*> fsos;
	makeFSOs(args,fsos);

	if(args->coarse_alignment) {
		// Start coarse alignment
		CoarseAligner *ca = new CoarseAligner(fsos,args);
		ca->run();

		delete ca;
	}

	if(args->gm_server) {
		GMServer *server = new GMServer(fsos,args);
		server->run();

		delete server;
	}

	if(args->gm_network_controller) {
		GMNetworkController *network_controller = new GMNetworkController(args);
		network_controller->run();

		delete network_controller;
	}

	if(args->dom_timing_test) {
		DOMTimingTest *dom_timing_test = new DOMTimingTest(fsos, args);
		dom_timing_test->run();

		delete dom_timing_test;
	}

	if(args->run_link_mode) {
		LinkMode *link_mode = new LinkMode(fsos, args);
		link_mode->run();

		delete link_mode;
	}

	// FSO::printAvgDur();

	delete args;
	for(unsigned int i = 0; i < fsos.size(); ++i) {
		delete fsos[i];
	}
	return 0;
}
