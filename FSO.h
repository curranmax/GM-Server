
#ifndef _FSO_H_
#define _FSO_H_

#include "dom_fetcher.h"
#include "args.h"

#include <gm.h>
#include <diode.h>

#include <string>
#include <map>
#include <vector>
#include <stdlib.h>

// Maps a (rack_id,fso_id) pair to the settings for gm1 and gm2 respectively
typedef std::map<std::pair<std::string,std::string>,std::pair<int,int> > ls_map;

class FSO {
  public:
  	static std::vector<float> durs;
  	static void addDur(float v) { durs.push_back(v); }
  	static void printAvgDur() {
  		float sum = 0;
  		for(unsigned int i = 0; i < durs.size(); ++i) {
  			sum += durs[i];
  		}
  		std::cout << sum / durs.size() << std::endl;
  	}

	FSO(const std::string &fso_dec_fname, Args* args_);
	FSO(const std::string &filename,
		const std::string &rack_id_, const std::string &fso_id_,
		const std::string &gm1_usb_id, int gm1_usb_channel,
		const std::string &gm2_usb_id, int gm2_usb_channel,
		Args* args_);
	~FSO();

	bool setToLink(const std::string &rack_id,const std::string &fso_id);
	void saveCurrentSettings(const std::string &rack_id,const std::string &fso_id);

	std::string getFSOID() const { return fso_id; }
	std::string getRackID() const { return rack_id; }
	std::string getUSBID(int gm_n) const;
	int getUSBChannel(int gm_n) const;
	std::vector<std::string> getLinkStrings() const;

	int getGM1Val() const { return gm1->getValue(); }
	int getGM2Val() const { return gm2->getValue(); }

	void setGM1Val(int v) { gm1->setValue(v); }
	void setGM2Val(int v) { gm2->setValue(v); }

	int getHorizontalGMVal() const { return horizontal_gm->getValue(); }
	int getVerticalGMVal() const { return vertical_gm->getValue(); }

	void setHorizontalGMVal(int v) { horizontal_gm->setValue(v); }
	void setVerticalGMVal(int v) { vertical_gm->setValue(v); }

	float getPositiveHorizontalDiodeVoltage() const { return ph_diode->getVoltage(); }
	float getNegativeHorizontalDiodeVoltage() const { return nh_diode->getVoltage(); }
	float getPositiveVerticalDiodeVoltage() const { return pv_diode->getVoltage(); }
	float getNegativeVerticalDiodeVoltage() const { return nv_diode->getVoltage(); }

	float getPowerDiodeVoltage() const { return power_diode->getVoltage(); }

	void changeGMVal(int gm_n,int delta);

	void addNewLink(const std::string &other_rack_id,const std::string &other_fso_id,int val1,int val2);

	void load();
	void save();

	// Fetch the power from the DOM
	float getPower() const;
	void startAutoAlign();
	void endAutoAlign();

  private:
  	GM* makeGM(int channel, const std::string &usb_id,bool debug);
  	Diode* makeDiode(int channel, const std::string& serial_number,
  						int num_samples, int sampling_rate, bool debug);

  	std::string fname;

	std::string rack_id, fso_id;
	std::string switch_address, switch_username, switch_password, port_name;

	GM *gm1, *gm2;
	GM *horizontal_gm, *vertical_gm;
	Diode *ph_diode, *nh_diode, *pv_diode, *nv_diode;
	Diode *power_diode;
	ls_map link_settings;

	DOMFetcher *dom;

	Args* args;
};

#endif
