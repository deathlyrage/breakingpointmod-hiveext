#include "Database.h"
#include "BreakingPointExt.h"
#include "Poco/Data/RecordSet.h"

extern BreakingPointExt * breakingPointExt;

Database::Database()
{
	Poco::Data::MySQL::Connector::registerConnector();
};

Database::~Database()
{
	activeSession->close();

	delete activeSession;
	activeSession = nullptr;

	Poco::Data::MySQL::Connector::unregisterConnector();
};

void Database::connect()
{
	try
	{
		// Open Console and Enable STD Write Out
		AllocConsole();
		freopen("CONOUT$", "w", stdout);

		// Log Connection Starting
		std::cout << "Starting Connection To Databases..." << std::endl;

		// Setup Session Null Ptrs
		activeSession = nullptr;

		// Connect to Game DB
		string game_conn = 
			"host=" + breakingPointExt->DatabaseIP + ";"
			"port=" + breakingPointExt->DatabasePort + ";"
			"user=" + breakingPointExt->DatabaseUser + ";"
			"password=" + breakingPointExt->DatabasePass + ";"
			"db=" + breakingPointExt->DatabaseName + ";"
			"auto-reconnect=true";
		activeSession = new Poco::Data::Session("MySQL", game_conn);

		// Clear the Database Password out of Memory
		breakingPointExt->DatabasePass = "";

		// Log Connection String
		//std::cout << "Database Connection String: " << game_conn << std::endl;

		// Game DB Failure
		if (activeSession->isConnected()) {
			std::cout << "Database Connection Success (Game DB)" << std::endl;
		} else {
			std::cout << "Database Connection Failure (Game DB)" << std::endl;
		}

		if (!activeSession) { std::cout << "Critical Database Connection Failure." << std::endl; }
	}
	catch (Data::ConnectionFailedException& ex)
	{
		std::cout << ex.what() << std::endl;
	}
	catch (Poco::Net::ICMPException& ex)
	{
		std::cout << ex.what() << ": " << ex.message() << std::endl;
	}
	catch (Poco::Exception& ex)
	{
		std::cout << ex.what() << ": " << ex.message() << std::endl;
	}
	catch (std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
	}
	catch (...)
	{
		std::cout << "unknown exception" << std::endl;
	}
};

void Database::disconnect()
{

}

int Database::ping(std::string domain)
{
	Poco::Net::HostEntry hostEntry = Poco::Net::DNS::hostByName(domain);
	Poco::Net::HostEntry::AddressList addresses = hostEntry.addresses();
	std::string address = addresses[0].toString();

	std::cout << domain << std::endl;
	std::cout << address << std::endl;

	// Declare and initialize variables
	int ms = -1;
	HANDLE hIcmpFile;
	unsigned long ipaddr = INADDR_NONE;
	DWORD dwRetVal = 0;
	DWORD dwError = 0;
	char SendData[] = "Data Buffer";
	LPVOID ReplyBuffer = NULL;
	DWORD ReplySize = 0;

	//std::cout << "test1" << std::endl;

	ipaddr = inet_addr(address.c_str());
	if (ipaddr == INADDR_NONE)
	{
		//std::cout << "test2 - fail" << std::endl;
		return -1;
	}

	//std::cout << "test2" << std::endl;

	hIcmpFile = IcmpCreateFile();
	if (hIcmpFile == INVALID_HANDLE_VALUE) {
		//std::cout << "test3 - fail" << std::endl;
		//printf("\tUnable to open handle.\n");
		//printf("IcmpCreatefile returned error: %ld\n", GetLastError());
		return -1;
	}
	// Allocate space for at a single reply
	ReplySize = sizeof(ICMP_ECHO_REPLY)+sizeof(SendData)+8;
	ReplyBuffer = (VOID *)malloc(ReplySize);
	if (ReplyBuffer == NULL) {
		//std::cout << "test4 - fail" << std::endl;
		//printf("\tUnable to allocate memory for reply buffer\n");
		return -1;
	}
	else {
		dwRetVal = IcmpSendEcho2(hIcmpFile, NULL, NULL, NULL, ipaddr, SendData, sizeof(SendData), NULL, ReplyBuffer, ReplySize, 1000);
		if (dwRetVal != 0)
		{
			PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
			struct in_addr ReplyAddr;
			ReplyAddr.S_un.S_addr = pEchoReply->Address;
			//printf("\tSent icmp message to %s\n", argv[1]);
			if (dwRetVal > 1) {
				//printf("\tReceived %ld icmp message responses\n", dwRetVal);
				//printf("\tInformation from the first response:\n");
			}
			else {
				//printf("\tReceived %ld icmp message response\n", dwRetVal);
				//printf("\tInformation from this response:\n");
			}
			//printf("\t  Received from %s\n", inet_ntoa(ReplyAddr));
			//printf("\t  Status = %ld  ", pEchoReply->Status);
			switch (pEchoReply->Status) {
			case IP_DEST_HOST_UNREACHABLE:
				//printf("(Destination host was unreachable)\n");
				break;
			case IP_DEST_NET_UNREACHABLE:
				//printf("(Destination Network was unreachable)\n");
				break;
			case IP_REQ_TIMED_OUT:
				//printf("(Request timed out)\n");
				break;
			default:
				//printf("\n");
				break;
			}

			//std::cout << "test6" << std::endl;
			//printf("\t  Roundtrip time = %ld milliseconds\n",
			//	pEchoReply->RoundTripTime);
			ms = pEchoReply->RoundTripTime;
		}
		else {
			//printf("Call to IcmpSendEcho2 failed.\n");
			dwError = GetLastError();
			switch (dwError) {
			case IP_BUF_TOO_SMALL:
				printf("\tReplyBufferSize to small\n");
				break;
			case IP_REQ_TIMED_OUT:
				printf("\tRequest timed out\n");
				break;
			default:
				printf("\tExtended error returned: %ld\n", dwError);
				break;
			}
		}
	}

	//Cleanup Memory
	IcmpCloseHandle(hIcmpFile);
	free(ReplyBuffer);

	//Return Ping
	return ms;
}

