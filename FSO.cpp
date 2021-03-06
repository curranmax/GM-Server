
#include "FSO.h"
#include "debug_gm.h"
#include "no_gm.h"
#include "no_diode.h"

#include <gm.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

std::vector<float> FSO::durs;

FSO::FSO(const std::string &fso_dec_fname, Args* args_) {
	fname = fso_dec_fname;

	rack_id = "";
	fso_id = "";

	gm1 = nullptr;
	gm2 = nullptr;
	horizontal_gm = nullptr;
	vertical_gm = nullptr;
	ph_diode = nullptr;
	nh_diode = nullptr;
	pv_diode = nullptr;
	nv_diode = nullptr;
	power_diode = nullptr;

	link_settings = ls_map();

	args = args_;

	load();
}

FSO::FSO(const std::string &filename,const std::string &rack_id_,const std::string &fso_id_,const std::string &gm1_usb_id,int gm1_usb_channel,const std::string &gm2_usb_id,int gm2_usb_channel,Args* args_) {
	fname = filename;
	rack_id = rack_id_;
	fso_id = fso_id_;

	horizontal_gm = nullptr;
	vertical_gm = nullptr;
	ph_diode = nullptr;
	nh_diode = nullptr;
	pv_diode = nullptr;
	nv_diode = nullptr;
	power_diode = nullptr;

	link_settings = ls_map();

	args = args_;

	gm1 = makeGM(gm1_usb_channel,gm1_usb_id,args->debug);
	gm2 = makeGM(gm2_usb_channel,gm2_usb_id,args->debug);

	gm1->connectDevice();
	gm2->connectDevice();
}

FSO::~FSO() {
	if(gm1 != NULL) {
		gm1->disconnectDevice();
		delete gm1;
	}
	if(gm2 != NULL) {
		gm2->disconnectDevice();
		delete gm2;
	}
}

void FSO::changeGMVal(int gm_n,int delta) {
	GM* gm;
	if(gm_n == 1) {
		gm = gm1;
	} else if(gm_n == 2) {
		gm = gm2;
	} else {
		std::cerr << "Invalid gm choice" << std::endl;
		exit(0);
	}

	int new_v = gm->getValue() + delta;
	gm->setValue(new_v);
}

int FSO::getUSBChannel(int gm_n) const {
	if(gm_n == 1) {
		return gm1->getChannel();
	} else if(gm_n == 2) {
		return gm2->getChannel();
	} else {
		std::cerr << "Invalid input" << std::endl;
		exit(1);
	}
}

std::string FSO::getUSBID(int gm_n) const {
	if(gm_n == 1) {
		return gm1->getSerialNumber();
	} else if(gm_n == 2) {
		return gm2->getSerialNumber();
	} else {
		std::cerr << "Invalid input" << std::endl;
		exit(1);
	}
}

std::vector<std::string> FSO::getLinkStrings() const {
	std::vector<std::string> rv;
	for(ls_map::const_iterator itr = link_settings.begin(); itr != link_settings.end(); ++itr) {
		std::stringstream sstr;
		sstr << "Link to ("  << itr->first.first << "," << itr->first.second << ") has GM values of (" << itr->second.first << "," << itr->second.second << ")";
		rv.push_back(sstr.str());
	}
	return rv;
}

