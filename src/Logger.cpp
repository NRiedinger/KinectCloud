#include "Logger.h"

#include <iostream>

void log(std::string message, LoggingSeverity severity)
{
#ifdef NDEBUG
	return;
#endif

	switch (severity) {
		case LoggingSeverity::Info:
			std::cout << ">> " << message << std::endl;
			break;
		case LoggingSeverity::Warning:
			std::cout << ">> [WARNING]: " << message << std::endl;
			break;
		case LoggingSeverity::Error:
			std::cerr << ">> [ERROR]: " << message << std::endl;
			break;
		default:
			break;
	}
}
