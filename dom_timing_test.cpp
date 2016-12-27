
#include "dom_timing_test.h"

void DOMTimingTest::run() {
	int num_iters = args->num_dom_iters;

	for(unsigned int i = 0; i < fsos.size(); ++i) {
		fsos[i]->startAutoAlign();
	}

	for(int j = 0; j < num_iters; ++j) {
		for(unsigned int i = 0; i < fsos.size(); ++i) {
			std::cout << j << " " << i << " " << fsos[i]->getPower() << std::endl;
		}
	}

	for(unsigned int i = 0; i < fsos.size(); ++i) {
		fsos[i]->endAutoAlign();
	}
}
