// BreakingPointExt.cpp : Defines the exported functions for the DLL application.
//

#include "BreakingPointExt.h"

#include <stdlib.h>
#include <iostream>

#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include <stdio.h>
#include <shellapi.h>

#include "ArmaConsole.h"
#include "Rcon.h"

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <Poco/UnicodeConverter.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <boost/progress.hpp>
//#define DEV

typedef boost::function<Sqf::Value(Sqf::Parameters)> HandlerFunc;
map<int, HandlerFunc> handlers;

BreakingPointExt::BreakingPointExt(std::string a_serverFolder, int a_serverPort)
{
	//Extension Version
	versionNum = "0.002";

	//Load Args
	serverFolder = a_serverFolder;
	serverPort = a_serverPort;

	database = new Database();
	loaded = false;

	//Load Ini File
	boost::property_tree::ptree pt;
	boost::property_tree::ini_parser::read_ini(serverFolder + "/BreakingPointExt.ini", pt);
	int totalSlots = pt.get<int>("VIP.slots", 100);
	int reservedSlots = pt.get<int>("VIP.reserved", 10);
	std::string fireDeamonActiveStr = pt.get<std::string>("FIREDAEMON.active", "false");
	std::string serviceName = pt.get<std::string>("FIREDAEMON.service", "BPA3_1");
	std::string fireDaemonPath = pt.get<std::string>("FIREDAEMON.path", "C:/Program Files/FireDaemon/Firedaemon.exe");
	std::string serverPassword = pt.get<std::string>("RCON.password", "rconpw");
	std::string whitelistStr = pt.get<std::string>("RCON.whitelist", "false");
	int rconPort = pt.get<int>("RCON.port", 2305);
	whitelist = false;
	fireDeamonActive = false;
	if (whitelistStr == "true") { whitelist = true; }
	if (fireDeamonActiveStr == "true") { fireDeamonActive = true; }
	threadingDebug = pt.get<bool>("THREADING.debug", false);
	int threadingSize = pt.get<int>("THREADING.size", 100);
	int threadingDelay = pt.get<int>("THREADING.delay", 50);

	// Database Details
	DatabaseIP = pt.get<std::string>("DATABASE.ip", "127.0.0.1");
	DatabaseName = pt.get<std::string>("DATABASE.database", "breakingpointmod");
	DatabaseUser = pt.get<std::string>("DATABASE.username", "root");
	DatabasePass = pt.get<std::string>("DATABASE.password", "");
	DatabasePort = pt.get<std::string>("DATABASE.port", "3306");

	//Init Rcon
	rcon = new Rcon();
	rcon->updateLogin("127.0.0.1", rconPort, serverPassword);
	rcon->init(totalSlots, reservedSlots, fireDeamonActive, serviceName, fireDaemonPath, whitelist);

	//Init Async Worker
	asyncWorker = new AsyncWorker(threadingDebug,threadingSize,threadingDelay);

	//Init VAC
	vac = new VAC();
	vac->init("VAC_WEB_API_KEY");

	//Server Init / Object Streaming
	handlers[100] = boost::bind(&BreakingPointExt::serverBootup, this, _1);
	handlers[101] = boost::bind(&BreakingPointExt::fetchObjects, this, _1);
	handlers[102] = boost::bind(&BreakingPointExt::fetchVehicles, this, _1);
	handlers[103] = boost::bind(&BreakingPointExt::fetchHouses, this, _1);
	handlers[104] = boost::bind(&BreakingPointExt::fetchTraps, this, _1);

	handlers[108] = boost::bind(&BreakingPointExt::serverTime, this, _1);
	handlers[109] = boost::bind(&BreakingPointExt::serverShutdown, this, _1);

	handlers[110] = boost::bind(&BreakingPointExt::playerConnected, this, _1);
	handlers[111] = boost::bind(&BreakingPointExt::playerDisconnected, this, _1);
	//handlers[112] = boost::bind(&BreakingPointExt::playerKicked, this, _1);
	//handlers[113] = boost::bind(&BreakingPointExt::playerAccount, this, _1);

	handlers[114] = boost::bind(&BreakingPointExt::serverLock, this, _1);
	handlers[115] = boost::bind(&BreakingPointExt::serverUnlock, this, _1);

	//Player Login
	handlers[201] = boost::bind(&BreakingPointExt::loadPlayer, this, _1);
	handlers[202] = boost::bind(&BreakingPointExt::loadCharacterDetails, this, _1);
	handlers[203] = boost::bind(&BreakingPointExt::playerActiveServer, this, _1);

	//Player Updates
	handlers[301] = boost::bind(&BreakingPointExt::playerDeath, this, _1);
	handlers[302] = boost::bind(&BreakingPointExt::playerUpdate, this, _1);
	handlers[303] = boost::bind(&BreakingPointExt::playerUpdateGear, this, _1);
	handlers[304] = boost::bind(&BreakingPointExt::playerUpdateDog, this, _1);
	handlers[305] = boost::bind(&BreakingPointExt::playerUpdatePos, this, _1);

	//Objects
	handlers[398] = boost::bind(&BreakingPointExt::objectFuel, this, _1);
	handlers[399] = boost::bind(&BreakingPointExt::objectTimestamp, this, _1);
	handlers[400] = boost::bind(&BreakingPointExt::objectPublish, this, _1);
	handlers[401] = boost::bind(&BreakingPointExt::objectInventory, this, _1);
	handlers[402] = boost::bind(&BreakingPointExt::objectDelete, this, _1);

	//Houses
	handlers[407] = boost::bind(&BreakingPointExt::housePublish, this, _1);
	handlers[408] = boost::bind(&BreakingPointExt::houseDelete, this, _1);
	handlers[409] = boost::bind(&BreakingPointExt::houseTimestamp, this, _1);

	//Traps
	handlers[410] = boost::bind(&BreakingPointExt::trapPublish, this, _1);
	handlers[411] = boost::bind(&BreakingPointExt::trapDelete, this, _1);

	//Vehicles
	handlers[500] = boost::bind(&BreakingPointExt::vehicleCreate, this, _1);
	handlers[501] = boost::bind(&BreakingPointExt::vehicleDelete, this, _1);
	handlers[502] = boost::bind(&BreakingPointExt::vehicleInventory, this, _1);
	handlers[504] = boost::bind(&BreakingPointExt::vehicleTimestamp, this, _1);
	handlers[505] = boost::bind(&BreakingPointExt::vehicleMoved, this, _1);
	handlers[506] = boost::bind(&BreakingPointExt::vehicleDamaged, this, _1);

	//Server Logging
	handlers[800] = boost::bind(&BreakingPointExt::combatLog, this, _1);
	handlers[801] = boost::bind(&BreakingPointExt::killLog, this, _1);
	handlers[802] = boost::bind(&BreakingPointExt::storageLog, this, _1);
	handlers[803] = boost::bind(&BreakingPointExt::hackLog, this, _1);
	handlers[804] = boost::bind(&BreakingPointExt::adminLog, this, _1);

	//Hack Data
	handlers[900] = boost::bind(&BreakingPointExt::fetchHackData, this, _1);
};

