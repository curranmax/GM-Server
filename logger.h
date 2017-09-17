
#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <iostream>
#include <fstream>

#define LOG(message) Logger::log(message, __FILE__, __LINE__, 0)
#define VLOG(message, v) Logger::log(message, __FILE__, __LINE__, v)

class Logger {
  public:
	static void init(const std::string &filename, int verbose_level_, bool log_stderr_);
	static void log(const std::string &message, const std::string &file, int line_number, int message_level);

	static std::string Now();
  private:
	Logger();
	~Logger();
	
	static std::ofstream ofstr;
	static int verbose_level;
	static bool log_stderr;
};


#endif
