#include "ArmaConsole.h"

namespace
{
	struct FinderData
	{
		FinderData(DWORD wntProcId)
			: wantedProcessId(wntProcId), targetConsole(nullptr), targetRichEdit(nullptr) {}

		DWORD wantedProcessId;
		HWND targetConsole;
		HWND targetRichEdit;
	};
	BOOL CALLBACK FindConsoleWndProc(HWND hWnd, LPARAM someParam)
	{
		FinderData* data = reinterpret_cast<FinderData*>(someParam);

		DWORD thisProcId;
		GetWindowThreadProcessId(hWnd, &thisProcId);
		if (thisProcId == data->wantedProcessId)
		{
			char classNameStr[256];
			memset(classNameStr, 0, sizeof(classNameStr));
			GetClassNameA(hWnd, classNameStr, sizeof(classNameStr) - 1);
			if (!_stricmp(classNameStr, "#32770")) //Dialog
			{
				data->targetConsole = hWnd;
				return false; //stops enumeration
			}
		}

		//continue enumeration
		return true;
	}
	BOOL CALLBACK FindRichEditProc(HWND hWnd, LPARAM someParam)
	{
		FinderData* data = reinterpret_cast<FinderData*>(someParam);

		char classNameStr[256];
		memset(classNameStr, 0, sizeof(classNameStr));
		GetClassNameA(hWnd, classNameStr, sizeof(classNameStr) - 1);
		if (!_stricmp(classNameStr, "RichEdit20A"))
		{
			data->targetRichEdit = hWnd;
			return false; //stops enumeration
		}

		return false;
	}
}

#include <Richedit.h>
#include <CommDlg.h>

ArmaConsole::ArmaConsole(std::string serverFolder) : _wndRich(nullptr)
{
	//find the rich edit control window in our process
	FinderData fndData(GetProcessId(GetCurrentProcess()));
	EnumWindows(FindConsoleWndProc, reinterpret_cast<LPARAM>(&fndData));
	if (fndData.targetConsole != nullptr)
	{
		EnumChildWindows(fndData.targetConsole, FindRichEditProc, reinterpret_cast<LPARAM>(&fndData));
		if (fndData.targetRichEdit != nullptr)
			_wndRich = fndData.targetRichEdit;
	}

	//setup logging engine
	Poco::AutoPtr<Poco::SplitterChannel> splitChan(new Poco::SplitterChannel);
	Poco::AutoPtr<Poco::FileChannel> pChannel(new Poco::FileChannel);
	pChannel->setProperty("path", serverFolder + "/BreakingPointExt.log");
	pChannel->setProperty("rotation", "never");
	pChannel->setProperty("archive", "timestamp");
	pChannel->setProperty("times", "local");

	Poco::AutoPtr<Poco::PatternFormatter> fileFormatter(new Poco::PatternFormatter);
	//fileFormatter->setProperty("pattern","%Y-%m-%d %H:%M:%S %s: [%p] %t");
	fileFormatter->setProperty("pattern", "%Y-%m-%d %H:%M:%S %s: %t");
	fileFormatter->setProperty("times", "local");

	Poco::AutoPtr<Poco::FormattingChannel> fileFormatChan(new Poco::FormattingChannel(fileFormatter, pChannel));
	splitChan->addChannel(fileFormatChan);

	Poco::Logger::root().setChannel(splitChan);
	Poco::Logger::root().setLevel(Poco::Message::PRIO_DEBUG);

	logEngine = &Poco::Logger::get("BreakingPointExt");
}

ArmaConsole::~ArmaConsole()
{
}

void ArmaConsole::log(const std::string& msg, LogType type)
{
	//Dont Log Debug Messages to GUI Window
	if (type == LogType::DEBUG)
	{
		logEngine->debug(msg);
		return;
	}

	if (_wndRich == nullptr)
		return;

	std::string text = msg + "\r\n";
	if (text[0] == '0') //remove prefix from hour to match arma
		text = " " + text.substr(1);

	static const size_t MAX_LINE_COUNT = 500;
	size_t lineCount = SendMessageW(_wndRich, EM_GETLINECOUNT, 0, 0);
	int lastPoint = -1;
	while (lineCount > MAX_LINE_COUNT)
	{
		FINDTEXTEXA findInfo;
		//search document from last point
		findInfo.chrg.cpMin = lastPoint + 1;
		findInfo.chrg.cpMax = -1;
		//search for newline
		findInfo.lpstrText = "\r";
		//no-find return
		findInfo.chrgText.cpMin = -1;
		findInfo.chrgText.cpMax = -1;

		int foundRes = SendMessageW(_wndRich, EM_FINDTEXTEX, FR_DOWN | FR_MATCHCASE, reinterpret_cast<LPARAM>(&findInfo));
		if (foundRes == -1)
			break;

		lastPoint = findInfo.chrgText.cpMax;
		lineCount--;
	}

	//prevent redraws
	SendMessage(_wndRich, WM_SETREDRAW, false, 0);

	if (lastPoint > 0)
	{
		//Select found text
		SendMessageW(_wndRich, EM_SETSEL, 0, lastPoint);
		//Remove selected text by replacing it with nothing
		SendMessageW(_wndRich, EM_REPLACESEL, false, reinterpret_cast<LPARAM>(L""));
	}

	//set selection to end
	SendMessageW(_wndRich, EM_SETSEL, -1, -1);

	//change the colour
	{
		//default information colour
		COLORREF newColour = RGB(0x33, 0x99, 0x33);
		CHARFORMATW colourData;
		memset(&colourData, 0, sizeof(colourData));
		colourData.cbSize = sizeof(colourData);
		colourData.dwMask = CFM_COLOR;

		switch (type)
		{
			case LogType::CRITICAL:
			{
				logEngine->critical(msg);

				newColour = RGB(0xFF, 0x00, 0x00);
				colourData.dwMask |= CFM_BOLD;
				colourData.dwEffects = CFE_BOLD;
				break;
			};
			case LogType::WARNING:
			{
				logEngine->warning(msg);

				newColour = RGB(0xFF, 0x99, 0x33);
				break;
			};
			case LogType::DEBUG:
			{
				newColour = RGB(0xFF, 0x00, 0x00);
				colourData.dwMask |= CFM_BOLD;
				colourData.dwEffects = CFE_BOLD;

				//logEngine->debug(msg);
				//newColour = RGB(0xBB, 0xBB, 0xBB);
				break;
			};
		};

		colourData.crTextColor = newColour;

		SendMessageW(_wndRich, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&colourData));
	}

	//insert utf8 text
	{
		SETTEXTEX setText;
		setText.flags = ST_SELECTION;
		setText.codepage = 65001;

		SendMessageW(_wndRich, EM_SETTEXTEX, reinterpret_cast<WPARAM>(&setText), reinterpret_cast<LPARAM>(text.c_str()));
	}

	//enable redraws
	SendMessage(_wndRich, WM_SETREDRAW, true, 0);

	//scroll to end
	SendMessageW(_wndRich, EM_SETSEL, -1, -1);
	SendMessageW(_wndRich, EM_SCROLLCARET, 0, 0);

	//request redraw
	RedrawWindow(_wndRich, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}