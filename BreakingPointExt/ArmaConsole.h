#pragma once

//#include <Windows.h>
#include <string>

#include <Poco/ConsoleChannel.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/SplitterChannel.h>
#include <Poco/AsyncChannel.h>
#include <Poco/String.h>
#include <Poco/Path.h>
#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/Message.h>
#include "Poco/Logger.h"
#include "Poco/SimpleFileChannel.h"
#include "Poco/AutoPtr.h"

class ArmaConsole
{
	public:
		ArmaConsole(std::string serverFolder);
		~ArmaConsole();

		enum class LogType
		{
			DEBUG,
			WARNING,
			CRITICAL
		};

		void log(const std::string& msg, LogType type = LogType::CRITICAL);
	private:
		HWND _wndRich;
		Poco::Logger* logEngine;

};