bool Database::connectionFailure()
{
	if (!activeSession->isConnected())
	{
		//Attempt Reconnect
		activeSession->reconnect();
	}

	return false;
}

/*
Database::Account Database::lookupAccount(std::string guid)
{
	Account account;
	account.guid = guid;

	try
	{
		if (webserver->isConnected())
		{
			//Check Forum DB
			{
				Poco::Data::Statement stmt((*webserver));
				stmt << "SELECT T1.`member_id` , T1.`member_group_id` , T1.`member_banned` , T1.`temp_ban` FROM `members` AS T1 INNER JOIN `pfields_content` AS T2 ON T1.`member_id` = T2.`member_id` WHERE T2.`field_14` = ?", use(guid), now;
				Poco::Data::RecordSet rs(stmt);

				if (rs.rowCount() > 0)
				{
					account.id = rs[0].convert<int>();
					account.group = rs[1].convert<int>();
					account.banned = rs[2].convert<bool>();
					std::string tempBan = rs[3].convert<std::string>();

					account.temp_banned = (tempBan != "" && tempBan != "0");

					//Developer
					switch (account.group)
					{
						case 16: // VIP
						case 11: // Developer
						case 8: // Owner and Operator
						case 4: // Admin
						case 15: // Head Admin
						case 17: // Forum Developer
						case 13: // Forum Mod
						case 10: // Senior Admin
						case 21: // Server Mod
						case 22: //Testing Team
						case 23: //Head Forum Mod
						account.vip = true;
					}
				}
			}

		}

		// Check Manual GUID Bans DB
		if (!account.banned && !account.temp_banned)
		{
			Poco::Data::Statement stmt((*activeSession));
			stmt << "SELECT 1 FROM `bans` WHERE `guid` = ?", use(guid), now;
			Poco::Data::RecordSet rs(stmt);
			if (rs.rowCount() > 0) {
				account.banned = true;
			}
		}
	}
	catch (Data::MySQL::StatementException& ex)
	{
		console->log(ex.message());
	}
	catch (Data::MySQL::TransactionException& ex)
	{
		console->log(ex.message());
	}
	catch (Poco::Exception& ex)
	{
		console->log(ex.what());
	}
	catch (boost::bad_lexical_cast& ex)
	{
		console->log(ex.what());
	}
	catch (boost::bad_get& ex)
	{
		console->log(ex.what());
	}
	catch (std::exception& ex)
	{
		console->log(ex.what());
	}
	catch (...)
	{
		console->log("Unknown Exception with lookupAccount(" + guid + ")");
	}

	return account;
}
*/

void Database::shutdownCleanup()
{
	//Cleanup Invalid Storage Objects
	Poco::Data::Statement stmt((*activeSession));
	stmt << "DELETE FROM `instance_deployable` WHERE `unique_id` = '0' AND `instance_id` = ?", use(serverID), now;
}

