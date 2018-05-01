
#include "coarse_align.h"

#include "auto_align.h"
#include "tracking.h"
#include "half_auto_align.h"
#include "sfp_auto_align.h"

#include <iostream>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <sstream>
#include <unistd.h>

#define OTHER_HOST "127.0.0.1" // "192.168.1.177"

// When modifying add instructions, and also maybe add command history


// From http://stackoverflow.com/questions/7469139/what-is-equivalent-to-getch-getche-in-linux
// -----------------------------------------------
static struct termios old, new0;

// Initialize new terminal i/o settings
void initTermios(int echo) {
	tcgetattr(0, &old);						// grab old terminal i/o settings
	new0 = old;								// make new settings same as old settings
	new0.c_lflag &= ~ICANON;				// disable buffered i/o
	new0.c_lflag &= echo ? ECHO : ~ECHO;	// set echo mode
	tcsetattr(0, TCSANOW, &new0);			// use these new terminal i/o settings now
}

// Restore old terminal i/o settings 
void resetTermios(void)  {
	tcsetattr(0, TCSANOW, &old);
}

// Read 1 character - echo defines echo mode
char getch_(int echo)  {
	char ch;
	initTermios(echo);
	ch = getchar();
	resetTermios();
	return ch;
}

// Read 1 character without echo
int getch() {
	int v = (int)getch_(0);
	return v;
}
// -----------------------------------------------

FSO* CoarseAligner::getFSO(std::string rack_id,std::string fso_id) const {
	for(unsigned int i = 0; i < fsos.size(); ++i) {
		if(fsos[i]->getRackID() == rack_id && fsos[i]->getFSOID() == fso_id) {
			return fsos[i];
		}
	}
	return NULL;
}

void CoarseAligner::listFSOs() const {
	for(unsigned int i = 0; i < fsos.size(); ++i) {
		std::cout << "  FSO (" << fsos[i]->getRackID() << "," << fsos[i]->getFSOID() << ")" << std::endl;
		std::cout << "    GM1 (" << fsos[i]->getUSBID(1) << "," << fsos[i]->getUSBChannel(1) << ")    GM2 (" << fsos[i]->getUSBID(2) << "," << fsos[i]->getUSBChannel(2) << ")" << std::endl;
		std::vector<std::string> link_strings = fsos[i]->getLinkStrings();
		for(unsigned int i = 0; i < link_strings.size(); ++i) {
			std::cout << "      " << link_strings[i] << std::endl;
		}
		std::cout << std::endl;
	}
}

void CoarseAligner::newFSO(const std::string &filename,const std::string &rack_id,const std::string &fso_id,const std::string &gm1_usb_id,int gm1_usb_channel,const std::string &gm2_usb_id,int gm2_usb_channel) {
	FSO* new_fso = new FSO(args->fso_dir + filename,rack_id,fso_id,gm1_usb_id,gm1_usb_channel,gm2_usb_id,gm2_usb_channel,args);
	new_fso->save();
	fsos.push_back(new_fso);
}

