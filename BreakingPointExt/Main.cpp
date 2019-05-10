
//#define DEV

#include "BreakingPointExt.h"

#include <stdlib.h>
#include <iostream>

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <unordered_map>

#include <WinSock2.h>

//#include <windows.h>

#include <stdio.h>
#include <shellapi.h>

#include "Database.h"

#include "ArmaConsole.h"
#include "Rcon.h"

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <Poco/UnicodeConverter.h>

#include <boost/progress.hpp>

extern "C" { 
	__declspec (dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function);
	__declspec (dllexport) void __stdcall RVExtensionVersion(char* output, int outputSize);
}

int serverID = 0;
bool worker_working = false; //worker working status

BreakingPointExt * breakingPointExt = nullptr;
ArmaConsole * console = nullptr;

std::vector<string> GetCmdLineParams(LPTSTR** argv = NULL, int* argc = NULL)
{
	LPTSTR cmdLine = GetCommandLineW();
	int numCmdLineArgs = 0;
	LPTSTR* cmdLineArgs = CommandLineToArgvW(cmdLine, &numCmdLineArgs);
	if (argv)
		*argv = cmdLineArgs;
	if (argc)
		*argc = numCmdLineArgs;

	std::vector<string> result;
	result.reserve(numCmdLineArgs);
	for (int i = 0; i<numCmdLineArgs; i++)
	{
		string utf8Arg;
		Poco::UnicodeConverter::toUTF8(cmdLineArgs[i], utf8Arg);
		result.push_back(utf8Arg);
	}

	if (cmdLineArgs && argv == NULL)
		LocalFree(cmdLineArgs);

	return result;
}

//Extension Startup
void ExtStartup()
{
	std::string serverFolder;
	int serverPort = 2302;

	//Fetch Server Folder Directory
	{
		LPTSTR* argv;
		int argc;
		auto cmdLine = GetCmdLineParams(&argv, &argc);

		//Fetch Server Folder Name
		for (auto it = cmdLine.begin(); it != cmdLine.end(); ++it)
		{
			string starter = "-profiles=";
			if (it->length() < starter.length())
				continue;
			string compareMe = it->substr(0, starter.length());
			if (!boost::iequals(compareMe, starter))
				continue;

			string rest = it->substr(compareMe.length());
			boost::trim(rest);

			serverFolder = rest;
		}

		//Fetch Server Port
		for (auto it = cmdLine.begin(); it != cmdLine.end(); ++it)
		{
			string starter = "-port=";
			if (it->length() < starter.length())
				continue;
			string compareMe = it->substr(0, starter.length());
			if (!boost::iequals(compareMe, starter))
				continue;

			string rest = it->substr(compareMe.length());
			boost::trim(rest);

			//Attempt to Cast Port from Int
			try
			{
				serverPort = boost::lexical_cast<int>(rest);
			}
			catch (boost::bad_lexical_cast)
			{
			}
		}
	}

	console = new ArmaConsole(serverFolder);
	breakingPointExt = new BreakingPointExt(serverFolder,serverPort);
	breakingPointExt->Startup();
};

//Extension Shutdown
void ExtShutdown()
{
	if (breakingPointExt) {
		breakingPointExt->Shutdown();
		delete breakingPointExt;
	}
	if (console) { delete console; };
};

void __stdcall RVExtension(char *output, int outputSize, const char *function)
{
	if (!worker_working)
	{
		worker_working = true;
		breakingPointExt->InitThreads();
	}

	if (!strncmp(function, "a:", 2)) // detect checking for result
	{
		breakingPointExt->callExtensionAsync(std::string(&function[2]),true);
		return;
	}
	else if (!strncmp(function, "z:", 2)) // detect checking for result
	{
		breakingPointExt->callExtensionAsync(std::string(&function[2]),false);
		return;
	}
	else if (!strncmp(function, "r:", 2)) // detect checking for result
	{
		long int num = atol(&function[2]); // ticket number or 0
		if (breakingPointExt->ticketExists(num))
		{
			if (breakingPointExt->ticketReady(num))
			{
				//result
				strncpy_s(output, outputSize, breakingPointExt->ticketResult(num).c_str(), _TRUNCATE);
				return;
			}
			// result is not ready
			strncpy_s(output, outputSize, "WAIT", _TRUNCATE);
			return;
		}
		// no such ticket 
		strncpy_s(output, outputSize, "EMPTY", _TRUNCATE);
		return;
	}
	else if (!strncmp(function, "s:", 2)) // detect ticket submission 
	{
		long int num = breakingPointExt->createTicket(std::string(&function[2]));
		strncpy_s(output, outputSize, std::to_string(num).c_str(), _TRUNCATE); // ticket number
		return;
	}
	else if (!strncmp(function, "d:", 2)) // detect direct blocking call 
	{
		std::string out = breakingPointExt->callExtension(std::string(&function[2]));
		strncpy_s(output, outputSize, out.c_str(), _TRUNCATE);
		return;
	} else {
		strncpy_s(output, outputSize, "INVALID COMMAND", _TRUNCATE); // other input 
	}
}

void __stdcall RVExtensionVersion(char* output, int outputSize)
{
	strncpy_s(output, outputSize, breakingPointExt->versionNum.c_str(), _TRUNCATE);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		ExtStartup();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		ExtShutdown();
		break;
	}
	return TRUE;
}