void Database::populateObjects(std::queue<Sqf::Parameters>& queue)
{
	try
	{
		//Reset Ghosting Timer (Server Restart) for this instance
		Poco::Data::Statement ghostingStmt((*activeSession));
		ghostingStmt << "UPDATE `survivor` SET `lastserver` = '0', `activeserver` = '0' WHERE `lastserver` = ? OR `activeserver` = ?", use(serverID), use(serverID);

		//Fetch Objects For This Instance
		Poco::Data::Statement stmt((*activeSession));
		stmt << "select id.`unique_id`, d.`class_name`, id.`owner_id`, id.`player_id`, id.`worldspace`, id.`inventory`, id.`fuel`, id.`lock`, id.`building_id` from `instance_deployable` id inner join `deployable` d on id.`deployable_id` = d.`id` where id.`instance_id` = ? AND `deployable_id` IS NOT NULL", use(serverID);
		stmt.execute();

		//Execute Ghosting Async
		ghostingStmt.executeAsync();

		//Process Objects Data Set
		Poco::Data::RecordSet rs(stmt);

		if (rs.columnCount() >= 1)
		{
			bool more = rs.moveFirst();
			while (more)
			{
				Sqf::Value worldSpace = Sqf::Parameters(); //[]
				Sqf::Value inventory = Sqf::Parameters(); //[]
				Sqf::Value parts = Sqf::Parameters(); //[]

				Sqf::Parameters objParams;
				objParams.push_back(string("OBJ"));

				string objectID = rs[0].convert<std::string>();
				objParams.push_back(objectID);

				string classname = rs[1].convert<std::string>();
				objParams.push_back(classname);

				string ownerID = rs[2].convert<std::string>();
				objParams.push_back(ownerID);

				string playerID = rs[3].convert<std::string>();
				objParams.push_back(playerID);

				try
				{
					string worldSpaceStr = rs[4].convert<std::string>();
					worldSpace = lexical_cast<Sqf::Value>(worldSpaceStr);
				}
				catch (boost::bad_lexical_cast)
				{
					console->log("Invalid Worldspace for ObjectID: " + objectID);
				}

				objParams.push_back(worldSpace);

				try
				{
					string inventoryStr = rs[5].convert<std::string>();
					inventory = lexical_cast<Sqf::Value>(inventoryStr);
				}
				catch (boost::bad_lexical_cast)
				{
					console->log("Invalid Inventory for ObjectID: " + objectID);
				}

				objParams.push_back(inventory);

				double fuel = rs[6].convert<double>();
				objParams.push_back(fuel);

				string lock = rs[7].convert<std::string>();
				objParams.push_back(lock);

				string buildingID = rs[8].convert<std::string>();
				objParams.push_back(buildingID);

				queue.push(objParams);

				more = rs.moveNext();
			}
		}

		//Wait For Async Ghosting Timer Statement
		ghostingStmt.wait();
	}
	catch (boost::bad_lexical_cast& ex)
	{
		console->log(ex.what());
	}
	catch (boost::exception&)
	{
		console->log("unknown boost exception");
	}
	catch (Data::MySQL::StatementException& ex)
	{
		console->log(ex.message());
	}
	catch (Data::MySQL::TransactionException& ex)
	{
		console->log(ex.message());
	}
	catch (std::exception& ex)
	{
		console->log(ex.what());
	}
}

void Database::populateVehicles(std::queue<Sqf::Parameters>& queue)
{
	try
	{
		Poco::Data::Statement stmt((*activeSession));
		stmt << "select `id`, `classname`, `worldspace`, `inventory`, `parts`, `fuel`, `damage` from `instance_vehicle` where `instance_id` = ?", use(serverID), now;
		Poco::Data::RecordSet rs(stmt);

		if (rs.columnCount() >= 1)
		{
			bool more = rs.moveFirst();
			while (more)
			{
				Sqf::Value worldSpace = Sqf::Parameters(); //[]
				Sqf::Value inventory = Sqf::Parameters(); //[]
				Sqf::Value parts = Sqf::Parameters(); //[]

				Sqf::Parameters objParams;
				objParams.push_back(string("VEH"));

				string uniqueID = rs[0].convert<std::string>();
				objParams.push_back(uniqueID);

				string classname = rs[1].convert<std::string>();
				objParams.push_back(classname);

				try
				{
					string worldSpaceStr = rs[2].convert<std::string>();
					worldSpace = lexical_cast<Sqf::Value>(worldSpaceStr);
				}
				catch (boost::bad_lexical_cast)
				{
					console->log("Invalid Worldspace for ObjectID: " + uniqueID);
				}

				objParams.push_back(worldSpace);

				try
				{
					string inventoryStr = rs[3].convert<std::string>();
					inventory = lexical_cast<Sqf::Value>(inventoryStr);
				}
				catch (boost::bad_lexical_cast)
				{
					console->log("Invalid Inventory for ObjectID: " + uniqueID);
				}

				objParams.push_back(inventory);

				try
				{
					string partsStr = rs[4].convert<std::string>();
					parts = lexical_cast<Sqf::Value>(partsStr);
				}
				catch (boost::bad_lexical_cast)
				{
					console->log("Invalid Parts for ObjectID: " + uniqueID);
				}

				objParams.push_back(parts);

				double fuel = rs[5].convert<double>();
				objParams.push_back(fuel);

				double damage = rs[6].convert<double>();
				objParams.push_back(damage);

				queue.push(objParams);

				more = rs.moveNext();
			}
		}
	}
	catch (boost::bad_lexical_cast& ex)
	{
		console->log(ex.what());
	}
	catch (boost::exception&)
	{
		console->log("unknown boost exception");
	}
	catch (Data::MySQL::StatementException& ex)
	{
		console->log(ex.message());
	}
	catch (Data::MySQL::TransactionException& ex)
	{
		console->log(ex.message());
	}
	catch (std::exception& ex)
	{
		console->log(ex.what());
	}
}

void Database::populateHouses(std::queue<Sqf::Parameters>& queue)
{
	try
	{
		Poco::Data::Statement stmt((*activeSession));
		stmt << "select `player_id`, `unique_id`, `building_id`, `building_name`, `worldspace`, `lock`, `explosive`, `reinforcement` from `buildings` where `instance_id` = ?", use(serverID), now;
		Poco::Data::RecordSet rs(stmt);

		if (rs.columnCount() >= 1)
		{
			bool more = rs.moveFirst();
			while (more)
			{
				Sqf::Parameters objParams;
				objParams.push_back(string("HOUSE"));

				string playerID = rs[0].convert<std::string>();
				objParams.push_back(playerID);

				string uniqueID = rs[1].convert<std::string>();
				objParams.push_back(uniqueID);

				string buildingID = rs[2].convert<std::string>();
				objParams.push_back(buildingID);

				string buildingName = rs[3].convert<std::string>();
				objParams.push_back(buildingName);

				try
				{
					string worldSpace = rs[4].convert<std::string>();
					Sqf::Value worldSpaceArray = lexical_cast<Sqf::Value>(worldSpace);
					objParams.push_back(worldSpaceArray);
				}
				catch (boost::bad_lexical_cast)
				{
					console->log("Invalid Worldspace for House: " + buildingID);
				}

				string lock = rs[5].convert<std::string>();
				objParams.push_back(lock);

				bool explosive = rs[6].convert<bool>();
				objParams.push_back(explosive);

				int reinforcement = rs[7].convert<int>();
				objParams.push_back(reinforcement);

				queue.push(objParams);

				more = rs.moveNext();
			};
		};
	}
	catch (std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
	};
};