void CoarseAligner::run() {
	// loop over user input
	// user commands: list commands, list fsos, make new FSO, add link to FSO, wiggle fsos and gms, change delta, enter modify mode, save fso state, reload fso
	std::string command = "";
	char raw_input[256];
	while(true) {
		command = "";
		std::cout << ">> ";
		std::cin.getline(raw_input,256);
		std::stringstream sstr(raw_input);
		sstr >> command;
		if(command == "list_fsos") {
			listFSOs();
		} else if(command == "help") {
			// List all commands
			std::cout << " list_fsos" << std::endl << "\tList all FSOs and their appropriate information" << std::endl << std::endl;
			std::cout << " new_fso filename rack_id fso_id gm1_usb_id gm1_usb_channel gm2_usb_id gm2_usb_channel" << std::endl << "\tMakes new FSO with provided attributes" << std::endl << std::endl;
			std::cout << " new_link this_rack_id this_fso_id other_rack_id other_fso_id gm1_guess gm2_guess" << std::endl << "\tAdds a new candidate link to given FSO" << std::endl << std::endl;
			std::cout << " set_link this_rack_id this_fso_id other_rack_id other_fso_id" << std::endl << "\tSets the link of the given FSO" << std::endl;
			std::cout << " flip_links this_rack_id this_fso_id other_rack_id1 other_fso_id1 other_rack_id2 other_fso_id2 num_iters delay" << std::endl << "\tFlips the FSO between the two selected links" << std::endl;
			std::cout << " wiggle_fso rack_id fso_id" << std::endl << "\tWiggles the two mirrors of the given FSO" << std::endl << std::endl;
			std::cout << " set_delta new_delta" << std::endl << "\tChanges delta used when modifying GM position" << std::endl << std::endl;
			std::cout << " modify this_rack_id this_fso_id other_rack_id other_fso_id" << std::endl << "\tModifies the GM of selected FSO" << std::endl << std::endl;
			std::cout << " save_fso rack_id fso_id" << std::endl << "\tSaves FSO to file" << std::endl << std::endl;
			std::cout << " reload_fso rack_id fso_id" << std::endl << "\tReloads FSO from file, and deletes local changes" << std::endl << std::endl;
			std::cout << " set_val rack_id fso_id v1 v2" << std::endl << "\tSets the value of the selected FSOs GMs" << std::endl << std::endl;
			std::cout << " aa_control this_rack_id this_fso_id other_addr other_rack_id other_fso_id" << std::endl << "\tStarts Auto Alignment process as the controller" << std::endl;
			std::cout << " aa_listen this_rack_id this_fso_id" << std::endl << "\tStarts Auto Alignment process as the listener" << std::endl;
			std::cout << " ts_control this_rack_id this_fso_id other_addr other_rack_id other_fso_id" << std::endl << "\tStarts Tracking process as the controller" << std::endl;
			std::cout << " ts_listen this_rack_id this_fso_id" << std::endl << "\tStarts Tracking process as the listener" << std::endl;
			std::cout << " haa_control this_rack_id this_fso_id other_addr other_rack_id other_fso_id" << std::endl << "\tStarts Half Auto Alignment process as the controller" << std::endl;
			std::cout << " haa_listen this_rack_id this_fso_id" << std::endl << "\tStarts Half Auto Alignment process as the listener" << std::endl;
			std::cout << " power rack_id fso_id" << std::endl << "\tGets the receive power of the selected fso" << std::endl;
			std::cout << " enable_map range step out_file" << std::endl << "\tSame as setting -do_map flag and also supplying parameters" << std::endl;
			std::cout << " disable_map" << std::endl << "\tRestores default tracking functionality" << std::endl;
			std::cout << " saa_control this_rack_id this_fso_id other_addr other_rack_id other_fso_id" << std::endl << "\tStarts SFP Auto Alignment" << std::endl;
			std::cout << " sfp_enable_map range step out_file" << std::endl << "\tSame as setting -map_sfp flag and also supplying parameters" << std::endl;
			std::cout << " sfp_auto_align" << std::endl << "\tRuns the automatic align process using the RSSI from the SFP" << std::endl;			
		} else if(command == "new_fso") {
			std::string filename,rack_id,fso_id,gm1_usb_id,gm2_usb_id;
			int gm1_usb_channel,gm2_usb_channel;
			sstr >> filename >> rack_id >> fso_id >> gm1_usb_id >> gm1_usb_channel >> gm2_usb_id >> gm2_usb_channel;
			newFSO(filename,rack_id,fso_id,gm1_usb_id,gm1_usb_channel,gm2_usb_id,gm2_usb_channel);
		} else if(command == "new_link") {
			std::string this_rack_id,this_fso_id,other_rack_id,other_fso_id;
			int gm1_val,gm2_val;
			sstr >> this_rack_id >> this_fso_id >> other_rack_id >> other_fso_id >> gm1_val >> gm2_val;
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				fso->addNewLink(other_rack_id,other_fso_id,gm1_val,gm2_val);
				fso->save();
			}
		} else if (command == "set_link") {
			std::string this_rack_id,this_fso_id,other_rack_id,other_fso_id;
			sstr >> this_rack_id >> this_fso_id >> other_rack_id >> other_fso_id;
			FSO* fso = getFSO(this_rack_id, this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				if(!fso->setToLink(other_rack_id,other_fso_id)) {
					std::cerr << "Invalid link selected" << std::endl;
				}
			}
		} else if(command == "flip_links") {
			std::string this_rack_id,this_fso_id,other_rack_id1,other_fso_id1, other_rack_id2,other_fso_id2;
			int n_flips = 0;
			float delay = 0.0;
			sstr >> this_rack_id >> this_fso_id >> other_rack_id1 >> other_fso_id1 >> other_rack_id2 >> other_fso_id2 >> n_flips >> delay;
			FSO* fso = getFSO(this_rack_id, this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				for(int i = 0; i < n_flips; ++i) {
					if(!fso->setToLink(other_rack_id1, other_fso_id1)) {
						std::cerr << "Invalid link selected" << std::endl;
						break;
					}
					usleep((delay * 1000000.0));

					if(!fso->setToLink(other_rack_id2, other_fso_id2)) {
						std::cerr << "Invalid link selected" << std::endl;
						break;
					}
					usleep((delay * 1000000.0));
				}
			}
		} else if(command == "wiggle_fso") {
			std::string rack_id,fso_id;
			sstr >> rack_id >> fso_id;
			FSO* fso = getFSO(rack_id,fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				int v1 = fso->getGM1Val();
				for(int i = 0; i < 20; ++i) {
					fso->setGM1Val(v1 + (i % 2 == 0 ? delta : -delta));
					// sleep(1);
				}
				fso->setGM1Val(v1);

				int v2 = fso->getGM2Val();
				for(int i = 0; i < 20; ++i) {
					fso->setGM2Val(v2 + (i % 2 == 0 ? delta : -delta));
					// sleep(1);
				}
				fso->setGM2Val(v2);
			}
		} else if(command == "set_delta") {
			int new_delta;
			sstr >> new_delta;
			if(new_delta > 0) {
				delta = new_delta;
			}
		} else if(command == "modify") {
			std::string this_rack_id,this_fso_id,other_rack_id,other_fso_id;
			sstr >> this_rack_id >> this_fso_id >> other_rack_id >> other_fso_id;
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				modify(fso,other_rack_id,other_fso_id);
			}
		} else if (command == "mod") {
			if(fsos.size() != 1) {
				std::cerr << "Shorthand only works with exactly 1 FSO!!!" << std::endl;
			} else {
				FSO* fso = fsos[0];
				if(fso->getNumberOfLinks() != 1){
					std::cerr << "Shorthand only works with an FSO with exactly 1 link!!!!" << std::endl;
				} else {
					std::string other_rack_id = "", other_fso_id = "";
					fso->getOnlyLink(&other_rack_id, &other_fso_id);
					modify(fso, other_rack_id, other_fso_id);
				}
			}
		} else if(command == "save_fso") {
			std::string rack_id,fso_id;
			sstr >> rack_id >> fso_id;
			FSO* fso = getFSO(rack_id,fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				fso->save();
			}
		} else if(command == "reload_fso") {
			std::string rack_id,fso_id;
			sstr >> rack_id >> fso_id;
			FSO* fso = getFSO(rack_id,fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				fso->load();
			}
		} else if(command == "set_val") {
			std::string rack_id,fso_id;
			int v1,v2;
			sstr >> rack_id >> fso_id >> v1 >> v2;
			FSO* fso = getFSO(rack_id,fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
			} else {
				fso->setGM1Val(v1);
				fso->setGM2Val(v2);
			}
		} else if(command == "aa_control") {
		} else if(command == "aac") {
		} else if(command == "aal") {
		} else if(command == "aa_listen") {
		} else if(command == "ts_control") {
			std::string this_rack_id, this_fso_id, other_addr, other_rack_id, other_fso_id;
			sstr >> this_rack_id >> this_fso_id >> other_addr >> other_rack_id >> other_fso_id;
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			// For now get addr from computer but would be nice to get it another way
			TrackingSystem *ts = TrackingSystem::connectTo(8888,other_addr,other_rack_id,other_fso_id);
			if(ts == NULL) {
				std::cerr << "Unable to start TrackingSystem Process" << std::endl;
			} else {
				ts->run(fso, args);
			}
		} else if(command == "tsc") {
			std::string this_rack_id = "rack_1", this_fso_id = "fso_1", other_addr = OTHER_HOST, other_rack_id = "rack_2", other_fso_id = "fso_1";
			std::cout << other_addr << std::endl;
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			// For now get addr from computer but would be nice to get it another way
			TrackingSystem *ts = TrackingSystem::connectTo(8888,other_addr,other_rack_id,other_fso_id);
			if(ts == NULL) {
				std::cerr << "Unable to start Tracking System Process" << std::endl;
			} else {
				ts->run(fso, args);
			}
		} else if(command == "tsl") {
			std::string this_rack_id = "rack_2", this_fso_id = "fso_1";
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			TrackingSystem *ts = TrackingSystem::listenFor(8888,this_rack_id,this_fso_id);
			if(ts == NULL) {
				std::cerr << "Unable to start Auto Alignment Process" << std::endl;
			} else {
				ts->run(fso, args);
			}
		} else if(command == "ts_listen") {
			std::string this_rack_id, this_fso_id;
			sstr >> this_rack_id >> this_fso_id;
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			TrackingSystem *ts = TrackingSystem::listenFor(8888,this_rack_id,this_fso_id);
			if(ts == NULL) {
				std::cerr << "Unable to start Auto Alignment Process" << std::endl;
			} else {
				ts->run(fso, args);
			}
		} else if(command == "kp") {
			float t;
			sstr >> t;
			args->k_proportional = t;
		} else if(command == "haa_control") {
			std::string this_rack_id, this_fso_id, other_addr, other_rack_id, other_fso_id;
			sstr >> this_rack_id >> this_fso_id >> other_addr >> other_rack_id >> other_fso_id;
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			// For now get addr from computer but would be nice to get it another way
			HalfAutoAlign *haa = HalfAutoAlign::connectTo(8888,other_addr,other_rack_id,other_fso_id);
			if(haa == NULL) {
				std::cerr << "Unable to start HalfAutoAlign Process" << std::endl;
			} else {
				haa->run(fso, args);
			}
		} else if(command == "haac") {
			std::string this_rack_id = "rack_2", this_fso_id = "fso_1", other_addr = OTHER_HOST, other_rack_id = "rack_1", other_fso_id = "fso_1";
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			// For now get addr from computer but would be nice to get it another way
			HalfAutoAlign *ha = HalfAutoAlign::connectTo(8888,other_addr,other_rack_id,other_fso_id);
			if(ha == NULL) {
				std::cerr << "Unable to start HalfAutoAlign Process" << std::endl;
			} else {
				ha->run(fso, args);
			}
		} else if(command == "haal") {
			std::string this_rack_id = "rack_1", this_fso_id = "fso_1";
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			HalfAutoAlign *ha = HalfAutoAlign::listenFor(8888,this_rack_id,this_fso_id);
			if(ha == NULL) {
				std::cerr << "Unable to start HalfAutoAlign Process" << std::endl;
			} else {
				ha->run(fso, args);
			}
		} else if(command == "haa_listen") {
			std::string this_rack_id, this_fso_id;
			sstr >> this_rack_id >> this_fso_id;
			FSO* fso = getFSO(this_rack_id,this_fso_id);
			if(fso == NULL) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}
			HalfAutoAlign *ha = HalfAutoAlign::listenFor(8888,this_rack_id,this_fso_id);
			if(ha == NULL) {
				std::cerr << "Unable to start HalfAutoAlign Process" << std::endl;
			} else {
				ha->run(fso, args);
			}
		} else if(command == "enable_map") {
			int map_range = 0, map_step = 0;
			std::string map_out_file = "";
			sstr >> map_range >> map_step >> map_out_file;
			args->do_map_voltage = true;
			args->map_range = map_range;
			args->map_step = map_step;
			args->map_voltage_out_file = map_out_file;
		} else if(command == "disable_map") {
			args->do_map_voltage = false;
		} else if(command == "saa_control"){
			std::string this_rack_id, this_fso_id, other_addr, other_rack_id, other_fso_id;
			sstr >> this_rack_id >> this_fso_id >> other_addr >> other_rack_id >> other_fso_id;
			FSO* fso = getFSO(this_rack_id, this_fso_id);
			if(fso == nullptr) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}

			SFPAutoAligner *saa = SFPAutoAligner::connectTo(8888, SFPAutoAligner::SockType::UDP, other_addr, other_rack_id, other_fso_id);
			if(saa == nullptr) {
				std::cerr << "Unable to start SFP Auto Alignment" << std::endl;
			} else {
				saa->run(args, fso, other_rack_id, other_fso_id);
			}
			delete saa;
		} else if(command == "saa"){
			std::string this_rack_id = "rack_1", this_fso_id = "fso_1", other_addr = OTHER_HOST, other_rack_id = "rack_2", other_fso_id = "fso_1";
			FSO* fso = getFSO(this_rack_id, this_fso_id);
			if(fso == nullptr) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}

			SFPAutoAligner *saa = SFPAutoAligner::connectTo(8888, SFPAutoAligner::SockType::TCP, other_addr, other_rack_id, other_fso_id);
			if(saa == nullptr) {
				std::cerr << "Unable to start SFP Auto Alignment" << std::endl;
			} else {
				saa->run(args, fso, other_rack_id, other_fso_id);
			}
			delete saa;
		} else if(command == "saal"){
			std::string this_rack_id = "rack_2", this_fso_id = "fso_1", other_rack_id = "rack_1", other_fso_id = "fso_1";
			FSO* fso = getFSO(this_rack_id, this_fso_id);
			if(fso == nullptr) {
				std::cerr << "Invalid FSO selected" << std::endl;
				continue;
			}

			SFPAutoAligner *saa = SFPAutoAligner::listenFor(8888, this_rack_id, this_fso_id);
			if(saa == nullptr) {
				std::cerr << "Unable to start SFP Auto Alignment" << std::endl;
			} else {
				saa->run(args, fso, other_rack_id, other_fso_id);
			}
			delete saa;
		} else if(command == "sfp_enable_map") {
			int map_range = 0, map_step = 0;
			std::string map_out_file = "";
			sstr >> map_range >> map_step >> map_out_file;
			args->sfp_map_power = true;
			args->sfp_map_range = map_range;
			args->sfp_map_step = map_step;
			args->sfp_map_out_file = map_out_file;
		} else if(command == "sfp_disable_map") {
			args->sfp_map_power = false;
		} else if(command == "quit" || command == "exit") {
			break;
		} else {
			std::cerr << "Invalid command: " << command << std::endl << "Use help to list all commands" << std::endl;
		}

	}
}

