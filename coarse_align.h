
#ifndef _COARSE_ALIGNER_H_
#define _COARSE_ALIGNER_H_

#include "args.h"
#include "FSO.h"

class CoarseAligner {
public:
	CoarseAligner(const std::vector<FSO*> fsos_,Args *args_) : args(args_),fsos(fsos_),delta(1) {} 
	~CoarseAligner() {}

	void run();

	void modify(FSO* fso,const std::string &rid,const std::string &fid);
	void printModifyHelp() const;

	void gm1_incr(FSO* fso);
	void gm1_decr(FSO* fso);
	void gm2_incr(FSO* fso);
	void gm2_decr(FSO* fso);

	float getPower(FSO* fso);

	FSO* getFSO(std::string rack_id,std::string fso_id) const;

	void listFSOs() const;
	void newFSO(const std::string &filename,const std::string &rack_id,const std::string &fso_id,const std::string &gm1_usb_id,int gm1_usb_channel,const std::string &gm2_usb_id,int gm2_usb_channel);
private:
	Args *args;
	std::vector<FSO*> fsos;

	int delta;
};

#endif