void Database::populateTraps(std::queue<Sqf::Parameters>& queue)
{
	try
	{
		Poco::Data::Statement stmt((*activeSession));
		stmt << "select `unique_id`, `classname`, `worldspace` from `instance_traps` where `instance_id` = ?", use(serverID);
		stmt.execute();
		Poco::Data::RecordSet rs(stmt);

		if (rs.columnCount() >= 1)
		{
			bool more = rs.moveFirst();
			while (more)
			{
				Sqf::Parameters objParams;
				objParams.push_back(string("TRAP"));

				string uniqueID = rs[0].convert<std::string>();
				objParams.push_back(uniqueID);

				string classname = rs[1].convert<std::string>();
				objParams.push_back(classname);

				try
				{
					string worldSpace = rs[2].convert<std::string>();
					Sqf::Value worldSpaceArray = lexical_cast<Sqf::Value>(worldSpace);
					objParams.push_back(worldSpaceArray);
				}
				catch (boost::bad_lexical_cast)
				{
					console->log("Invalid Worldspace for Trap: " + uniqueID);
				}

				queue.push(objParams);

				more = rs.moveNext();
			};
		};
	}
	catch (std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
	};
};

void Database::populateHackData(int requestID, std::queue<Sqf::Parameters>& queue)
{
	Poco::Data::Statement stmt((*activeSession));
	switch (requestID)
	{
		//Weapons
		case 1:
		{
			  stmt << "select `weapon` from `hack_weapons` where 1";
			  break;
		}
		//Magazines
		case 2:
		{
			  stmt << "select `magazine` from `hack_magazines` where 1";
			  break;
		}
		//Vars
		case 3:
		{
			  stmt << "select `var` from `hack_vars` where 1";
			  break;
		}
		//Scripts
		case 4:
		{
			  stmt << "select `script` from `hack_scripts` where 1";
			  break;
		}
	}

	stmt.execute();
	Poco::Data::RecordSet rs(stmt);
	if (rs.columnCount() >= 1)
	{
		bool more = rs.moveFirst();
		while (more)
		{
			Sqf::Parameters objParams;

			string str = rs[0].convert<std::string>();
			objParams.push_back(str);

			queue.push(objParams);

			more = rs.moveNext();
		};
	};
};

