#include "Utils.h"



void Logger::log(std::string message, LoggingSeverity severity)
{
#ifdef NDEBUG
	return;
#endif

	switch (severity) {
		case LoggingSeverity::Info:
			s_buffer << ">> " << message << std::endl;
			break;
		case LoggingSeverity::Warning:
			s_buffer << ">> [WARNING]: " << message << std::endl;
			break;
		case LoggingSeverity::Error:
			s_buffer << ">> [ERROR]: " << message << std::endl;
			break;
		default:
			break;
	}

	s_updated = true;
}