void FSO::load() {

	std::ifstream ifstr(fname, std::ifstream::in);

	std::string line;
	std::string token;

	std::string gm1_serial_number = "", gm2_serial_number = "";
	int gm1_channel = -1, gm2_channel = -1;

	int horizontal_gm_num = -1, vertical_gm_num = -1;

	std::string ph_diode_serial_number = "";
	int ph_diode_channel = -1,
			ph_diode_num_samples = -1,
			ph_diode_sampling_rate = -1;
	std::string nh_diode_serial_number = "";
	int nh_diode_channel = -1,
			nh_diode_num_samples = -1,
			nh_diode_sampling_rate = -1;
	std::string pv_diode_serial_number = "";
	int pv_diode_channel = -1,
			pv_diode_num_samples = -1,
			pv_diode_sampling_rate = -1;
	std::string nv_diode_serial_number = "";
	int nv_diode_channel = -1,
			nv_diode_num_samples = -1,
			nv_diode_sampling_rate = -1;

	std::string power_diode_string = "";
	std::string power_diode_serial_number = "";
	int power_diode_channel = -1,
			power_diode_num_samples = -1,
			power_diode_sampling_rate = -1;
	while(std::getline(ifstr,line)) {
		token = "";
		std::stringstream sstr(line);

		sstr >> token;

		if(token == "#"){
			// Comment, should really be if it starts with # and not just a lone #
			continue;
		} else if(token == "rack_id") {
			sstr >> rack_id;
		} else if(token == "fso_id") {
			sstr >> fso_id;
		} else if(token == "gm1_serial_number") {
			sstr >> gm1_serial_number;
		} else if(token == "gm1_channel") {
			sstr >> gm1_channel;
		} else if(token == "gm2_serial_number") {
			sstr >> gm2_serial_number;
		} else if(token == "gm2_channel") {
			sstr >> gm2_channel;
		} else if(token == "horizontal_gm") {
			sstr >> horizontal_gm_num;
		} else if(token == "vertical_gm") {
			sstr >> vertical_gm_num;
		} else if(token == "link") {
			std::string other_rack_id, other_fso_id;
			int val1,val2;
			sstr >> other_rack_id >> other_fso_id >> val1 >> val2;
			link_settings[std::make_pair(other_rack_id,other_fso_id)] = std::make_pair(val1,val2);
		} else if(token == "ph_diode_serial_number") {
			sstr >> ph_diode_serial_number;
		} else if(token == "ph_diode_channel") {
			sstr >> ph_diode_channel;
		} else if(token == "ph_diode_num_samples") {
			sstr >> ph_diode_num_samples;
		} else if(token == "ph_diode_sampling_rate") {
			sstr >> ph_diode_sampling_rate;
		} else if(token == "nh_diode_serial_number") {
			sstr >> nh_diode_serial_number;
		} else if(token == "nh_diode_channel") {
			sstr >> nh_diode_channel;
		} else if(token == "nh_diode_num_samples") {
			sstr >> nh_diode_num_samples;
		} else if(token == "nh_diode_sampling_rate") {
			sstr >> nh_diode_sampling_rate;
		} else if(token == "pv_diode_serial_number") {
			sstr >> pv_diode_serial_number;
		} else if(token == "pv_diode_channel") {
			sstr >> pv_diode_channel;
		} else if(token == "pv_diode_num_samples") {
			sstr >> pv_diode_num_samples;
		} else if(token == "pv_diode_sampling_rate") {
			sstr >> pv_diode_sampling_rate;
		} else if(token == "nv_diode_serial_number") {
			sstr >> nv_diode_serial_number;
		} else if(token == "nv_diode_channel") {
			sstr >> nv_diode_channel;
		} else if(token == "nv_diode_num_samples") {
			sstr >> nv_diode_num_samples;
		} else if(token == "nv_diode_sampling_rate") {
			sstr >> nv_diode_sampling_rate;
		} else if(token == "power_diode") {
			sstr >> power_diode_string;
		} else if(token == "power_diode_serial_number") {
			sstr >> power_diode_serial_number;
		} else if(token == "power_diode_channel") {
			sstr >> power_diode_channel;
		} else if(token == "power_diode_num_samples") {
			sstr >> power_diode_num_samples;
		} else if(token == "power_diode_sampling_rate") {
			sstr >> power_diode_sampling_rate;
		}
	}

	// Right now there is no way to differentiate devices, but should change that eventually
	gm1 = makeGM(gm1_channel, gm1_serial_number, args->debug);
	gm2 = makeGM(gm2_channel, gm2_serial_number, args->debug);
	if (horizontal_gm_num == 1) {
		horizontal_gm = gm1;
	} else if(horizontal_gm_num == 2) {
		horizontal_gm = gm2;
	} else {
		horizontal_gm = gm1;
	}

	if(vertical_gm_num == 1) {
		vertical_gm = gm1;
	} else if(vertical_gm_num == 2) {
		vertical_gm = gm2;
	} else {
		vertical_gm = gm2;
	}

	ph_diode = makeDiode(ph_diode_channel,
							ph_diode_serial_number,
							ph_diode_num_samples,
							ph_diode_sampling_rate,
							args->debug);
	nh_diode = makeDiode(nh_diode_channel,
							nh_diode_serial_number,
							nh_diode_num_samples,
							nh_diode_sampling_rate,
							args->debug);
	pv_diode = makeDiode(pv_diode_channel,
							pv_diode_serial_number,
							pv_diode_num_samples,
							pv_diode_sampling_rate,
							args->debug);
	nv_diode = makeDiode(nv_diode_channel,
							nv_diode_serial_number,
							nv_diode_num_samples,
							nv_diode_sampling_rate,
							args->debug);

	power_diode = makeDiode(power_diode_channel,
							power_diode_serial_number,
							power_diode_num_samples,
							power_diode_sampling_rate,
							args->debug);


	// TODO add err if power_diode != nullptr and power_diode_str != ""

	if(power_diode == nullptr) {
		if (power_diode_string == "ph") {
			power_diode = ph_diode;
		}
		if (power_diode_string == "nh") {
			power_diode = nh_diode;
		}
		if (power_diode_string == "pv") {
			power_diode = pv_diode;
		}
		if (power_diode_string == "nv") {
			power_diode = nv_diode;
		}
	}

	horizontal_gm->connectDevice();
	vertical_gm->connectDevice();

	ph_diode->connectDevice();
	nh_diode->connectDevice();
	pv_diode->connectDevice();
	nv_diode->connectDevice();

	power_diode->connectDevice();

	// If only one link, then set it to that
	if(link_settings.size() == 1) {
		for(ls_map::const_iterator itr = link_settings.begin(); itr != link_settings.end(); ++itr) {
			setToLink(itr->first.first, itr->first.second);
			break;
		}
	}
}