Sqf::Value Database::fetchCharacterInitial(string playerID, string playerName)
{
	try
	{
		bool newPlayer = false;
		std::string guid = VAC::convertSteamIDtoBEGUID(playerID);

		//Make Sure Player Exists in the Database
		{
			Poco::Data::Statement stmt((*activeSession));
			stmt << "select `name` from `profile` WHERE `unique_id` = ?", use(playerID), now;
			Poco::Data::RecordSet rs(stmt);

			if (rs.rowCount() > 0 && rs.columnCount() >= 1)
			{
				std::string dbName = rs[0].convert<std::string>();
				if (playerName != dbName)
				{
					Poco::Data::Statement name_stmt((*activeSession));
					name_stmt << "update `profile` set `name` = ? where `unique_id` = ?", use(playerName), use(playerID), now;
				};
			} else {
				newPlayer = true;
				Poco::Data::Statement profile_stmt((*activeSession));
				profile_stmt << "insert into `profile` (`unique_id`, `name`, `guid`) values (?, ?, ?)", use(playerID), use(playerName), use(guid), now;
			}
		}

		//Get Characters From Database
		Poco::Data::Statement character_stmt((*activeSession));
		character_stmt << "select s.`id`, s.`lastserver`, s.`activeserver`, s.`inventory`, s.`backpack`, "
			"timestampdiff(minute, s.`start_time`, s.`last_updated`) as `SurvivalTime`, "
			"timestampdiff(minute, s.`last_updated`, NOW()) as `MinsLastUpdate`, "
			//"timestampdiff(minute, s.`last_drank`, NOW()) as `MinsLastDrank`, "
			"s.`dog` from `survivor` s join `instance` i on s.`world_id` = i.`world_id` and i.`id` = ? where s.`unique_id` = ? and s.`is_dead` = 0", use(serverID), use(playerID), now;
		Poco::Data::RecordSet rs(character_stmt);

		bool newChar = false; //not a new char
		int characterID = -1; //invalid charid
		int lastServer = 0;
		int activeServer = 0;
		Sqf::Value inventory = lexical_cast<Sqf::Value>("[]"); //empty inventory
		Sqf::Value backpack = lexical_cast<Sqf::Value>("[]"); //empty backpack
		Sqf::Value survival = lexical_cast<Sqf::Value>("[0,0,0]"); //0 mins alive, 0 mins since last ate, 0 mins since last drank
		Sqf::Value medical = lexical_cast<Sqf::Value>("[false,false,false,false,false,false,false,0,12000,[],[0,0],0]");
		string model = ""; //empty models will be defaulted by scripts
		Sqf::Value dog = lexical_cast<Sqf::Value>("[]"); //dog class id, dog hunger, dog thirst

		if (rs.rowCount() > 0 && rs.columnCount() >= 1)
		{
			characterID = rs[0].convert<int>();

			lastServer = rs[1].convert<int>();
			activeServer = rs[2].convert<int>();

			string inv = rs[3].convert<std::string>();
			inventory = lexical_cast<Sqf::Value>(inv);

			string bpk = rs[4].convert<std::string>();
			backpack = lexical_cast<Sqf::Value>(bpk);

			{
				Sqf::Parameters& survivalArr = boost::get<Sqf::Parameters>(survival);
				int i;
				i = rs[5].convert<int>();
				survivalArr[0] = i;
				i = rs[6].convert<int>();
				survivalArr[1] = i;
			}

			string dg = rs[7].convert<std::string>();
			dog = lexical_cast<Sqf::Value>(dg);

		} else {
			newChar = true;

			//Insert New Char into DB
			Poco::Data::Statement stmt((*activeSession));
			stmt << "insert into `survivor` (`unique_id`, `world_id`) select ?, i.`world_id` from `instance` i where i.`id` = ?", use(playerID), use(serverID), now;

			//Get The New Character's ID
			{
				Poco::Data::Statement stmt((*activeSession));
				stmt << "select s.`id` from `survivor` s join `instance` i on s.world_id = i.world_id and i.id = ? where s.`unique_id` = ? and s.`is_dead` = 0", use(serverID), use(playerID), now;
				Poco::Data::RecordSet rs(stmt);

				if (rs.rowCount() > 0 && rs.columnCount() >= 1) {
					characterID = rs[0].convert<int>();
				} else {
					Sqf::Parameters retVal;
					retVal.push_back(string("ERROR"));
					return retVal;
				}
			}
		}

		//Always Push Back Full Login Details
		Sqf::Parameters retVal;
		retVal.push_back(string("PASS"));
		retVal.push_back(newChar);
		retVal.push_back(lexical_cast<string>(characterID));
		retVal.push_back(inventory);
		retVal.push_back(survival);
		retVal.push_back(lastServer);
		retVal.push_back(activeServer);
		retVal.push_back(dog);
		retVal.push_back(guid);
		return retVal;
	}
	catch (Poco::Exception const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (boost::bad_lexical_cast const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (boost::bad_get const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (std::exception const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
}

Sqf::Value Database::fetchCharacterDetails(int characterID)
{
	try
	{
		Poco::Data::Statement stmt((*activeSession));
		stmt << "select s.`worldspace`, s.`medical`, s.`zombie_kills`, s.`headshots`, s.`survivor_kills`, s.`state`, s.`class`, p.`ranger`, p.`outlaw`, p.`hunter`, p.`nomad`, p.`survivalist`, p.`engineer`, p.`undead`, p.`total_survivor_kills`, p.`clan` " <<
			"from `survivor` s join `profile` p on s.`unique_id` = p.`unique_id` where s.`id` = ?", use(characterID), now;

		Sqf::Parameters retVal;

		Poco::Data::Statement updateInstance((*activeSession));
		updateInstance << "update `survivor` set `lastserver` = ?, `activeserver` = ? where `id` = ?", use(serverID), use(serverID), use(characterID);
		updateInstance.executeAsync();

		Poco::Data::RecordSet rs(stmt);
		if (rs.rowCount() > 0 && rs.columnCount() >= 1)
		{
			Sqf::Value worldSpace = Sqf::Parameters(); //empty worldspace
			Sqf::Value medical = Sqf::Parameters(); //script will fill this in if empty
			Sqf::Value stats = lexical_cast<Sqf::Value>("[0,0,0]"); //killsZ, headZ, killsH
			Sqf::Value currentState = Sqf::Parameters(); //empty state (aiming, etc)

			int myClass = 0;
			int ranger = 0;
			int outlaw = 0;
			int hunter = 0;
			int nomad = 0;
			int survivalist = 0;
			int engineer = 0;
			int undead = 0;
			int humanKills = 0;
			string clan = "0";

			//Get Worldspace
			try
			{
				string ws = rs[0].convert<std::string>();
				worldSpace = lexical_cast<Sqf::Value>(ws);
			}
			catch (boost::bad_lexical_cast)
			{
				console->log("Invalid Worldspace for Character: " + characterID);
			}


			//Get Medical
			try
			{
				string med = rs[1].convert<std::string>();
				medical = lexical_cast<Sqf::Value>(med);
			}
			catch (boost::bad_lexical_cast)
			{
				console->log("Invalid Medical for Character: " + characterID);
			}

			//Get Stats
			try
			{
				Sqf::Parameters& statsArr = boost::get<Sqf::Parameters>(stats);
				int i;
				i = rs[2].convert<int>();
				statsArr[0] = i;
				i = rs[3].convert<int>();
				statsArr[1] = i;
				i = rs[4].convert<int>();
				statsArr[2] = i;
			}
			catch (boost::bad_lexical_cast)
			{
				console->log("Invalid Stats for Character: " + characterID);
			}

			//Get Current State
			try
			{
				string state = rs[5].convert<std::string>();
				currentState = lexical_cast<Sqf::Value>(state);
			}
			catch (boost::bad_lexical_cast)
			{
				console->log("Invalid State for Character: " + characterID);
			}

			myClass = rs[6].convert<int>();
			ranger = rs[7].convert<int>();
			outlaw = rs[8].convert<int>();
			hunter = rs[9].convert<int>();
			nomad = rs[10].convert<int>();
			survivalist = rs[11].convert<int>();
			engineer = rs[12].convert<int>();
			undead = rs[13].convert<int>();
			humanKills = rs[14].convert<int>();

			clan = rs[15].convert<string>();

			retVal.push_back(string("PASS"));
			retVal.push_back(medical);
			retVal.push_back(stats);
			retVal.push_back(currentState);
			retVal.push_back(worldSpace);

			retVal.push_back(myClass);
			retVal.push_back(ranger);
			retVal.push_back(outlaw);
			retVal.push_back(hunter);
			retVal.push_back(nomad);
			retVal.push_back(survivalist);
			retVal.push_back(engineer);
			retVal.push_back(undead);
			retVal.push_back(clan);

			//Wait for Async Update Player Model / Instance to finish
			updateInstance.wait();
		} else {
			retVal.push_back(string("ERROR"));
		}

		return retVal;
	}
	catch (Poco::Exception const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (boost::bad_lexical_cast const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (boost::bad_get const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (std::exception const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
};

bool Database::clearActiveServer(int characterID, string playerID)
{
	if (characterID > 0)
	{
		//Update Active Server
		Poco::Data::Statement stmt((*activeSession));
		stmt << "update `survivor` set `activeserver` = 0 where `id` = ?", use(characterID), now;

		//Update Last Login
		Poco::Data::Statement lastLogin((*activeSession));
		lastLogin << "update `survivor` set `last_updated` = CURRENT_TIMESTAMP where `id` = ?", use(characterID), now;
	}

	Poco::Data::Statement stmt((*activeSession));
	stmt << "update `survivor` set `activeserver` = 0 where `unique_id` = ? AND `activeserver` = ?", use(playerID), use(serverID), now;

	return true;
};

Sqf::Value Database::killCharacter(int characterID)
{
	//Update Stats Profile
	{
		Poco::Data::Statement stmt((*activeSession));
		stmt << "update `profile` p inner join `survivor` s on s.`unique_id` = p.`unique_id` set p.`survival_attempts` = p.`survival_attempts` + 1, p.`total_survivor_kills` = p.`total_survivor_kills` + s.`survivor_kills`, p.`total_zombie_kills` = p.`total_zombie_kills` + s.`zombie_kills`, p.`total_headshots` = p.`total_headshots` + s.`headshots`, p.`total_survival_time` = p.`total_survival_time` + s.`survival_time` where s.`id` = ? and s.`is_dead` = 0", use(characterID), now;
	}

	//Kill Character
	Poco::Data::Statement stmt((*activeSession));
	stmt << "update `survivor` set `is_dead` = 1 where `id` = ?", use(characterID), now;

	//Return Success
	Sqf::Parameters retVal;
	retVal.push_back(string("PASS"));
	retVal.push_back(lexical_cast<string>(characterID));
	return retVal;

};

typedef map<string, Sqf::Value> FieldsType;
bool Database::updateCharacter(int characterID, const FieldsType& fields)
{
	map<string, string> sqlFields;

	for (auto it = fields.begin(); it != fields.end(); ++it)
	{
		const string& name = it->first;
		const Sqf::Value& val = it->second;

		//arrays
		if (name == "worldspace" || name == "inventory" || name == "backpack" || name == "medical" || name == "state" || name == "dog")
			sqlFields[name] = "'" + lexical_cast<string>(val)+"'";
		//booleans
		else if (name == "just_ate" || name == "just_drank")
		{
			if (boost::get<bool>(val))
			{
				string newName = "last_ate";
				if (name == "just_drank")
					newName = "last_drank";

				sqlFields[newName] = "CURRENT_TIMESTAMP";
			}
		}
		//other stats
		else if (name == "zombie_kills" || name == "headshots" || name == "survival_time" || name == "survivor_kills" || name == "lastserver" || name == "class" || name == "ranger" || name == "outlaw" || name == "hunter" || name == "nomad" || name == "survivalist" || name == "engineer" || name == "undead")
		{
			sqlFields[name] = "'" + lexical_cast<string>(val)+"'";
		}
		//strings
		else if (name == "model")
		{
			sqlFields[name] = "'" + boost::get<string>(val) +"'";
		}
	}

	if (sqlFields.size() > 0)
	{
		string setClause = "";
		bool joinProfile = false;
		for (auto it = sqlFields.begin(); it != sqlFields.end();)
		{
			string fieldName = it->first;

			if (fieldName == "ranger" || fieldName == "outlaw" || fieldName == "hunter" || fieldName == "nomad" || fieldName == "survivalist" || fieldName == "engineer" || fieldName == "undead") {
				joinProfile = true;
				setClause += "p.`" + fieldName + "` = " + it->second;
			}
			else {
				setClause += "s.`" + fieldName + "` = " + it->second;
			}
			++it;
			if (it != sqlFields.end())
				setClause += " , ";
		}

		string query = "update `survivor` s ";
		if (joinProfile)
			query += "join `profile` p on s.`unique_id` = p.`unique_id` ";
		query += "set " + setClause + " where s.`id` = " + lexical_cast<string>(characterID);


		//bool exRes = getDB()->execute(query.c_str());
		//poco_assert(exRes == true);

		Poco::Data::Statement stmt((*activeSession));
		stmt << query, now;
	}

	return true;
};

bool Database::timestampObject(Int64 uniqueID)
{
	Poco::Data::Statement stmt((*activeSession));
	std::string uniqueIDStr = lexical_cast<string>(uniqueID);
	stmt << "UPDATE `instance_deployable` SET `last_updated` = now() WHERE `unique_id` = ? and `instance_id` = ?", use(uniqueID), use(serverID), now;
	return true;
}

bool Database::fuelObject(Int64 uniqueID, double fuel)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "UPDATE `instance_deployable` SET `fuel` = ? WHERE `unique_id` = ? and `instance_id` = ?", use(fuel), use(uniqueID), use(serverID), now;
	return true;
}

bool Database::createObject(string className, double damage, int characterID, string playerID, Sqf::Value worldSpace, Sqf::Value inventory, Sqf::Value hitPoints, double fuel, string lock, Int64 uniqueID, Int64 buildingID)
{
	std::string worldSpaceStr = lexical_cast<string>(worldSpace);
	std::string inventoryStr = lexical_cast<string>(inventory);
	std::string hitpointsStr = lexical_cast<string>(hitPoints);

	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `instance_deployable` (`unique_id`, `deployable_id`, `owner_id`, `player_id`, `instance_id`, `worldspace`, `inventory`, `Damage`,`Hitpoints`, `Fuel`, `lock`, `building_id`, `created`) " <<
		"select ?, d.id, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP from deployable d where d.class_name = ?",
		use(uniqueID), use(characterID), use(playerID), use(serverID), use(worldSpaceStr), use(inventoryStr), use(damage), use(hitpointsStr),
		use(fuel), use(lock), use(buildingID), use(className), now;
	return true;
}

bool Database::deleteObject(Int64 objectIdent)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "delete from `instance_deployable` where `unique_id` = ? and `instance_id` = ?", use(objectIdent), use(serverID), now;
	return true;
};

bool Database::updateObjectInventory(Int64 objectIdent, Sqf::Value& inventory)
{
	std::string inventoryStr = lexical_cast<string>(inventory);

	Poco::Data::Statement stmt((*activeSession));
	stmt << "update `instance_deployable` set `inventory` = ? where `unique_id` = ? and `instance_id` = ?", use(inventoryStr), use(objectIdent), use(serverID), now;
	return true;
};

Sqf::Value Database::createVehicle(string className, double damage, Sqf::Value worldSpace, Sqf::Value inventory, Sqf::Value hitPoints, double fuel,string id)
{
	std::string worldSpaceStr = lexical_cast<string>(worldSpace);
	std::string inventoryStr = lexical_cast<string>(inventory);
	std::string hitpointsStr = lexical_cast<string>(hitPoints);
	
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `instance_vehicle` (`instance_id`, `unique_id`, `classname`, `worldspace`, `inventory`, `parts`, `fuel`, `damage`, `created`) " <<
		"VALUES (?,?,?,?,?,?,?,?,CURRENT_TIMESTAMP)",
		use(serverID), use(id), use(className), use(worldSpaceStr), use(inventoryStr), use(hitpointsStr), use(fuel), use(damage), now;
	return getLastInsertId();
}

bool Database::deleteVehicle(Int64 objectIdent)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "delete from `instance_vehicle` where `id` = ? and `instance_id` = ?", use(objectIdent), use(serverID), now;
	return true;
};

bool Database::updateVehicleInventory(Int64 objectIdent, Sqf::Value& inventory)
{
	std::string inventoryStr = lexical_cast<string>(inventory);
	Poco::Data::Statement stmt((*activeSession));
	stmt << "update `instance_vehicle` set `inventory` = ? where `id` = ? and `instance_id` = ?", use(inventoryStr), use(objectIdent), use(serverID), now;
	return true;
};

bool Database::timestampVehicle(Int64 uniqueID)
{
	Poco::Data::Statement stmt((*activeSession));
	std::string uniqueIDStr = lexical_cast<string>(uniqueID);
	stmt << "UPDATE `instance_vehicle` SET `last_updated` = now() WHERE `id` = ? and `instance_id` = ?", use(uniqueID), use(serverID), now;
	return true;
}

bool Database::updateVehicleMovement(Int64 objectIdent, Sqf::Value& worldSpace, double fuel)
{
	Poco::Data::Statement stmt((*activeSession));
	std::string worldSpaceStr = lexical_cast<string>(worldSpace);
	stmt << "update `instance_vehicle` set `worldspace` = ?, `fuel` = ? where `id` = ? and `instance_id` = ?", use(worldSpaceStr), use(fuel), use(objectIdent), use(serverID), now;
	return true;
}

bool Database::updateVehicleStatus(Int64 objectIdent, Sqf::Value& hitPoints, double damage)
{
	std::string hitPointsStr = lexical_cast<string>(hitPoints);
	Poco::Data::Statement stmt((*activeSession));
	stmt << "update `instance_vehicle` set `parts` = ?, `damage` = ? where `id` = ? and `instance_id` = ?", use(hitPointsStr), use(damage), use(objectIdent), use(serverID), now;
	return true;
}

bool Database::createTrap(Int64 trapUID, string& classname, Sqf::Value& worldspace)
{
	std::string worldSpaceStr = lexical_cast<string>(worldspace);
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `instance_traps` (`unique_id`, `classname`, `worldspace`, `instance_id`, `created`) " <<
		"VALUES (?,?,?,?,CURRENT_TIMESTAMP)",
		use(trapUID), use(classname), use(worldSpaceStr), use(serverID), now;
	return true;
}

bool Database::deleteTrap(Int64 trapUID)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "delete from `instance_traps` where `unique_id` = ? and `instance_id` = ?", use(trapUID), use(serverID), now;
	return true;
}

bool Database::createHouse(string& playerID, string& uniqueID, string& buildingID, string& buildingName, Sqf::Value& worldspace, string& lock, int explosive, int reinforcement)
{
	std::string worldSpaceStr = lexical_cast<string>(worldspace);
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `buildings` (`player_id`, `unique_id`, `building_id`, `building_name`, `worldspace`, `lock`, `explosive`, `reinforcement`, `instance_id`) " <<
		"VALUES (?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE `player_id`=VALUES(`player_id`), `unique_id`=VALUES(`unique_id`), `building_id`=VALUES(`building_id`), `building_name`=VALUES(`building_name`), `worldspace`=VALUES(`worldspace`), `lock`=VALUES(`lock`), `explosive`=VALUES(`explosive`), `reinforcement`=VALUES(`reinforcement`), `instance_id`=VALUES(`instance_id`)",
		use(playerID), use(uniqueID), use(buildingID), use(buildingName), use(worldSpaceStr), use(lock), use(explosive), use(reinforcement), use(serverID), now;
	return true;
}

bool Database::deleteHouse(Int64 buildingID, Int64 buildingUID)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "delete from `buildings` where `unique_id` = ? and `building_id` = ? and `instance_id` = ?", use(buildingUID), use(buildingID), use(serverID), now;
	return true;
}

bool Database::timestampHouse(Int64 buildingID)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "UPDATE `buildings` SET `lastupdated` = now() WHERE `unique_id` = ? and `instance_id` = ?", use(buildingID), use(serverID), now;
	return true;
}

bool Database::combatLog(string playerID, string playerName, int characterID)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `combat_log` (`player_id`, `player_name`, `character_id`, `instance`) VALUES (?,?,?,?)", use(playerID), use(playerName), use(characterID), use(serverID), now;
	return true;
};

bool Database::killLog(string playerID, string playerName, string killerID, string killerName, int distance, string weapon)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `kill_log` (`player_id`, `player_name`, `killer_id`, `killer_name`, `distance`, `weapon`, `instance`) VALUES (?,?,?,?,?,?,?)", use(playerID), use(playerName), use(killerID), use(killerName), use(distance), use(weapon), use(serverID), now;
	return true;
};

