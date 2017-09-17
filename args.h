
#ifndef _ARGS_H_
#define _ARGS_H_

#include <string>
#include <vector>

class Args {
  private:
	// Container used to declare command line input
	class Arg_Val {
	  public:
		Arg_Val(float* val_ptr_,float def_val,const std::string &flag_str_,const std::string &input_name_,const std::string &help_text_) { val_ptr = (void*) val_ptr_; *val_ptr_ = def_val; flag_str = flag_str_; val_type = "f"; input_name = input_name_; help_text = help_text_; }
		Arg_Val(int* val_ptr_,int def_val,const std::string &flag_str_,const std::string &input_name_,const std::string &help_text_) { val_ptr = (void*) val_ptr_; *val_ptr_ = def_val; flag_str = flag_str_; val_type = "i"; input_name = input_name_; help_text = help_text_; }
		Arg_Val(std::string* val_ptr_,const std::string &def_val,const std::string &flag_str_,const std::string &input_name_,const std::string &help_text_) { val_ptr = (void*) val_ptr_; *val_ptr_ = def_val; flag_str = flag_str_; val_type = "s"; input_name = input_name_; help_text = help_text_; }
		Arg_Val(bool* val_ptr_,const std::string &flag_str_,const std::string &help_text_) { val_ptr = (void*) val_ptr_; *val_ptr_ = false; flag_str = flag_str_; val_type = "b"; help_text = help_text_; }

		Arg_Val(const Arg_Val& av) {val_ptr = av.val_ptr; flag_str = av.flag_str; val_type = av.val_type; input_name = av.input_name; help_text = av.help_text; }

		bool isMatch(const std::vector<std::string> &cmd_args, unsigned int &i);
		void setVal(const std::vector<std::string> &cmd_args, unsigned int &i);

		void* val_ptr;
		std::string flag_str;
		std::string val_type;
		std::string input_name;
		std::string help_text;
	};
	
  public:
	Args(const int &argc, char const *argv[]);
	~Args() {}

	void printHelp() const;

	// Variables
	bool coarse_alignment;
	bool gm_server;
	bool gm_network_controller;
	bool dom_timing_test;
	bool verbose;
	bool debug;
	bool fake_dom;
	bool manual_save;
	bool dom_stay_connected;

	std::string fso_dir;
	std::string auto_algo;

	float auto_align_delta;

	std::vector<int> ints;
	std::vector<Arg_Val> args;

	// Auto Alignment Testing
	std::string peak_file;
	int num_peak_tests;
	float random_min_val;
	float random_max_val;

	// DOM Testing
	int num_dom_iters;

	// Tracking system
	float k_proportional;
	float k_integral;
	float k_derivative;
	bool keep_beam_stationary;

	// Data Driven Tracking System;
	std::string data_drive_tracking_file;
	float epsilon;

	// Tracking system - mapVoltage
	bool do_map_voltage;
	int map_range;
	int map_step;
	std::string record_type;
	std::string map_voltage_out_file;

	// SFP Auto Alignment
	std::string sfp_map_in_file;
	float sfp_tracking_start;
	float sfp_tracking_stop;
	float sfp_rssi_zero_value;

	int sfp_search_delta;
	int sfp_num_search_locs;
	float sfp_table_epsilon;

	// SFP Auto Alignment - map
	bool sfp_map_power;
	int sfp_map_range;
	int sfp_map_step;
	std::string sfp_map_out_file;

	// SFP Auto Alignment - debug options
	bool sfp_test_server;

	// SFP Auto Alignment - getRSSI options
	int sfp_max_num_messages;
	int sfp_max_num_changes;
	int sfp_num_message_average;

	// LOGGER
	std::string log_file;
	int log_level;
	bool log_stderr;
};

#endif
