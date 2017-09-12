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

Code to Convert SteamID -> BEGUID 
From Frank https://gist.github.com/Fank/11127158

*/


#include "VAC.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <Poco/Data/MetaColumn.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Session.h>

#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Path.h>
#include <Poco/URI.h>

#include <Poco/Exception.h>

#include <Poco/NumberParser.h>

#include <Poco/Types.h>
#include <Poco/MD5Engine.h>
#include <Poco/DigestEngine.h>

#include <string>

#include "ArmaConsole.h"
#include "BreakingPointExt.h"

extern ArmaConsole * console;
extern BreakingPointExt * breakingPointExt;

void VAC::init(std::string api_key)
{
	this->api_key = api_key;
	session.reset(new Poco::Net::HTTPClientSession("api.steampowered.com", 80));
}

bool VAC::isNumber(const std::string &input_str)
{
	bool status = true;
	for (unsigned int index=0; index < input_str.length(); index++)
	{
		if (!std::isdigit(input_str[index]))
		{
			status = false;
			break;
		}
	}
	return status;
}

// From Frank https://gist.github.com/Fank/11127158
// Modified to use libpoco
std::string VAC::convertSteamIDtoBEGUID(const std::string &input_str)
{
	Poco::MD5Engine md5;
	Poco::Int64 steamID = Poco::NumberParser::parse64(input_str);
	Poco::Int8 i = 0, parts[8] = { 0 };

	do
	{
		parts[i++] = steamID & 0xFFu;
	} while (steamID >>= 8);

	std::stringstream bestring;
	bestring << "BE";
	for (int i = 0; i < sizeof(parts); i++) {
		bestring << char(parts[i]);
	}

	md5.update(bestring.str());
	return Poco::DigestEngine::digestToHex(md5.digest());
}

bool VAC::checkVAC(std::string &steamID)
{
	SteamVacInfo vac_info;

	Poco::URI uri("http://api.steampowered.com/ISteamUser/GetPlayerBans/v1/?key=" + api_key + "&format=json&steamids=" + steamID);
	Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());

	// prepare path
	std::string path(uri.getPathAndQuery());
	if (path.empty()) path = "/";

	// send request
	Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
	session.sendRequest(req);

	// get response
	Poco::Net::HTTPResponse res;
	std::istream &is = session.receiveResponse(res);
	Poco::Dynamic::Var json = parser.parse(is);

	// check for vac bans
	if (res.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
	{
		Poco::JSON::Object::Ptr json_object = json.extract<Poco::JSON::Object::Ptr>();
		for (const auto &val : *(json_object->getArray("players")))
		{
			Poco::JSON::Object::Ptr sub_json_object = val.extract<Poco::JSON::Object::Ptr>();
			json_object = val.extract<Poco::JSON::Object::Ptr>();

			//SteamId
			if (sub_json_object->has("SteamId"))
			{
				vac_info.steamID = sub_json_object->getValue<std::string>("SteamId");
			} else {
				vac_info.steamID = "";
			}

			//NumberOfVACBans
			if (sub_json_object->has("NumberOfVACBans"))
			{
				vac_info.NumberOfVACBans = sub_json_object->getValue<int>("NumberOfVACBans");
			} else {
				vac_info.NumberOfVACBans = 0;
			}

			//VACBanned
			if (sub_json_object->has("VACBanned"))
			{
				vac_info.VACBanned = sub_json_object->getValue<bool>("VACBanned");
			} else {
				vac_info.VACBanned = false;
			}

			//DaysSinceLastBan
			if (sub_json_object->has("DaysSinceLastBan"))
			{
				vac_info.DaysSinceLastBan = sub_json_object->getValue<int>("DaysSinceLastBan");
			} else {
				vac_info.DaysSinceLastBan = 0;
			}
		}

		//VAC Check Log
		std::stringstream strstream;
		strstream << "VAC Check: ID: " << vac_info.steamID << " Banned: " << vac_info.VACBanned;
		console->log(strstream.str(),ArmaConsole::LogType::DEBUG);

		//Return VAC Status
		if (vac_info.VACBanned) { return true; }
		if (vac_info.NumberOfVACBans > 0) { return true; }
		if (vac_info.DaysSinceLastBan > 0) { return true; }
	}
	return false;
}

/*
void VAC::callProtocol(std::string input_str, std::string &result)
{
	Poco::StringTokenizer t_arg(input_str, ":");
	const int num_of_inputs = t_arg.count();
	if (num_of_inputs == 2)
	{
		if (!isNumber(t_arg[1])) // Check Valid Steam ID
		{
			result  = "[0, \"VAC: Error Invalid Steam ID\"]";
		}
		else
		{
			std::string steamdID = convertSteamIDtoBEGUID(t_arg[1]);
			updateVAC(extension_ptr->getAPIKey(), steamdID);

			if (boost::iequals(t_arg[0], std::string("GetFriends")) == 1)
			{
				//TODO
				result  = "[0, \"VAC: NOT WORKING YET\"]";
			}
			else if (boost::iequals(t_arg[0], std::string("VACBanned")) == 1)
			{
				//TODO
				//	VAC_Cache.get(steamID).VACBanned
				result  = "[0, \"VAC: NOT WORKING YET\"]";
			}
			else if (boost::iequals(t_arg[0], std::string("NumberOfVACBans")) == 1)
			{
				//TODO
				//	VAC_Cache.get(steamID).DaysSinceLastBan
				result  = "[0, \"VAC: NOT WORKING YET\"]";
			}
			else if (boost::iequals(t_arg[0], std::string("DaysSinceLastBan")) == 1)
			{
				//TODO
				//	VAC_Cache.get(steamID).EconomyBan
				result  = "[0, \"VAC: NOT WORKING YET\"]";
			}
			else if (boost::iequals(t_arg[0], std::string("EconomyBan")) == 1)
			{
				//TODO
				result  = "[0, \"VAC: NOT WORKING YET\"]";
			}
			else if (boost::iequals(t_arg[0], std::string("VACAutoBan")) == 1)
			{
				if (true)
				{
					//if ((Poco::NumberParser::parse(vac_info.NumberOfVACBans) >= vac_ban_check.NumberOfVACBans) && (Poco::NumberParser::parse(vac_info.DaysSinceLastBan) <=vac_ban_check.DaysSinceLastBan ))
						//rcon.sendCommand("ban " + convertSteamIDtoBEGUID(steam_id) + " " + vac_ban_check.BanDuration + " " + vac_ban_check.BanMessage);
				}
			}
			else
			{
				result  = "[0, \"VAC: Invalid Query Type\"]";
			}
		}
	}
}
*/