void FSO::save() {
	std::ofstream ofstr(fname, std::ofstream::out);

	// ID
	ofstr << "rack_id " << rack_id << std::endl << "fso_id " << fso_id << std::endl << std::endl;
	
	// GMs
	bool gm_print = false;
	if(gm1 != nullptr && !gm1->isNull()) {
		gm_print = true;
		if(gm1->getSerialNumber() != "") {
			ofstr << "gm1_serial_number " << gm1->getSerialNumber() << std::endl;
		}
		ofstr << "gm1_channel " << gm1->getChannel() << std::endl;
	}
	if(gm2 != nullptr && !gm2->isNull()) {
		gm_print = true;
		if(gm1->getSerialNumber() != "") {
			ofstr << "gm2_serial_number " << gm2->getSerialNumber() << std::endl;
		}
		ofstr << "gm2_channel " << gm2->getChannel() << std::endl;
	}
	if(horizontal_gm != nullptr && !horizontal_gm->isNull()) {
		gm_print = true;
		if(horizontal_gm == gm1) {
			ofstr << "horizontal_gm 1"<< std::endl;
		}
		if(horizontal_gm == gm2) {
			ofstr << "horizontal_gm 2" << std::endl;
		}
	}
	if(vertical_gm != nullptr && !vertical_gm->isNull()) {
		gm_print = true;
		if(vertical_gm == gm1) {
			ofstr << "vertical_gm 1"<< std::endl;
		}
		if(vertical_gm == gm2) {
			ofstr << "vertical_gm 2" << std::endl;
		}
	}
	if(gm_print) {
		ofstr << std::endl;
	}

	// Diodes
	bool diode_print_something = false;
	if (ph_diode != nullptr && !ph_diode->isNull()) {
		diode_print_something = true;
		if(ph_diode->getSerialNumber() != "") {
			ofstr << "ph_diode_serial_number " << ph_diode->getSerialNumber() << std::endl;
		}
		ofstr << "ph_diode_channel " << ph_diode->getChannel() << std::endl;
		ofstr << "ph_diode_num_samples " << ph_diode->getNumSamples() << std::endl;
		ofstr << "ph_diode_sampling_rate " << ph_diode->getSamplingRate() << std::endl;
	}
	if (nh_diode != nullptr && !nh_diode->isNull()) {
		diode_print_something = true;
		if(nh_diode->getSerialNumber() != "") {
			ofstr << "nh_diode_serial_number " << nh_diode->getSerialNumber() << std::endl;
		}
		ofstr << "nh_diode_channel " << nh_diode->getChannel() << std::endl;
		ofstr << "nh_diode_num_samples " << nh_diode->getNumSamples() << std::endl;
		ofstr << "nh_diode_sampling_rate " << nh_diode->getSamplingRate() << std::endl;
	}
	if (pv_diode != nullptr && !pv_diode->isNull()) {
		diode_print_something = true;
		if(pv_diode->getSerialNumber() != "") {
			ofstr << "pv_diode_serial_number " << pv_diode->getSerialNumber() << std::endl;
		}
		ofstr << "pv_diode_channel " << pv_diode->getChannel() << std::endl;
		ofstr << "pv_diode_num_samples " << pv_diode->getNumSamples() << std::endl;
		ofstr << "pv_diode_sampling_rate " << pv_diode->getSamplingRate() << std::endl;
	}
	if (nv_diode != nullptr && !nv_diode->isNull()) {
		diode_print_something = true;
		if(nv_diode->getSerialNumber() != "") {
			ofstr << "nv_diode_serial_number " << nv_diode->getSerialNumber() << std::endl;
		}
		ofstr << "nv_diode_channel " << nv_diode->getChannel() << std::endl;
		ofstr << "nv_diode_num_samples " << nv_diode->getNumSamples() << std::endl;
		ofstr << "nv_diode_sampling_rate " << nv_diode->getSamplingRate() << std::endl;
	}
	if(power_diode != nullptr && !power_diode->isNull()) {
		diode_print_something = true;
		if(power_diode == ph_diode) {
			ofstr << "power_diode ph" << std::endl;
		} else if(power_diode == nh_diode) {
			ofstr << "power_diode nh" << std::endl;
		} else if(power_diode == pv_diode) {
			ofstr << "power_diode pv" << std::endl;
		} else if(power_diode == nv_diode) {
			ofstr << "power_diode nv" << std::endl;
		} else {
			if(power_diode->getSerialNumber() != "") {
				ofstr << "power_diode_serial_number " << power_diode->getSerialNumber() << std::endl;
			}
			ofstr << "power_diode_channel " << power_diode->getChannel() << std::endl;
			ofstr << "power_diode_num_samples " << power_diode->getNumSamples() << std::endl;
			ofstr << "power_diode_sampling_rate " << power_diode->getSamplingRate() << std::endl;
		}
	}
	if(diode_print_something) {
		ofstr << std::endl;
	}

	// Links
	for(ls_map::const_iterator itr = link_settings.begin(); itr != link_settings.end(); ++itr) {
		ofstr << "link "  << itr->first.first << " " << itr->first.second << " " << itr->second.first << " " << itr->second.second << std::endl;
	}
	ofstr << std::endl;
}

