
#include "logger.h"

#include <sstream>
#include <chrono>
#include <iomanip>

std::ofstream Logger::ofstr;
int Logger::verbose_level = 0;
bool Logger::log_stderr = false;

std::string Logger::Now() {
	// std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	// std::time_t now_c = std::chrono::system_clock::to_time_t(now);

	std::stringstream sstr;
	// TODO inlucde millisecond information when printing time.
	// sstr << std::put_time(std::localtime(&now_c), "%m-%d-%y_%H:%M:%S");
	sstr << "NO";

	return sstr.str();
}

void Logger::init(const std::string &filename, int verbose_level_, bool log_stderr_) {
	ofstr.open(filename, std::ofstream::out);

	if(verbose_level_ >= 0) {
		verbose_level = verbose_level_;
	} else {
		verbose_level = 0;
	}

	log_stderr = log_stderr_;
}

void Logger::log(const std::string &message, const std::string &file, int line_number, int message_level) {
	if (message_level <= verbose_level) {
		ofstr << file << ":" << line_number << " " << Logger::Now() << " | " << message << std::endl;
		if(log_stderr) {
			std::cerr << file << ":" << line_number << " " << Logger::Now() << " | " << message << std::endl;
		}
	}

}

