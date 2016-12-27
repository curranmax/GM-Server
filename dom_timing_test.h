
#ifndef _DOM_TIMING_TEST_H_
#define _DOM_TIMING_TEST_H_

#include "FSO.h"
#include "args.h"

class DOMTimingTest {
  public:
	DOMTimingTest(const std::vector<FSO*> fsos_,Args *args_) : fsos(fsos_), args(args_) {}
	~DOMTimingTest() {}

	void run();
  private:
  	std::vector<FSO*> fsos;
  	Args *args;
};

#endif