void FSO::addNewLink(const std::string &other_rack_id,const std::string &other_fso_id,int val1,int val2) {
	link_settings[std::make_pair(other_rack_id,other_fso_id)] = std::make_pair(val1,val2);
}

bool FSO::setToLink(const std::string &rack_id,const std::string &fso_id) {
	std::pair<std::string,std::string> link_id = std::make_pair(rack_id,fso_id);
	if(link_settings.count(link_id) == 0) {
		return false;
	}
	std::pair<int,int> link_vals = link_settings[link_id];
	gm1->setValue(link_vals.first);
	gm2->setValue(link_vals.second);
	return true;
}

void FSO::saveCurrentSettings(const std::string &rack_id,const std::string &fso_id) {
	int v1 = gm1->getValue();
	int v2 = gm2->getValue();
	link_settings[std::make_pair(rack_id,fso_id)] = std::make_pair(v1,v2);
}

GM* FSO::makeGM(int channel, const std::string &usb_id, bool debug) {
	if(channel < 0) {
		return new NoGM();
	} else {
		if(debug) {
			return new DebugGM(channel,usb_id);
		} else {
			return new GM(channel, usb_id);
		}
	}
}

Diode* FSO::makeDiode(int channel, const std::string& serial_number,
  						int num_samples, int sampling_rate, bool debug) {
	if (channel < 0 || debug) {
		return new NoDiode();
	} else {
		if (num_samples <= 0 && sampling_rate <= 0) {
			return new Diode(channel, serial_number);
		} else {
			return new Diode(channel, serial_number, num_samples, sampling_rate);
		}
	}
}

void FSO::getOnlyLink(std::string* other_rack_id, std::string* other_fso_id) {
	if(getNumberOfLinks() != 1) {
		return;
	}

	for(ls_map::const_iterator itr = link_settings.begin(); itr != link_settings.end(); ++itr) {
		*other_rack_id = itr->first.first;
		*other_fso_id = itr->first.second;
	}
}
