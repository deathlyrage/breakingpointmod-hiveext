#pragma once

#include "ArmaConsole.h"
#include "sqf.h"

#include "VAC.h"

#include "Poco/Uri.h"
#include "Poco/String.h"
#include "Poco/Format.h"
#include "Poco/Tuple.h"
#include "Poco/NamedTuple.h"
#include "Poco/Exception.h"
#include "Poco/Data/LOB.h"
#include "Poco/Data/Row.h"
#include "Poco/Data/Session.h"
#include "Poco/Data/Preparation.h"
#include "Poco/Data/RowFilter.h"
#include "Poco/Data/RowFormatter.h"
#include "Poco/Data/RowIterator.h"
#include "Poco/Data/Statement.h"
#include "Poco/Data/StatementImpl.h"
#include "Poco/Data/StatementCreator.h"
#include "Poco/Data/SessionFactory.h"
#include "Poco/Data/MySQL/Connector.h"
#include "Poco/Data/MySQL/MySQLException.h"
#include "Poco/Data/MySQL/Utility.h"
#include "Poco/Nullable.h"
#include "Poco/Net/DNS.h"
#include "Poco/Net/NetException.h"

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <iostream>

#include <iostream>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <vector>
#include <string>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

using boost::lexical_cast;

using namespace Poco;
using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco::Data::MySQL;

extern int serverID;

extern ArmaConsole * console;

class Database

{
	private:
		Poco::Data::Session * activeSession;
		
	public:

		struct Account
		{
			int id = -1;
			int group = 0;
			bool vip = false;
			bool banned = false;
			bool temp_banned = false;
			std::string guid = "";
		};

		Database();
		~Database();

		void connect();
		void disconnect();
		int ping(std::string domain);

		bool connectionFailure();

		void shutdownCleanup();

		//Account lookupAccount(std::string guid);

		void populateObjects(std::queue<Sqf::Parameters>& queue);
		void populateVehicles(std::queue<Sqf::Parameters>& queue);
		void populateHouses(std::queue<Sqf::Parameters>& queue);
		void populateTraps(std::queue<Sqf::Parameters>& queue);
		void populateHackData(int requestID, std::queue<Sqf::Parameters>& queue);

		Sqf::Value fetchCharacterInitial(string playerID, string playerName);

		Sqf::Value fetchCharacterDetails(int characterID);

		bool clearActiveServer(int characterID, string playerID);

		Sqf::Value killCharacter(int characterID);

		typedef map<string, Sqf::Value> FieldsType;
		bool updateCharacter(int characterID, const FieldsType& fields);

		bool timestampObject(Int64 uniqueID);
		bool fuelObject(Int64 uniqueID, double fuel);
		
		bool createObject(string className, double damage, int characterID, string playerID, Sqf::Value worldSpace, Sqf::Value inventory, Sqf::Value hitPoints, double fuel, string lock, Int64 uniqueID, Int64 buildingID);
		bool updateObjectInventory(Int64 objectIdent, Sqf::Value& inventory);
		bool deleteObject(Int64 objectIdent);

		Sqf::Value createVehicle(string className, double damage, Sqf::Value worldSpace, Sqf::Value inventory, Sqf::Value hitPoints, double fuel, string id);
		bool deleteVehicle(Int64 objectIdent);
		bool updateVehicleInventory(Int64 objectIdent, Sqf::Value& inventory);
		bool timestampVehicle(Int64 uniqueID);

		bool updateVehicleMovement(Int64 objectIdent, Sqf::Value& worldSpace, double fuel);
		bool updateVehicleStatus(Int64 objectIdent, Sqf::Value& hitPoints, double damage);

		bool createTrap(Int64 trapUID, string& classname, Sqf::Value& worldspace);
		bool deleteTrap(Int64 trapUID);

		bool createHouse(string& playerID, string& uniqueID, string& buildingID, string& buildingName, Sqf::Value& worldspace, string& lock, int explosive, int reinforcement);
		bool deleteHouse(Int64 buildingID, Int64 buildingUID);
		bool timestampHouse(Int64 buildingID);

		bool combatLog(string playerID, string playerName, int characterID);
		bool killLog(string playerID, string playerName, string killerID, string killerName, int distance, string weapon);
		bool storageLog(string playerID, string playerName, string deployableID, string deployableName, string ownerID);
		bool hackLog(string playerID, string playerName, string reason);
		bool kickLog(string name, string guid, string kick);
		Sqf::Value getLastInsertId();
		bool adminLog(string playerID, string playerName, string action);
};