BreakingPointExt::~BreakingPointExt()
{
	if (rcon) 
	{
		delete rcon;
		rcon = nullptr;
	}
	if (database) 
	{ 
		delete database;
		database = nullptr;
	}
	if (asyncWorker) 
	{ 
		delete asyncWorker;
		asyncWorker = nullptr;
	}
	if (vac) 
	{ 
		delete vac;
		vac = nullptr;
	}
}

void BreakingPointExt::Startup()
{
}

void BreakingPointExt::Shutdown()
{
	if (rcon) {
		rcon->disconnect();
		rcon_worker_thread.join();
	}

	if (asyncWorker) {
		asyncWorker->stop();
		async_worker_thread.join();
	}
}
 
void BreakingPointExt::InitThreads()
{
	//Start RCON Thread
	if (rcon) { rcon_worker_thread.start(*rcon); }

	//Start Async Worker Thread
	if (asyncWorker) { async_worker_thread.start(*asyncWorker); }
}

/*
Database::Account BreakingPointExt::lookupAccount(std::string guid)
{
	return database->lookupAccount(guid);
}
*/

void BreakingPointExt::callExtensionAsync(std::string cmd, bool important)
{
	asyncWorker->execute(cmd, important);
}

void BreakingPointExt::logKick(std::string name, std::string guid, std::string kick)
{
	asyncWorker->execute("CHILD:112:" + name + ":" + guid + ":" + kick + ":");
}

Sqf::Parameters BreakingPointExt::booleanReturn(bool isGood)
{
	Sqf::Parameters retVal;
	string retStatus = "PASS";
	if (!isGood)
		retStatus = "ERROR";

	retVal.push_back(retStatus);
	return retVal;
}