void CoarseAligner::gm1_incr(FSO* fso) {
	fso->changeGMVal(1,delta);
}

void CoarseAligner::gm1_decr(FSO* fso) {
	fso->changeGMVal(1,-delta);
}

void CoarseAligner::gm2_incr(FSO* fso) {
	fso->changeGMVal(2,delta);
}

void CoarseAligner::gm2_decr(FSO* fso) {
	fso->changeGMVal(2,-delta);
}

void CoarseAligner::printModifyHelp() const {
	std::cout << "Arrow Keys or WASD - modify GM position" << std::endl;
	std::cout << "v - print current delta value" << std::endl;
	std::cout << "z - halve current delta value" << std::endl;
	std::cout << "x - double current delta value" << std::endl;
	std::cout << "g - print current GM positions" << std::endl;
	std::cout << "k - save current GM positions" << std::endl;
	std::cout << "l - reload GM positions from file" << std::endl;
	std::cout << "q - quit modify mode" << std::endl;
	std::cout << "h - display help again" << std::endl;
}

void CoarseAligner::modify(FSO* fso,const std::string &rid,const std::string &fid) {
	if(!fso->setToLink(rid,fid)) {
		std::cerr << "Invalid link selected" << std::endl;
		return;
	}
	printModifyHelp();
	int key = getch();
	std::string error = "";
	while(key != (int)'q') {
		// Arrow keys are triple of the form (27,91,65-68)
		if(key == 27) {
			key = getch();
			if(key == 91) {
				key = getch();
				if(key == 65) {
					// UP
					gm1_incr(fso);
				} else if(key == 66) {
					// DOWN
					gm1_decr(fso);
				} else if(key == 67) {
					// RIGHT
					gm2_incr(fso);
				} else if(key == 68) {
					// LEFT
					gm2_decr(fso);
				} else {
					error = "Invalid Input!";
					break;
				}
			} else {
				error = "Invalid Input!";
				break;
			}
		} else if(key == (int)'w') {
			gm1_incr(fso);
		} else if(key == (int)'s') {
			gm1_decr(fso);
		} else if(key == (int)'d') {
			gm2_incr(fso);
		} else if(key == (int)'a') {
			gm2_decr(fso);
		} else if(key == (int)'v') {
			std::cout << "Current delta value is " << delta << std::endl;
		} else if(key == (int)'z') {
			std::cout << "Delta went from " << delta;
			delta = delta / 2;
			if(delta < 1) {
				delta = 1;
			}
			std::cout << " to " << delta << std::endl;
		} else if(key == (int)'x') {
			std::cout << "Delta went from " << delta;
			delta = delta * 2;
			std::cout << " to " << delta << std::endl;
		} else if(key == (int)'g') {
			// Print current GM positions
			std::cout << "\tGM1 " << fso->getGM1Val() << "  \tGM2 " << fso->getGM2Val() << std::endl;
		} else if(key == (int)'k') {
			fso->saveCurrentSettings(rid, fid);
			fso->save();
			std::cout << "Saved Successfully" << std::endl;
		} else if(key == (int)'l') {
			fso->load();
			if(!fso->setToLink(rid,fid)) {
				std::cerr << "Link not in file" << std::endl;
				return;
			}
			std::cout << "Loaded Sucessfully" << std::endl;
		} else if(key == (int)'h') {
			printModifyHelp();
		}

		key = getch();
	}
	if(error != "") {
		std::cerr << error << std::endl;
		return;
		// There was an error, and the var error is the error message
	}
	if(!args->manual_save) {
		fso->saveCurrentSettings(rid,fid);
		fso->save();
		std::cout << "FSO saved successfully" << std::endl;
	}
}
