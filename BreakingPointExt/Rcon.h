/*
Copyright (C) 2012 Prithu "bladez" Parker <https://github.com/bladez-/Rcon>
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

* Change Log
* Changed Code to use Poco Net Library
*/


#pragma once

#include <boost/crc.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/NetException.h>

#include <Poco/Stopwatch.h>

#include <Poco/ExpireCache.h>

#include <atomic>
#include <unordered_map>

class Rcon : public Poco::Runnable
{
	public:
		Rcon();
		~Rcon();

		void init(int a_totalSlots, int a_reservedSlots, bool fireDeamonActive, std::string a_serviceName, std::string a_fireDaemonPath, bool a_whitelist);
		void updateLogin(std::string address, int port, std::string password);

		void run();
		void disconnect();
		bool status();

		bool overReservedSlots();
	
		void addCommand(std::string command);

		struct Player
		{
			std::string name = "";
			std::string guid = "";
		};

		void kick(int playerIndex, std::string reason);
		void say(int playerIndex, std::string message);

		void kick(std::string guid, std::string reason);
		void say(std::string guid, std::string message);

		void lock();
		void unlock();

		void loadScripts();
		void loadEvents();

		void restart();
	protected:
		std::unordered_map<int, Player> players;
		boost::mutex mutex_players;
		int playerCount;

		int totalSlots;
		int reservedSlots;
		bool fireDeamonActive;
		std::string serviceName;
		std::string fireDaemonPath;
		int lastWarningMinute;
		bool whitelist;

		std::atomic<bool> *locked;
	private:
		void msgRecieved(std::string msg);
		
		typedef std::pair< int, std::unordered_map < int, std::string > > RconMultiPartMsg;

		struct RconPacket
		{
			char *cmd;
			char cmd_char_workaround;
			unsigned char packetCode;
		};
		RconPacket rcon_packet;

		struct RconLogin
		{
			char *password;
			std::string address;
			int port;

			bool auto_reconnect;
		};
		RconLogin rcon_login;

		Poco::Net::DatagramSocket dgs;

		Poco::Stopwatch rcon_timer;

		char buffer[4096];
		int buffer_size;

		std::string keepAlivePacket;

		boost::crc_32_type crc32;

		// Mutex Locks
		std::atomic<bool> *rcon_run_flag;
		std::atomic<bool> *rcon_login_flag;

		std::vector< std::string > rcon_commands;
		boost::mutex mutex_rcon_commands;

		// Functions
		void connect();
		void mainLoop();

		void createKeepAlive();
		void sendKeepAlive();
		void sendPacket();
		void extractData(int pos, std::string &result);
};