Sqf::Value BreakingPointExt::serverBootup(Sqf::Parameters params)
{
	serverID = boost::get<int>(params.at(0));

	database->connect();
	
	loaded = true;

	Sqf::Parameters retVal;
	retVal.push_back(versionNum);
	return retVal;
};

Sqf::Value BreakingPointExt::serverShutdown(Sqf::Parameters params)
{
	database->shutdownCleanup();

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::fetchObjects(Sqf::Parameters params)
{
	Sqf::Parameters retVal;

	if (objectQueue.empty())
	{
		database->populateObjects(objectQueue);

		retVal.push_back(string("ObjectStreamStart"));
		retVal.push_back(static_cast<int>(objectQueue.size()));
	}
	else
	{
		retVal = objectQueue.front();
		objectQueue.pop();
	}
	return retVal;
};

Sqf::Value BreakingPointExt::fetchVehicles(Sqf::Parameters params)
{
	Sqf::Parameters retVal;

	if (objectQueue.empty())
	{
		database->populateVehicles(objectQueue);

		retVal.push_back(string("VehicleStreamStart"));
		retVal.push_back(static_cast<int>(objectQueue.size()));
	}
	else
	{
		retVal = objectQueue.front();
		objectQueue.pop();
	}
	return retVal;
};


Sqf::Value BreakingPointExt::fetchHouses(Sqf::Parameters params)
{
	Sqf::Parameters retVal;

	if (objectQueue.empty())
	{
		database->populateHouses(objectQueue);

		retVal.push_back(string("HouseStreamStart"));
		retVal.push_back(static_cast<int>(objectQueue.size()));
	}
	else
	{
		retVal = objectQueue.front();
		objectQueue.pop();
	}
	return retVal;
};

Sqf::Value BreakingPointExt::fetchTraps(Sqf::Parameters params)
{
	Sqf::Parameters retVal;

	if (objectQueue.empty())
	{
		database->populateTraps(objectQueue);

		retVal.push_back(string("TrapStreamStart"));
		retVal.push_back(static_cast<int>(objectQueue.size()));
	}
	else
	{
		retVal = objectQueue.front();
		objectQueue.pop();
	}
	return retVal;
};

Sqf::Value BreakingPointExt::serverTime(Sqf::Parameters params)
{
	namespace pt = boost::posix_time;
	pt::ptime now = pt::second_clock::local_time();

	Sqf::Parameters retVal;
	retVal.push_back(string("PASS"));
	{
		Sqf::Parameters dateTime;
		dateTime.push_back(static_cast<int>(now.date().year()));
		dateTime.push_back(static_cast<int>(now.date().month()));
		dateTime.push_back(static_cast<int>(now.date().day()));
		dateTime.push_back(static_cast<int>(now.time_of_day().hours()));
		dateTime.push_back(static_cast<int>(now.time_of_day().minutes()));
		retVal.push_back(dateTime);
	}

	//Determine If Shutdown Mode or Not
	bool shutdownMode = false;
	int shutdownTimes[10] = {2,5,8,11,14,17,20,23};
	int hour = static_cast<int>(now.time_of_day().hours());
	int minute = static_cast<int>(now.time_of_day().minutes());
	for (int i = 0; i < 8; ++i) {
		if (hour == shutdownTimes[i] && minute > 54) {
			shutdownMode = true;
			break;
		}
	}
	retVal.push_back(shutdownMode);
	return retVal;
};

int BreakingPointExt::timeUntilRestart()
{
	int remainingTime = 61;
	namespace pt = boost::posix_time;
	pt::ptime now = pt::second_clock::local_time();
	int shutdownTimes[10] = { 2, 5, 8, 11, 14, 17, 20, 23 };
	int hour = static_cast<int>(now.time_of_day().hours());
	int minute = static_cast<int>(now.time_of_day().minutes());
	for (int i = 0; i < 8; ++i) {
		if (hour == shutdownTimes[i]) {
			remainingTime = (60 - minute);
			break;
		}
	}
	return remainingTime;
}

/*
void BreakingPointExt::checkAccount(std::string playerID, std::string guid)
{
	if (whitelist)
	{
		if (rcon->status())
		{
			//Database Checks
			if (loaded)
			{
				//Lookup Account
				Database::Account account = lookupAccount(guid);

				//Whitelisting
				if (account.id < 0)
				{
					rcon->kick(guid, "Account Whitelist Failed.");
					return;
				};

				//Temp Banned
				if (account.temp_banned)
				{
					rcon->kick(guid, "Temp Ban");
					return;
				}

				//Perma Banned
				if (account.banned)
				{
					rcon->kick(guid, "Perma Ban");
					return;
				}

				//VIP Slots
				if (rcon->overReservedSlots())
				{
					if (!account.vip)
					{
						rcon->kick(guid, "Become a VIP to access reserved slots.");
						return;
					}
				}
			} else {
				rcon->kick(guid, "Server Not Ready Yet.");
				return;
			}
		}
	}
}
*/

Sqf::Value BreakingPointExt::playerConnected(Sqf::Parameters params)
{
	std::string playerID = Sqf::GetStringAny(params.at(0));
	std::string playerName = Sqf::GetStringAny(params.at(1));
	std::string guid = VAC::convertSteamIDtoBEGUID(playerID);

	//checkAccount(playerID, guid);

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::playerDisconnected(Sqf::Parameters params)
{
	std::string playerID = Sqf::GetStringAny(params.at(0));
	std::string playerName = Sqf::GetStringAny(params.at(1));

	//Clear Active Server via Player ID
	database->clearActiveServer(0, playerID);

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::serverLock(Sqf::Parameters params)
{
	rcon->lock();

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::serverUnlock(Sqf::Parameters params)
{
	rcon->unlock();

	return booleanReturn(true);
};

//Sqf::Value BreakingPointExt::playerKicked(Sqf::Parameters params)
//{
//	std::string name = Sqf::GetStringAny(params.at(0));
//	std::string guid = Sqf::GetStringAny(params.at(1));
//	std::string kick = Sqf::GetStringAny(params.at(2));
//
//	//Clear Active Server via Player ID
//	database->kickLog(name, guid, kick);
//
//	return booleanReturn(true);
//};
//
//Sqf::Value BreakingPointExt::playerAccount(Sqf::Parameters params)
//{
//	std::string guid = Sqf::GetStringAny(params.at(0));
//	lookupAccount(guid);
//	return booleanReturn(true);
//};

Sqf::Value BreakingPointExt::loadPlayer(Sqf::Parameters params)
{
	std::string playerID = Sqf::GetStringAny(params.at(0));
	const std::string playerName = Sqf::GetStringAny(params.at(1));

	return database->fetchCharacterInitial(playerID, playerName);
};

Sqf::Value BreakingPointExt::loadCharacterDetails(Sqf::Parameters params)
{
	int characterID = Sqf::GetIntAny(params.at(0));
	return database->fetchCharacterDetails(characterID);
};

Sqf::Value BreakingPointExt::playerActiveServer(Sqf::Parameters params)
{
	int characterID = Sqf::GetIntAny(params.at(0));
	std::string playerID = Sqf::GetStringAny(params.at(1));
	return booleanReturn(database->clearActiveServer(characterID, playerID));
};

//Player Updates / Death

Sqf::Value BreakingPointExt::playerDeath(Sqf::Parameters params)
{
	int characterID = Sqf::GetIntAny(params.at(0));
	return database->killCharacter(characterID);
};

Sqf::Value BreakingPointExt::playerUpdate(Sqf::Parameters params)
{
	int characterID = Sqf::GetIntAny(params.at(0));
	Database::FieldsType fields;

	try
	{
		if (!Sqf::IsNull(params.at(1)))
		{
			try
			{
				Sqf::Parameters medicalArr = boost::get<Sqf::Parameters>(params.at(1));
				if (medicalArr.size() > 0)
				{
					for (size_t i = 0; i < medicalArr.size(); i++)
					{
						if (Sqf::IsAny(medicalArr[i]))
						{
							//logger().warning("update.medical[" + lexical_cast<string>(i)+"] changed from any to []");
							medicalArr[i] = Sqf::Parameters();
						}
					}
					Sqf::Value medical = medicalArr;
					fields["medical"] = medical;
				}
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for medical array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(2)))
		{
			try
			{
				int moreKillsZ = boost::get<int>(params.at(2));
				if (moreKillsZ > 0) fields["zombie_kills"] = moreKillsZ;
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for zombie_kills.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(3)))
		{
			try
			{
				int moreKillsH = boost::get<int>(params.at(3));
				if (moreKillsH > 0) fields["headshots"] = moreKillsH;
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for headshots.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(4)))
		{
			try
			{
				int moreKillsHuman = boost::get<int>(params.at(4));
				if (moreKillsHuman > 0) fields["survivor_kills"] = moreKillsHuman;
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for survivor_kills.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(5)))
		{
			try
			{
				string classa = boost::get<string>(params.at(5));
				fields["class"] = boost::lexical_cast<int>(classa);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for class.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(6)))
		{
			try
			{
				string ranger = boost::get<string>(params.at(6));
				fields["ranger"] = boost::lexical_cast<int>(ranger);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for ranger array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(7)))
		{
			try
			{
				string outlaw = boost::get<string>(params.at(7));
				fields["outlaw"] = boost::lexical_cast<int>(outlaw);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for outlaw array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(8)))
		{
			try
			{
				string hunter = boost::get<string>(params.at(8));
				fields["hunter"] = boost::lexical_cast<int>(hunter);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for hunter array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(9)))
		{
			try
			{
				string nomad = boost::get<string>(params.at(9));
				fields["nomad"] = boost::lexical_cast<int>(nomad);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for nomad array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(10)))
		{
			try
			{
				string survivalist = boost::get<string>(params.at(10));
				fields["survivalist"] = boost::lexical_cast<int>(survivalist);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for survivalist array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(11)))
		{
			try
			{
				string engineer = boost::get<string>(params.at(11));
				fields["engineer"] = boost::lexical_cast<int>(engineer);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for engineer array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(12)))
		{
			try
			{
				string undead = boost::get<string>(params.at(12));
				fields["undead"] = boost::lexical_cast<int>(undead);
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for undead array.", ArmaConsole::LogType::CRITICAL);
			}
		}
		if (!Sqf::IsNull(params.at(13)))
		{
			try
			{
				Sqf::Parameters currentStateArr = boost::get<Sqf::Parameters>(params.at(13));
				if (currentStateArr.size() > 0)
				{
					Sqf::Value currentState = currentStateArr;
					fields["state"] = currentState;
				}
			}
			catch (boost::bad_get&)
			{
				console->log("playerUpdate bad boost::get for currentState array.", ArmaConsole::LogType::CRITICAL);
			}
		}
	}
	catch (const std::out_of_range&)
	{
		console->log("playerUpdate " + lexical_cast<string>(characterID)+" only had " + lexical_cast<string>(params.size()) + " parameters.", ArmaConsole::LogType::CRITICAL);
	}

	return database->updateCharacter(characterID, fields);
};

Sqf::Value BreakingPointExt::playerUpdatePos(Sqf::Parameters params)
{
	int characterID = Sqf::GetIntAny(params.at(0));
	Database::FieldsType fields;

	try
	{
		if (!Sqf::IsNull(params.at(1)))
		{
			Sqf::Parameters worldSpaceArr = boost::get<Sqf::Parameters>(params.at(1));
			if (worldSpaceArr.size() > 0)
			{
				Sqf::Value worldSpace = worldSpaceArr;
				fields["worldspace"] = worldSpace;
			}
		}
	}
	catch (const std::out_of_range&)
	{
		console->log("playerUpdatePos " + lexical_cast<string>(characterID)+" only had " + lexical_cast<string>(params.size()) + " parameters.", ArmaConsole::LogType::CRITICAL);
	}

	return database->updateCharacter(characterID, fields);
};

Sqf::Value BreakingPointExt::playerUpdateGear(Sqf::Parameters params)
{
	int characterID = Sqf::GetIntAny(params.at(0));
	Database::FieldsType fields;

	try
	{
		if (!Sqf::IsNull(params.at(1)))
		{
			Sqf::Parameters inventoryArr = boost::get<Sqf::Parameters>(params.at(1));
			if (inventoryArr.size() > 0)
			{
				Sqf::Value inventory = inventoryArr;
				fields["inventory"] = inventory;
			}
		}
	}
	catch (const std::out_of_range&)
	{
		console->log("Update character gear " + lexical_cast<string>(characterID)+" only had " + lexical_cast<string>(params.size()) + " parameters.", ArmaConsole::LogType::CRITICAL);
	}
	catch (boost::bad_lexical_cast& ex)
	{
		console->log(ex.what(), ArmaConsole::LogType::CRITICAL);
	}
	catch (boost::bad_get& ex)
	{
		console->log(ex.what(), ArmaConsole::LogType::CRITICAL);
	}

	return database->updateCharacter(characterID, fields);
};

Sqf::Value BreakingPointExt::playerUpdateDog(Sqf::Parameters params)
{
	int characterID = Sqf::GetIntAny(params.at(0));
	Database::FieldsType fields;

	try
	{
		if (!Sqf::IsNull(params.at(1)))
		{
			Sqf::Parameters dog = boost::get<Sqf::Parameters>(params.at(1));
			fields["dog"] = dog;
		}
	}
	catch (const std::out_of_range&)
	{
		console->log("Update character dog " + lexical_cast<string>(characterID)+" only had " + lexical_cast<string>(params.size()) + " parameters.", ArmaConsole::LogType::CRITICAL);
	}
	catch (boost::bad_lexical_cast& ex)
	{
		console->log(ex.what(), ArmaConsole::LogType::CRITICAL);
	}
	catch (boost::bad_get& ex)
	{
		console->log(ex.what(), ArmaConsole::LogType::CRITICAL);
	}

	return booleanReturn(database->updateCharacter(characterID, fields));
};

Sqf::Value BreakingPointExt::objectTimestamp(Sqf::Parameters params)
{
	Int64 uniqueID = Sqf::GetBigInt(params.at(0));
	if (uniqueID > 0)
		return booleanReturn(database->timestampObject(uniqueID));
	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::objectFuel(Sqf::Parameters params)
{
	Int64 uniqueID = Sqf::GetBigInt(params.at(0));
	double fuel = Sqf::GetDouble(params.at(1));
	if (uniqueID > 0)
		return booleanReturn(database->fuelObject(uniqueID, fuel));
	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::objectPublish(Sqf::Parameters params)
{
	string className = boost::get<string>(params.at(0));
	double damage = Sqf::GetDouble(params.at(1));
	int characterID = Sqf::GetIntAny(params.at(2));
	string playerID = Sqf::GetStringAny(params.at(3));
	Sqf::Value worldSpace = boost::get<Sqf::Parameters>(params.at(4));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(5));
	Sqf::Value hitPoints = boost::get<Sqf::Parameters>(params.at(6));
	double fuel = Sqf::GetDouble(params.at(7));
	string lock = Sqf::GetStringAny(params.at(8));
	Int64 uniqueID = Sqf::GetBigInt(params.at(9));
	Int64 buildingID = Sqf::GetBigInt(params.at(10));

	return booleanReturn(database->createObject(className, damage, characterID, playerID, worldSpace, inventory, hitPoints, fuel, lock, uniqueID, buildingID));
};

Sqf::Value BreakingPointExt::objectInventory(Sqf::Parameters params)
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(1));
	if (objectIdent > 0)
		return booleanReturn(database->updateObjectInventory(objectIdent, inventory));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::objectDelete(Sqf::Parameters params)
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	if (objectIdent > 0)
		return booleanReturn(database->deleteObject(objectIdent));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::vehicleCreate(Sqf::Parameters params)
{
	string className = boost::get<string>(params.at(0));
	double damage = Sqf::GetDouble(params.at(1));
	Sqf::Value worldSpace = boost::get<Sqf::Parameters>(params.at(2));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(3));
	Sqf::Value hitPoints = boost::get<Sqf::Parameters>(params.at(4));
	double fuel = Sqf::GetDouble(params.at(5));
	string id = "0";

	return database->createVehicle(className, damage, worldSpace, inventory, hitPoints, fuel, id);
};

Sqf::Value BreakingPointExt::vehicleDelete(Sqf::Parameters params)
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	if (objectIdent > 0)
		return booleanReturn(database->deleteVehicle(objectIdent));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::vehicleInventory(Sqf::Parameters params)
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(1));
	if (objectIdent > 0)
		return booleanReturn(database->updateVehicleInventory(objectIdent, inventory));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::vehicleTimestamp(Sqf::Parameters params)
{
	Int64 uniqueID = Sqf::GetBigInt(params.at(0));
	if (uniqueID > 0)
		return booleanReturn(database->timestampVehicle(uniqueID));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::vehicleMoved(Sqf::Parameters params)
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value worldspace = boost::get<Sqf::Parameters>(params.at(1));
	double fuel = Sqf::GetDouble(params.at(2));
	if (objectIdent > 0)
		booleanReturn(database->updateVehicleMovement(objectIdent, worldspace, fuel));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::vehicleDamaged(Sqf::Parameters params)
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value hitPoints = boost::get<Sqf::Parameters>(params.at(1));
	double damage = Sqf::GetDouble(params.at(2));

	if (objectIdent > 0) //sometimes script sends this with object id 0, which is bad
		return booleanReturn(database->updateVehicleStatus(objectIdent, hitPoints, damage));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::trapPublish(Sqf::Parameters params)
{
	Int64 uniqueID = Sqf::GetBigInt(params.at(0));
	string classname = Sqf::GetStringAny(params.at(1));
	Sqf::Value worldspace = boost::get<Sqf::Parameters>(params.at(2));
	if (uniqueID > 0)
		return booleanReturn(database->createTrap(uniqueID, classname, worldspace));
	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::trapDelete(Sqf::Parameters params)
{
	Int64 trapUID = Sqf::GetBigInt(params.at(0));
	if (trapUID > 0)
		return booleanReturn(database->deleteTrap(trapUID));
	return booleanReturn(true);
};


Sqf::Value BreakingPointExt::housePublish(Sqf::Parameters params)
{
	string playerID = Sqf::GetStringAny(params.at(0));
	string uniqueID = Sqf::GetStringAny(params.at(1));
	string buildingID = Sqf::GetStringAny(params.at(2));
	string buildingName = Sqf::GetStringAny(params.at(3));
	Sqf::Value worldspace = boost::get<Sqf::Parameters>(params.at(4));
	string lock = Sqf::GetStringAny(params.at(5));
	int explosive = Sqf::GetIntAny(params.at(6));
	int reinforcement = Sqf::GetIntAny(params.at(7));
	return booleanReturn(database->createHouse(playerID, uniqueID, buildingID, buildingName, worldspace, lock, explosive, reinforcement));
};

Sqf::Value BreakingPointExt::houseDelete(Sqf::Parameters params)
{
	Int64 buildingID = Sqf::GetBigInt(params.at(0));
	Int64 buildingUID = Sqf::GetBigInt(params.at(1));
	
	if (buildingID != 0 && buildingUID != 0)
		return booleanReturn(database->deleteHouse(buildingID,buildingUID));

	return booleanReturn(true);
};

Sqf::Value BreakingPointExt::houseTimestamp(Sqf::Parameters params)
{
	Int64 buildingID = Sqf::GetBigInt(params.at(0));

	if (buildingID != 0) //all the vehicles have objectUID = 0, so it would be bad to delete those
		return booleanReturn(database->timestampHouse(buildingID));

	return booleanReturn(true);
};

//Logging

Sqf::Value BreakingPointExt::combatLog(Sqf::Parameters params)
{
	std::string playerID = Sqf::GetStringAny(params.at(0));
	std::string playerName = Sqf::GetStringAny(params.at(1));
	int characterID = Sqf::GetIntAny(params.at(2));
	return booleanReturn(database->combatLog(playerID, playerName, characterID));
};

Sqf::Value BreakingPointExt::killLog(Sqf::Parameters params)
{
	string playerID = Sqf::GetStringAny(params.at(0));
	string playerName = Sqf::GetStringAny(params.at(1));
	string killerID = Sqf::GetStringAny(params.at(2));
	string killerName = Sqf::GetStringAny(params.at(3));
	int distance = Sqf::GetIntAny(params.at(4));
	string weapon = Sqf::GetStringAny(params.at(5));
	return booleanReturn(database->killLog(playerID, playerName, killerID, killerName, distance, weapon));
};

Sqf::Value BreakingPointExt::storageLog(Sqf::Parameters params)
{
	string playerID = Sqf::GetStringAny(params.at(0));
	string playerName = Sqf::GetStringAny(params.at(1));
	string deployableID = Sqf::GetStringAny(params.at(2));
	string deployableName = Sqf::GetStringAny(params.at(3));
	string ownerID = Sqf::GetStringAny(params.at(4));
	return booleanReturn(database->storageLog(playerID, playerName, deployableID, deployableName, ownerID));
};

Sqf::Value BreakingPointExt::hackLog(Sqf::Parameters params)
{
	string playerID = Sqf::GetStringAny(params.at(0));
	string playerName = Sqf::GetStringAny(params.at(1));
	string reason = Sqf::GetStringAny(params.at(2));
	return booleanReturn(database->hackLog(playerID, playerName, reason));
};

Sqf::Value BreakingPointExt::adminLog(Sqf::Parameters params)
{
	string playerID = Sqf::GetStringAny(params.at(0));
	string playerName = Sqf::GetStringAny(params.at(1));
	string action = Sqf::GetStringAny(params.at(2));
	return booleanReturn(database->adminLog(playerID, playerName, action));
};

Sqf::Value BreakingPointExt::fetchHackData(Sqf::Parameters params)
{
	Sqf::Parameters retVal;

	if (objectQueue.empty())
	{
		int requestID = boost::get<int>(params.at(0));
		database->populateHackData(requestID, objectQueue);

		retVal.push_back(string("CustomStreamStart"));
		retVal.push_back(static_cast<int>(objectQueue.size()));
	}
	else
	{
		retVal = objectQueue.front();
		objectQueue.pop();
	}
	return retVal;
};


bool BreakingPointExt::invalidName(std::string playerName)
{
	return (playerName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_() ") != std::string::npos);
}

bool BreakingPointExt::invalidName(std::string playerName, std::string guid)
{
	if (invalidName(playerName)) {
		rcon->kick(guid, INVALID_NAME_MSG);
		return true;
	} else {
		return false;
	}
}

bool BreakingPointExt::invalidName(std::string playerName, int index)
{
	if (invalidName(playerName)) {
		rcon->kick(index, INVALID_NAME_MSG);
		return true;
	} else {
		return false;
	}
}

std::string BreakingPointExt::callExtension(std::string function)
{

#ifdef DEV
	// start timing
	boost::timer t;

	// log extension start if enabled
	if (threadingDebug)
	{
		console->log("callExtensionStart: " + string(function), ArmaConsole::LogType::CRITICAL);
	}
#endif

	Sqf::Parameters params;
	try
	{
		params = lexical_cast<Sqf::Parameters>(function);
	}
	catch (boost::bad_lexical_cast)
	{
		console->log("Cannot parse function: " + string(function));
		return "";
	}
	catch (...)
	{
		console->log("Cannot parse function: " + string(function));
		return "";
	}

	int funcNum = -1;
	try
	{
		string childIdent = boost::get<string>(params.at(0));
		if (childIdent != "CHILD")
			throw std::runtime_error("First element in parameters must be CHILD");

		params.erase(params.begin());
		funcNum = boost::get<int>(params.at(0));
		params.erase(params.begin()); 
	}
	catch (...)
	{
		console->log("Invalid function format: " + string(function));
		return "";
	}

	if (handlers.count(funcNum) < 1)
	{
		console->log("Invalid method id: " + lexical_cast<string>(funcNum));
		return "";
	}

	HandlerFunc handler = handlers[funcNum];
	Sqf::Value res;
	try
	{
		res = handler(params);
#ifdef DEV
		// log it if debug is enabled
		if (threadingDebug)
		{
			console->log("callExtensionResult: " + lexical_cast<string>(res), ArmaConsole::LogType::CRITICAL);
		}
#endif
	}
	catch (Poco::Data::MySQL::ConnectionException& ex)
	{
		console->log("Error executing |" + string(function) + "|");
		console->log(ex.what());
		console->log(ex.message());

		bool reconnection = database->connectionFailure();
		if (reconnection) 
		{
			callExtension(function);
		}
		else
		{
			return "";
		}
	}
	catch (Poco::Data::MySQL::StatementException& ex)
	{
		console->log("Error executing |" + string(function) + "|");
		console->log(ex.what());
		console->log(ex.message());
		return "";
	}
	catch (Poco::Data::MySQL::MySQLException& ex)
	{
		console->log("Error executing |" + string(function) + "|");
		console->log(ex.what());
		console->log(ex.message());
		return "";
	}
	catch (Poco::Exception& ex)
	{
		console->log("Error executing |" + string(function) + "|");
		console->log(ex.what());
		console->log(ex.message());
		return "";
	}
	catch (boost::exception&)
	{
		console->log("Error executing |" + string(function) + "|");
		console->log("unknown boost exception");
		return "";
	}
	catch (std::exception& ex)
	{
		console->log("Error executing |" + string(function) + "|");
		console->log(ex.what());
		return "";
	}
	catch (...)
	{
		console->log("Error executing |" + string(function) + "|");
		return "";
	}

#ifdef DEV
	// log it if debug is enabled
	if (threadingDebug)
	{
		// get time it took to complete this hive call
		std::string elapsed_time_str = boost::lexical_cast<std::string>(t.elapsed());

		// perform debug log
		console->log("callExtensionEnd: " + string(function) + " Time Taken: " + string(elapsed_time_str), ArmaConsole::LogType::CRITICAL);
	}
#endif

	return lexical_cast<string>(res);
};
