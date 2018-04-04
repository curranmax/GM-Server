
#include "args.h"

#include "logger.h"

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>

Args::Args(const int &argc, char const *argv[]) {
	// Checks for a '-config' flag
	std::string config_file = "";
	for(int i = 1; i < argc; ++i) {
		if(strcmp(argv[i], "-config") == 0 && i + 1 < argc) {
			config_file = argv[i + 1];
			break;
		}
	}

	std::vector<std::string> cmd_args;
	if(config_file != "") {
		std::ifstream ifstr(config_file, std::ifstream::in);
		std::string token = "";

		while(ifstr >> token) {
			cmd_args.push_back(token);
		}
	}
	for(int i = 1; i < argc; ++i) {
		if(strcmp(argv[i], "-config") == 0 && i + 1 < argc) {
			++i;
			continue;
		}
		cmd_args.push_back(argv[i]);
	}

	std::stringstream log_file_sstr;
	log_file_sstr << "log/log_" << Logger::Now() << ".txt";

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
			Arg_Val(&k_proportional, 1.0, "-kp", "K_PROPORTIONAL", "Factor used in proportional part of tracking system"),
			Arg_Val(&k_integral,     0.0, "-ki", "K_INTEGRAL",     "Factor used in integral part of tracking system"),
			Arg_Val(&k_derivative,   0.0, "-kd", "K_DERIVATIVE",   "Factor used in derivative part of tracking system"),
			Arg_Val(&keep_beam_stationary, "-kbs", "If this flag is set then will try and make the difference between opposite diodes equal to the initial difference"),
			Arg_Val(&data_drive_tracking_file, "", "-ddtf", "DATA_DRIVEN_TRACKING_FILE", "File to use for data driven technique."),
			Arg_Val(&epsilon, 0.001, "-eps", "EPSILON", "Epsilon used in data driven tracking."),
			Arg_Val(&do_map_voltage, "-do_map", "Instead of normal tracking algorithm, does runs TrackingSystem::mapVoltage"),
			Arg_Val(&map_range, 500, "-mr", "MAP_RANGE", "Range to use for TrackingSystem::mapVoltage"),
			Arg_Val(&map_step, 10, "-ms", "MAP_STEP", "Step to use for TrackingSystem::mapVoltage"),
			Arg_Val(&record_type, "all", "-record_type", "RECORD_TYPE", "Either 'all' or 'linear'"),
			Arg_Val(&map_voltage_out_file, "data/map_voltage_data.txt", "-map_voltage_out_file", "MV_OUT", "File to write data to"),
			Arg_Val(&sfp_map_in_file, "data/sfp_map.txt", "-sfp_map_in", "MV_IN", "File to get data from for SFP tracking"),
			Arg_Val(&sfp_tracking_start, 0.0, "-sfp_start", "START", "Percent Difference from Peak to start tracking"),
			Arg_Val(&sfp_tracking_stop, 0.0, "-sfp_stop", "STOP", "Percent Difference from Peak to stop tracking"),
			Arg_Val(&sfp_power_zero_value, 0.0, "-sfp_zero", "ZERO_VAL", "Any Power less than or equal to this value will be clamped to 0.0"),
			Arg_Val(&sfp_search_delta, 0, "-sfp_sd", "SEARCH_DELTA", "The search delta to use for the SFP Auto Alignment"),
			Arg_Val(&sfp_num_search_locs, 3, "-sfp_nsl", "NUM_SEARCH_LOCS", "The number of locations to search when doing SFP Auto Alignment. Must be either 3 or 5."),
			Arg_Val(&sfp_table_epsilon, 0.0, "-sfp_eps", "EPSILON", "When doing the table lookup, differences between current Power and Powers in the table less than the given epsilon will be treated as 0"),
			Arg_Val(&sfp_relative_table_option, 0, "-sfp_rto", "TABLE_OPTIONS", "Controls the table lookup options"),
			Arg_Val(&sfp_map_power, "-map_sfp", "Instead of normal tracking algorithm, maps a large value of GM settings"),
			Arg_Val(&sfp_map_range, 500, "-sfp_mr", "MAP_RANGE", "Range to use for mapping the SFP received power"),
			Arg_Val(&sfp_map_step, 10, "-sfp_ms", "MAP_STEP", "Step to use for mapping the SFP received power"),
			Arg_Val(&sfp_map_out_file, "data/sfp_map.txt", "-sfp_map_out", "MV_OUT", "File to write SFP map data to"),
			Arg_Val(&sfp_test_server, "-sfp_test_server", "If given, sends the GM values to the other side"),
			Arg_Val(&sfp_no_update, "-sfp_no_update", "If given, fake Arduino, doesn't move beam while tracking"),
			Arg_Val(&sfp_gradient_threshold, 0.0, "-sfp_gt", "GRADIENT_THRESHOLD", "Threshold of the gradient in the Constant Update Algorithm"),
			Arg_Val(&sfp_constant_response, 0, "-sfp_const_resp", "CONSTANT_RESPONSE", "Constant value used for the Constant Update Algorithm"),
			Arg_Val(&sfp_max_num_messages, 1, "-sfp_mnm", "MAX_MESSAGES", "The maximum number of messages used when fetching the Power"),
			Arg_Val(&sfp_max_num_changes, 1, "-sfp_mnc", "MAX_CHANGES", "The number of changes in received Power before returning a value"),
			Arg_Val(&sfp_num_message_average, 1, "-sfp_nma", "NUM_TO_AVERAGE", "The number of unique Powers to average"),
			Arg_Val(&log_file, log_file_sstr.str(), "-log_file", "FILE", "File to write log information to"),
			Arg_Val(&log_level, 0, "-vlog", "VERBOSE_LEVEL", "Verbositiy level used when logging"),
			Arg_Val(&log_stderr, "-log_stderr", "If given, also logs to STDERR")
		};

	for(unsigned int i = 0; i < cmd_args.size(); ++i) {
		if(cmd_args[i] == "-h" || cmd_args[i] == "-help") {
			printHelp();
		}
		bool match_found = false;
		for(unsigned int j = 0; j < args.size(); ++j) {
			if(args[j].isMatch(cmd_args, i)) {
				args[j].setVal(cmd_args, i);
				match_found = true;
				break;
			}
		}
		// IF NONE FOUND PRINT AN ERROR MESSAGE
		if(!match_found) {
			std::cerr << "INVALID COMMAND LINE ARG: " << cmd_args[i] << std::endl;
		}
	}

	// Init Logger
	Logger::init(log_file, log_level, log_stderr);

	{
		// Logs the cmd_args
		std::stringstream args_sstr;
		args_sstr << "cmd_args: {";
		for(unsigned int i = 0; i < cmd_args.size(); ++i) {
			if(i != 0) {
				args_sstr << ", ";
			}
			args_sstr << cmd_args[i];
		}
		args_sstr << "}";
		LOG(args_sstr.str());
	}
}

void Args::printHelp() const {
	for(unsigned int i = 0; i < args.size(); ++i) {
		std::cout << " " << args[i].flag_str << " " << args[i].input_name << std::endl;
		std::cout << "    " << args[i].help_text << std::endl << std::endl;
	}
	exit(0);
}

bool Args::Arg_Val::isMatch(const std::vector<std::string> &cmd_args, unsigned int &i) {
	if(cmd_args[i] == flag_str) {
		if(val_type == "f" || val_type == "i" || val_type == "s") {
			if(i >= cmd_args.size() - 1) {
				return false;
			}
			i++;
		}
		return true;
	}
	return false;
}

void Args::Arg_Val::setVal(const std::vector<std::string> &cmd_args, unsigned int &i) {
	if(val_type == "f") {
		*((float*) val_ptr) = atof(cmd_args[i].c_str());
	}
	if(val_type == "i") {
		*((int*) val_ptr) = atoi(cmd_args[i].c_str());
	}
	if(val_type == "s") {
		*((std::string*) val_ptr) = cmd_args[i];
	}
	if(val_type == "b") {
		*((bool*) val_ptr) = true;
	}
}
