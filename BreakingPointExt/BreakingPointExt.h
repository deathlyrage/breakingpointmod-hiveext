#pragma once

#include "Sqf.h"
#include "Rcon.h"
#include <queue>
#include <map>
#include "Database.h"
#include "AsyncWorker.h"

#define INVALID_NAME_MSG "Invalid Characters in name. It must be only contain A-Z a-z 0-9 _ ( )"

class BreakingPointExt
{
	public:
		BreakingPointExt(std::string a_serverFolder,int a_serverPort);
		~BreakingPointExt();

		void Startup();
		void Shutdown();

		void InitThreads();

		std::string callExtension(std::string function);
		static Sqf::Parameters BreakingPointExt::booleanReturn(bool isGood);

		std::string versionNum;

		bool loaded;
		bool whitelist;
		bool fireDeamonActive;
		bool threadingDebug;

		int timeUntilRestart();

		// Database Details
		std::string DatabaseIP;
		std::string DatabaseName;
		std::string DatabaseUser;
		std::string DatabasePass;
		std::string DatabasePort;

		//Check Invalid Names
		bool invalidName(std::string playerName);
		bool invalidName(std::string playerName, std::string guid);
		bool invalidName(std::string playerName, int index);

		//Ticket Async Commands
		inline long int createTicket(std::string cmd) { return asyncWorker->createTicket(cmd); }
		inline bool ticketExists(long int ticketID) { return asyncWorker->ticketExists(ticketID); }
		inline bool ticketReady(long int ticketID) { return asyncWorker->ticketReady(ticketID); }
		inline std::string ticketResult(long int ticketID) { return asyncWorker->ticketResult(ticketID); }

		//Complete Async Commands
		void callExtensionAsync(std::string cmd, bool important = true);

		// Check Player Account on Web Server / Forums DB
		//Database::Account lookupAccount(std::string guid);
		//void checkAccount(std::string playerID, std::string guid);
		//void checkAccountAsync(std::string guid);

		void logKick(std::string name, std::string guid, std::string kick);

	protected:
		//Worker Threads
		Poco::Thread rcon_worker_thread;
		Poco::Thread steam_worker_thread;
		Poco::Thread guid_worker_thread;
		Poco::Thread async_worker_thread;

		Database * database;
		Rcon * rcon;
		AsyncWorker * asyncWorker;
		VAC * vac;

		//Server Details
		std::string serverFolder;
		int serverPort;

	private:
		std::queue<Sqf::Parameters> objectQueue;

		Sqf::Value serverBootup(Sqf::Parameters params);
		Sqf::Value serverShutdown(Sqf::Parameters params);

		Sqf::Value fetchObjects(Sqf::Parameters params);
		Sqf::Value fetchVehicles(Sqf::Parameters params);
		Sqf::Value fetchHouses(Sqf::Parameters params);
		Sqf::Value fetchTraps(Sqf::Parameters params);
		Sqf::Value serverTime(Sqf::Parameters params);

		Sqf::Value loadPlayer(Sqf::Parameters params);
		Sqf::Value loadCharacterDetails(Sqf::Parameters params);
		Sqf::Value playerActiveServer(Sqf::Parameters params);

		Sqf::Value playerConnected(Sqf::Parameters params);
		Sqf::Value playerDisconnected(Sqf::Parameters params);
		//Sqf::Value playerKicked(Sqf::Parameters params);
		//Sqf::Value playerAccount(Sqf::Parameters params);

		Sqf::Value serverLock(Sqf::Parameters params);
		Sqf::Value serverUnlock(Sqf::Parameters params);

		Sqf::Value playerDeath(Sqf::Parameters params);
		Sqf::Value playerUpdate(Sqf::Parameters params);
		Sqf::Value playerUpdateGear(Sqf::Parameters params);
		Sqf::Value playerUpdateDog(Sqf::Parameters params);
		Sqf::Value playerUpdatePos(Sqf::Parameters params);

		Sqf::Value objectFuel(Sqf::Parameters params);
		Sqf::Value objectTimestamp(Sqf::Parameters params);
		Sqf::Value objectPublish(Sqf::Parameters params);
		Sqf::Value objectInventory(Sqf::Parameters params);
		Sqf::Value objectDelete(Sqf::Parameters params);

		Sqf::Value vehicleCreate(Sqf::Parameters params);
		Sqf::Value vehicleDelete(Sqf::Parameters params);
		Sqf::Value vehicleInventory(Sqf::Parameters params);
		Sqf::Value vehicleTimestamp(Sqf::Parameters params);
		Sqf::Value vehicleMoved(Sqf::Parameters params);
		Sqf::Value vehicleDamaged(Sqf::Parameters params);

		Sqf::Value trapPublish(Sqf::Parameters params);
		Sqf::Value trapDelete(Sqf::Parameters params);

		Sqf::Value housePublish(Sqf::Parameters params);
		Sqf::Value houseDelete(Sqf::Parameters params);
		Sqf::Value houseTimestamp(Sqf::Parameters params);

		Sqf::Value combatLog(Sqf::Parameters params);
		Sqf::Value killLog(Sqf::Parameters params);
		Sqf::Value storageLog(Sqf::Parameters params);
		Sqf::Value hackLog(Sqf::Parameters params);
		Sqf::Value adminLog(Sqf::Parameters params);

		Sqf::Value fetchHackData(Sqf::Parameters params);

};