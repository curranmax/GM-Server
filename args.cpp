
#include "args.h"

#include <iostream>
#include <stdlib.h>
#include <string.h>

Args::Args(const int &argc, char const *argv[]) {
	// Initialize arguments and their flags
	args = {Arg_Val(&coarse_alignment,"-c","Starts coarse alignment mode"),
			Arg_Val(&gm_server,"-s","Starts gm server mode"),
			Arg_Val(&gm_network_controller,"-n","Starts the GM Network Controller"),
			Arg_Val(&dom_timing_test,"-dom","Runs a test to measure the time it takes to issue one DOM request"),
			Arg_Val(&verbose,"-v","Enables verbose mode"),
			Arg_Val(&debug,"-d","Enables debug mode (doesn't use actual GMs)"),
			Arg_Val(&fso_dir,"fso_data/","-fso","FSO_DIR","Directory with all FSO data"),
			Arg_Val(&fake_dom,"-fake_dom","Doesn't connect to Switch, and gives fake power readings"),
			Arg_Val(&manual_save,"-man_save","Modifying link settings does not save on quit"),
			Arg_Val(&dom_stay_connected,"-con","Keeps connection to switch active between auto_alignment"),
			Arg_Val(&auto_align_delta, 60.0, "-aad", "AAD", "Time (in seconds) between sucessive checks of power of the FSOs"),
			Arg_Val(&auto_algo,"gradient_descent_sphere","-aaa","AAA","Algorithm for auto align. Must be: full_scan, step_one, max_one, or gradient_descent"),
			Arg_Val(&peak_file, "Peak/data2.txt", "-peak_file", "PF", "File to store peak location and power"),
			Arg_Val(&num_peak_tests, 10, "-npt", "NPT", "Number of searches from random spots to perform"),
			Arg_Val(&random_min_val, 0.0, "-peak_rand_min", "RMIN", "Minimum distance to randomly go from first peak"),
			Arg_Val(&random_max_val, 100.0, "-peak_rand_max", "RMAX", "Maximum distance to randomly go from first peak"),
			Arg_Val(&num_dom_iters, 1, "-dom_iters", "DOM_ITERS", "Number of times to request the received power of a SFP in the DOM Tests"),
			Arg_Val(&k_proportional, 1.0, "-kp", "K_PROPORTIONAL", "Factor used in tracking system"),
			Arg_Val(&do_map_voltage, "-do_map", "Instead of normal tracking algorithm, does runs TrackingSystem::mapVoltage"),
			Arg_Val(&map_range, 500, "-mr", "MAP_RANGE", "Range to use for TrackingSystem::mapVoltage"),
			Arg_Val(&map_step, 10, "-ms", "MAP_STEP", "Step to use for TrackingSystem::mapVoltage"),
			Arg_Val(&record_type, "all", "-record_type", "RECORD_TYPE", "Either 'all' or 'linear'"),
			Arg_Val(&map_voltage_out_file, "data/map_voltage_data.txt", "-map_voltage_out_file", "MV_OUT", "File to write data to")
		};

	for(int i = 1; i < argc; ++i) {
		if(strcmp(argv[i],"-h") == 0 || strcmp(argv[i],"-help") == 0) {
			printHelp();
		}
		bool match_found = false;
		for(unsigned int j = 0; j < args.size(); ++j) {
			if(args[j].isMatch(argv,i,argc)) {
				args[j].setVal(argv,i,argc);
				match_found = true;
				break;
			}
		}
		// IF NONE FOUND PRINT AN ERROR MESSAGE
		if(!match_found) {
			std::cerr << "INVALID COMMAND LINE ARG: " << argv[i] << std::endl;
		}
	}
}

void Args::printHelp() const {
	for(unsigned int i = 0; i < args.size(); ++i) {
		std::cout << " " << args[i].flag_str << " " << args[i].input_name << std::endl;
		std::cout << "    " << args[i].help_text << std::endl << std::endl;
	}
	exit(0);
}

bool Args::Arg_Val::isMatch(char const **vs,int &i, int num_v) {
	if(vs[i] == flag_str) {
		if(val_type == "f" || val_type == "i" || val_type == "s") {
			if(i >= num_v - 1) {
				return false;
			}
			i++;
		}
		return true;
	}
	return false;
}

void Args::Arg_Val::setVal(char const **vs,int &i, int num_v) {
	if(val_type == "f") {
		*((float*) val_ptr) = atof(vs[i]);
	}
	if(val_type == "i") {
		*((int*) val_ptr) = atoi(vs[i]);
	}
	if(val_type == "s") {
		*((std::string*) val_ptr) = vs[i];
	}
	if(val_type == "b") {
		*((bool*) val_ptr) = true;
	}
}
