
#include <string>

#pragma once

enum class LoggingSeverity {
	Info,
	Warning,
	Error
};

void log(std::string message, LoggingSeverity severity = LoggingSeverity::Info);

