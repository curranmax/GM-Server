
#include "logger.h"

std::ofstream Logger::ofstr;
int Logger::verbose_level = 0;
bool Logger::log_stderr = false;

void Logger::init(const std::string &filename, int verbose_level_, bool log_stderr_) {
	ofstr.open(filename, std::ofstream::out);

	if(verbose_level_ <= 0) {
		verbose_level = verbose_level_;
	} else {
		verbose_level = 0;
	}

	log_stderr = log_stderr_;
}

void Logger::log(const std::string &message, const std::string &file, int line_number, int message_level) {
	// TODO inlucde millisecond information when printing time.

	if (message_level <= verbose_level) {
		ofstr << file << ":" << line_number << " " << __TIME__ << " | " << message << std::endl;
	}

	if(log_stderr) {
		std::cerr << file << ":" << line_number << " " << __TIME__ << " | " << message << std::endl;
	}
}