bool Database::storageLog(string playerID, string playerName, string deployableID, string deployableName, string ownerID)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `storage_log` (`player_id`, `player_name`, `deployable_id`, `deployable_name`, `owner_id`, `instance`) VALUES (?,?,?,?,?,?)", use(playerID), use(playerName), use(deployableID), use(deployableName), use(ownerID), use(serverID), now;
	return true;
};

bool Database::hackLog(string playerID, string playerName, string reason)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `bans_pid` (`player_id`, `name`, `reason`, `instance`) VALUES (?,?,?,?)", use(playerID), use(playerName), use(reason), use(serverID), now;
	return true;
};

bool Database::kickLog(string name, string guid, string kick)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `battleye` (`name`, `guid`, `message`, `instance`) VALUES (?,?,?,?)", use(name), use(guid), use(kick), use(serverID), now;
	return true;
};

Sqf::Value Database::getLastInsertId()
{
	try
	{
		Poco::Data::Statement stmt((*activeSession));
		stmt << "SELECT LAST_INSERT_ID()", now;

		Sqf::Parameters retVal;

		Poco::Data::RecordSet rs(stmt);
		if (rs.rowCount() > 0 && rs.columnCount() >= 1)
		{
			Sqf::Value lastInsertId;
			try
			{
				string id = rs[0].convert<string>();
				lastInsertId = lexical_cast<Sqf::Value>(id);
				retVal.push_back(lastInsertId);
			}
			catch (boost::bad_lexical_cast)
			{
				console->log("Error getting last inserted ID!");
			}
		}
		else
		{
			retVal.push_back("ERROR");
		}
		return retVal;
	}
	catch (Poco::Exception const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (boost::bad_lexical_cast const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (boost::bad_get const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
	catch (std::exception const& ex)
	{
		Sqf::Parameters retVal;
		retVal.push_back(string("ERROR"));
		console->log(ex.what());
		return retVal;
	}
};

bool Database::adminLog(string playerID, string playerName, string action)
{
	Poco::Data::Statement stmt((*activeSession));
	stmt << "insert into `admin_log` (`player_id`, `name`, `action`, `instance`) VALUES (?,?,?,?)", use(playerID), use(playerName), use(action), use(serverID), now;
	return true;
};