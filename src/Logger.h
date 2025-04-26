#include <string>  
#include <iostream> 
#include <sstream>   

#pragma once

enum class LoggingSeverity {
	Info,
	Warning,
	Error
};

class Logger {
public:
	static void log(std::string message, LoggingSeverity severity = LoggingSeverity::Info);
	inline static std::stringstream s_buffer;
	inline static bool s_updated = false;
};



