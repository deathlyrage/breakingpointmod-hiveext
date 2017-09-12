/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once

#include <cstdlib>
#include <iostream>
#include <thread>
#include <memory>

#include "Poco/Dynamic/Var.h"
#include "Poco/JSON/Parser.h"

#include <Poco/MD5Engine.h>
#include <Poco/DigestEngine.h>

#include <Poco/AbstractCache.h>
#include <Poco/DateTime.h>
#include <Poco/ExpireCache.h>

#include <Poco/Net/HTTPClientSession.h>

//#include <boost/thread/thread.hpp>
#include <Poco/Data/SessionPool.h>

#include <mutex>

class VAC
{
	public:
		void init(std::string api_key);
		static std::string convertSteamIDtoBEGUID(const std::string &steamid);

		bool checkVAC(std::string &steam_id);

	private:
		std::string api_key;
		std::unique_ptr<Poco::Net::HTTPClientSession> session;

		struct SteamVacInfo
		{
			std::string steamID;
			bool VACBanned;
			int NumberOfVACBans;
			int DaysSinceLastBan;
		};

		bool isNumber(const std::string &input_str);
		

		std::mutex vacMutex;

		Poco::Dynamic::Var *json;
		Poco::JSON::Parser parser;

		int response = -1;
};
