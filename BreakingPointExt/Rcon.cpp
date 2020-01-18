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
* Changed Code to use Poco Net Library & Poco Checksums
*/

#include <boost/crc.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/NetException.h>
#include <Poco/Timestamp.h>
#include <Poco/DateTimeFormatter.h>

#include <Poco/Exception.h>
#include <Poco/Stopwatch.h>

#include <Poco/AbstractCache.h>
#include <Poco/ExpireCache.h>
#include <Poco/SharedPtr.h>

#include <Poco/NumberFormatter.h>
#include <Poco/Thread.h>

#include <atomic>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>

#include "Rcon.h"

#include "ArmaConsole.h"

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "BreakingPointExt.h"

//#include <Windows.h>
#include <stdio.h>
#include <tchar.h>

extern ArmaConsole * console;
extern BreakingPointExt * breakingPointExt;

Rcon::Rcon()
{
	rcon_run_flag = new std::atomic<bool>(false);
	rcon_login_flag = new std::atomic<bool>(false);
	locked = new std::atomic<bool>(false);
}

Rcon::~Rcon()
{
	delete rcon_run_flag;
	delete rcon_login_flag;
	delete locked;
}

void Rcon::sendPacket()
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(rcon_packet.packetCode);
	if (rcon_packet.packetCode == 0x01)
	{
		cmdStream.put(0x00); // seq number
		cmdStream << rcon_packet.cmd;
	}
	else if (rcon_packet.packetCode == 0x02)
	{
		cmdStream.put(rcon_packet.cmd_char_workaround);
	}
	else if (rcon_packet.packetCode == 0x00)
	{
		cmdStream << rcon_packet.cmd;
	}
	std::string cmd = cmdStream.str();
	crc32.reset();
	crc32.process_bytes(cmd.data(), cmd.length());
	long int crcVal = crc32.checksum();
	std::stringstream hexStream;
	hexStream << std::setfill('0') << std::setw(sizeof(int) * 2);
	hexStream << std::hex << crcVal;
	std::string crcAsHex = hexStream.str();
	unsigned char reversedCrc[4];
	unsigned int x;
	std::stringstream converterStream;
	for (int i = 0; i < 4; i++)
	{
		converterStream << std::hex << crcAsHex.substr(6 - (2 * i), 2).c_str();
		converterStream >> x;
		converterStream.clear();
		reversedCrc[i] = x;
	}
	// Create Packet
	std::stringstream cmdPacketStream;
	cmdPacketStream.put(0x42); // B
	cmdPacketStream.put(0x45); // E
	cmdPacketStream.put(reversedCrc[0]); // 4-byte Checksum
	cmdPacketStream.put(reversedCrc[1]);
	cmdPacketStream.put(reversedCrc[2]);
	cmdPacketStream.put(reversedCrc[3]);
	cmdPacketStream << cmd;
	std::string packet = cmdPacketStream.str();
	dgs.sendBytes(packet.data(), packet.size());
}


void Rcon::createKeepAlive()
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(0x01);
	cmdStream.put(0x00); // Seq Number
	cmdStream.put('\0');
	std::string cmd = cmdStream.str();
	crc32.reset();
	crc32.process_bytes(cmd.data(), cmd.length());
	long int crcVal = crc32.checksum();
	std::stringstream hexStream;
	hexStream << std::setfill('0') << std::setw(sizeof(int) * 2);
	hexStream << std::hex << crcVal;
	std::string crcAsHex = hexStream.str();
	unsigned char reversedCrc[4];
	unsigned int x;
	std::stringstream converterStream;
	for (int i = 0; i < 4; i++)
	{
		converterStream << std::hex << crcAsHex.substr(6 - (2 * i), 2).c_str();
		converterStream >> x;
		converterStream.clear();
		reversedCrc[i] = x;
	}
	// Create Packet
	std::stringstream cmdPacketStream;
	cmdPacketStream.put(0x42); // B
	cmdPacketStream.put(0x45); // E
	cmdPacketStream.put(reversedCrc[0]); // 4-byte Checksum
	cmdPacketStream.put(reversedCrc[1]);
	cmdPacketStream.put(reversedCrc[2]);
	cmdPacketStream.put(reversedCrc[3]);
	cmdPacketStream << cmd;
	cmdPacketStream.str();
	keepAlivePacket = cmdPacketStream.str();
}

void Rcon::sendKeepAlive()
{
	// Keep Alive
	//console->log("Keep Alive Sending", ArmaConsole::LogType::DEBUG);
	rcon_timer.restart();
	dgs.sendBytes(keepAlivePacket.data(), keepAlivePacket.size());
	//console->log("Keep Alive Sent", ArmaConsole::LogType::DEBUG);
}


void Rcon::extractData(int pos, std::string &result)
{
	std::stringstream ss;
	for (size_t i = pos; i < buffer_size; ++i)
	{
		ss << buffer[i];
	}
	result = ss.str();
}

void Rcon::kick(int playerIndex, std::string reason)
{
	std::string command = "kick " + boost::lexical_cast<string>(playerIndex)+" " + reason;
	console->log(command,ArmaConsole::LogType::DEBUG);
	addCommand(command);
}

void Rcon::say(int playerIndex, std::string message)
{
	std::string command = "say " + boost::lexical_cast<string>(playerIndex)+" " + message;
	console->log(command, ArmaConsole::LogType::DEBUG);
	addCommand(command);
}

void Rcon::kick(string guid, std::string reason)
{
	int playerIndex = 0;
	mutex_players.lock();
	for (auto it = players.begin(); it != players.end(); ++it)
	{
		if (it->second.guid == guid)
		{
			playerIndex = it->first;
			break;
		}
	}
	mutex_players.unlock();

	std::string command = "kick " + boost::lexical_cast<string>(playerIndex)+" " + reason;
	console->log(command, ArmaConsole::LogType::DEBUG);
	addCommand(command);
}

void Rcon::say(string guid, std::string message)
{
	int playerIndex = 0;
	mutex_players.lock();
	for (auto it = players.begin(); it != players.end(); ++it)
	{
		if (it->second.guid == guid)
		{
			playerIndex = it->first;
			break;
		}
	}
	mutex_players.unlock();

	std::string command = "say " + boost::lexical_cast<string>(playerIndex)+" " + message;
	console->log(command, ArmaConsole::LogType::DEBUG);
	addCommand(command);
}

void Rcon::lock()
{
	std::string command = "lock";
	console->log(command, ArmaConsole::LogType::DEBUG);
	addCommand(command);
	*locked = true;
}

void Rcon::unlock()
{
	std::string command = "unlock";
	console->log(command, ArmaConsole::LogType::DEBUG);
	addCommand(command);
	*locked = false;
}

bool Rcon::overReservedSlots()
{
	mutex_players.lock();
	int reservedTotal = totalSlots - reservedSlots;
	bool overReserved = (playerCount >= reservedTotal);
	mutex_players.unlock();
	return overReserved;
}

void Rcon::msgRecieved(std::string msg)
{
	try
	{
		const char * msgC = msg.c_str();
		if (!strncmp(msgC, "Verified GUID", 13))
		{
			std::vector<std::string> result;
			boost::algorithm::split(result, msg, boost::is_any_of(" "));

			//Declare Player Strings
			int index;
			Player player;

			//Calculate Information from String
			{
				//Index
				index = boost::lexical_cast<int>(result[5].erase(0, 1));

				//Name
				std::vector<std::string> result_name;
				boost::algorithm::split(result_name, msg, boost::is_any_of("#"));
				std::vector<std::string> result_name2;
				boost::algorithm::split(result_name2, result_name[1], boost::is_any_of(" "));

				result_name2.erase(result_name2.begin());

				//Default Name
				player.name = "";

				//Add Remaining Characters
				for (auto & characters : result_name2) { player.name += characters; }

				//Guid
				std::string raw_guid = result[2];
				player.guid = raw_guid.substr(1, raw_guid.size() - 2);
			}

			//Don't Include Headless Client Users
			if (player.name == "headlessclient" || player.name == "hc")
			{
				if (player.guid == "")
				{
					return;
				} else {
					kick(index, "This name is reserved for Headless Clients!");
				}
			}

			//Don't Let People Join If Server Is Locked
			if (*locked)
			{
				kick(index, "Server is Locked");
			}

			//Erase Possible Existing Player
			if (players.find(index) != players.end()) { players.erase(index); }

			//Insert Player
			mutex_players.lock();
			playerCount += 1; //Increase Player Count
			players.insert(std::pair<long int, Player>(index, player));
			mutex_players.unlock();

			//Kick (Invalid Name)
			if (breakingPointExt->invalidName(player.name,index)) return;

			//Log
			console->log("HiveExt:Connected() Name: " + player.name + " GUID: " + player.guid, ArmaConsole::LogType::DEBUG);

		}
		else if ((!strncmp(msgC, "Player", 6)) && boost::algorithm::ends_with(msg, "disconnected"))
		{
			//Get Name and Index
			std::vector<std::string> result;
			boost::algorithm::split(result, msg, boost::is_any_of(" "));
			int index = boost::lexical_cast<int>(result[1].erase(0, 1));
			std::string name = result[2];

			//Check HC
			if (name == "hc" || name == "headlessclient") return;

			//Remove Player
			mutex_players.lock();
			playerCount -= 1; //Lower Player Count
			if (players.find(index) != players.end()) { players.erase(index); }
			mutex_players.unlock();

			//Log
			console->log("HiveExt:Disconnected() Name: " + name, ArmaConsole::LogType::DEBUG);
		}
		else if (boost::algorithm::contains(msg, string("has been kicked by BattlEye")))
		{
			int index;
			std::string name;
			std::string kick;

			{
				//Get Name and Index
				std::vector<std::string> result;
				boost::algorithm::split(result, msg, boost::is_any_of(" "));
				index = boost::lexical_cast<int>(result[1].erase(0, 1));
				name = result[2];

				//Get Kick Message
				std::vector<std::string> result2;
				boost::algorithm::split(result2, msg, boost::is_any_of(":"));
				kick = result2[1];
			}
			
			//Check HC
			if (name == "hc" || name == "headlessclient") return;

			//Remove Player
			mutex_players.lock();
			playerCount -= 1; //Lower Player Count
			if (players.find(index) != players.end()) 
			{
				//Player player = players[index];
				//breakingPointExt->logKick(player.name, player.guid, kick);
				players.erase(index);
			}
			mutex_players.unlock();

			//Log
			console->log("HiveExt:Kicked() Name: " + name + " Kick: " + kick, ArmaConsole::LogType::DEBUG);
		}
	}
	catch (...)
	{
		console->log("Rcon Unknown Exception");
		mutex_players.unlock();
	}
}


void Rcon::mainLoop()
{
	*rcon_login_flag = false;

	int elapsed_seconds;
	int sequenceNum;

	// 2 Min Cache for UDP Multi-Part Messages
	Poco::ExpireCache<int, RconMultiPartMsg > rcon_msg_cache(120000);

	dgs.setReceiveTimeout(Poco::Timespan(5, 0));

	while (true)
	{
		//Check If Server Should Restart
		int timeUntilRestart = breakingPointExt->timeUntilRestart();
		int timeUntilShutdownMode = timeUntilRestart - 5;
		std::string timeUntilShutdownModeStr = boost::lexical_cast<string>(timeUntilShutdownMode);
		std::string timeUntilRestartStr = boost::lexical_cast<string>(timeUntilRestart);

		if (timeUntilRestart < 60)
		{
			//Less Then 2 Minutes until Restart Restart Process
			if (timeUntilRestart < 2)
			{
				say(-1, "Server is restarting now.");
				restart();
				return;
			}
			else {
				if (timeUntilShutdownMode != lastWarningMinute)
				{
					lastWarningMinute = timeUntilShutdownMode;
					if (timeUntilShutdownMode > 1) { say(-1, timeUntilShutdownModeStr + " Minutes until Server Shutdown Mode."); }
					else if (timeUntilShutdownMode == 1) { say(-1, timeUntilShutdownModeStr + " Minute until Server Shutdown Mode."); }
				}
			}
		}

		//Process Rcon
		try
		{
			buffer_size = dgs.receiveBytes(buffer, sizeof(buffer) - 1);
			buffer[buffer_size] = '\0';

			if (buffer[7] == 0x00)
			{
				if (buffer[8] == 0x01)
				{
					console->log("Rcon: Logged In", ArmaConsole::LogType::DEBUG);
					*rcon_login_flag = true;
					rcon_timer.restart();
				}
				else
				{
					// Login Failed
					console->log("Rcon: Failed Login... Disconnecting");
					*rcon_login_flag = false;
					disconnect();
					break;
				}
			}
			else if ((buffer[7] == 0x01) && *rcon_login_flag)
			{
				// Rcon Server Ack Message Received
				sequenceNum = buffer[8];

				// Reset Timer
				rcon_timer.restart();

				if (!((buffer[9] == 0x00) && (buffer_size > 9)))
				{
					// Server Received Command Msg
					std::string result;
					extractData(9, result);
					if (result.empty())
					{
						//console->log("Server Received Command Msg EMPTY",ArmaConsole::LogType::DEBUG);
					}
					else
					{
						//console->log("Server Received Command Msg: " + result, ArmaConsole::LogType::DEBUG);
					}
				}
				else
				{
					// Rcon Multi-Part Message Recieved
					int numPackets = buffer[10];
					int packetNum = buffer[11];

					std::string partial_msg;
					extractData(12, partial_msg);

					if (!(rcon_msg_cache.has(sequenceNum)))
					{
						// Doesn't have sequenceNum in Buffer
						RconMultiPartMsg rcon_mp_msg;
						rcon_mp_msg.first = 1;
						rcon_msg_cache.add(sequenceNum, rcon_mp_msg);

						Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_msg_cache.get(sequenceNum);
						ptrElem->second[packetNum] = partial_msg;
					}
					else
					{
						// Has sequenceNum in Buffer
						Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_msg_cache.get(sequenceNum);
						ptrElem->first = ptrElem->first + 1;
						ptrElem->second[packetNum] = partial_msg;

						if (ptrElem->first == numPackets)
						{
							// All packets Received, re-construct message
							std::string result;
							for (int i = 0; i < numPackets; ++i)
							{
								result = result + ptrElem->second[i];
							}
							//console->log("Info: " + result, ArmaConsole::LogType::DEBUG);
							rcon_msg_cache.remove(sequenceNum);
						}
					}
				}
			}
			else if (buffer[7] == 0x02)
			{
				if (!*rcon_login_flag)
				{
					// Already Logged In 
					*rcon_login_flag = true;
				}
				else
				{
					// Recieved Chat Messages
					std::string result;
					extractData(9, result);

					// Respond to Server Msgs i.e chat messages, to prevent timeout
					rcon_packet.packetCode = 0x02;
					rcon_packet.cmd_char_workaround = buffer[8];
					sendPacket();

					// Reset Timer
					rcon_timer.restart();

					//Handle Message
					msgRecieved(result);
				}
			}

			if (*rcon_login_flag)
			{
				// Checking for Commands to Send
				boost::lock_guard<boost::mutex> lock(mutex_rcon_commands);
				for (auto &rcon_command : rcon_commands)
				{
					char *cmd = new char[rcon_command.size() + 1];
					std::strcpy(cmd, rcon_command.c_str());
					delete[]rcon_packet.cmd;
					rcon_packet.cmd = cmd;
					rcon_packet.packetCode = 0x01;
					sendPacket();
				}
				// Clear Comands
				rcon_commands.clear();
				if (!*rcon_run_flag)
				{
					break;
				}
			}
			else
			{
				if (!*rcon_run_flag)
				{
					boost::lock_guard<boost::mutex> lock(mutex_rcon_commands);
					if (rcon_commands.empty())
					{
						break;
					}
				}
			}
		}
		catch (Poco::TimeoutException&)
		{
			if (!*rcon_run_flag)  // Checking Run Flag
			{
				break;
			}
			else
			{
				elapsed_seconds = rcon_timer.elapsedSeconds();
				if (elapsed_seconds >= 45)
				{
					console->log("Rcon: TIMED OUT...");
					rcon_timer.restart();
					connect();
				}
				else if (elapsed_seconds >= 30)
				{
					sendKeepAlive();
				}
				else if (*rcon_run_flag)
				{
					// Checking for Commands to Send
					boost::lock_guard<boost::mutex> lock(mutex_rcon_commands);

					for (auto &rcon_command : rcon_commands)
					{
						char *cmd = new char[rcon_command.size() + 1];
						std::strcpy(cmd, rcon_command.c_str());
						delete[]rcon_packet.cmd;
						rcon_packet.cmd = cmd;
						rcon_packet.packetCode = 0x01;
						sendPacket();
					}

					// Clear Comands
					rcon_commands.clear();
				}
			}
		}
		catch (Poco::Net::ConnectionRefusedException& e)
		{
			console->log("Rcon: Error Connect: " + e.displayText());
			disconnect();
		}
		catch (Poco::Exception& e)
		{
			console->log("Rcon: Error Rcon: " + e.displayText());
			disconnect();
		}
		catch (...)
		{
			console->log("Rcon Unknown Exception");
		}
	}
}


void Rcon::addCommand(std::string command)
{
	if (*rcon_run_flag)
	{
		boost::lock_guard<boost::mutex> lock(mutex_rcon_commands);
		rcon_commands.push_back(std::move(command));
	}
}


void Rcon::run()
{
	Poco::Net::SocketAddress sa(rcon_login.address, rcon_login.port);
	dgs.connect(sa);

	connect();
	mainLoop();
}


void Rcon::connect()
{
	*rcon_login_flag = false;
	*rcon_run_flag = true;
	*locked = false;

	// Login Packet
	rcon_packet.cmd = rcon_login.password;
	rcon_packet.packetCode = 0x00;
	sendPacket();
	console->log("Rcon: Sent Login Info", ArmaConsole::LogType::DEBUG);

	// Reset Timer
	rcon_timer.start();
}

bool Rcon::status()
{
	return *rcon_login_flag;
}


void Rcon::disconnect()
{
	*rcon_run_flag = false;
}

void Rcon::init(int a_totalSlots, int a_reservedSlots, bool a_fireDeamonActive , std::string a_serviceName, std::string a_fireDaemonPath, bool a_whitelist)
{
	whitelist = a_whitelist;
	lastWarningMinute = -1; 
	playerCount = 0;
	totalSlots = a_totalSlots;
	reservedSlots = a_reservedSlots;
	serviceName = a_serviceName;
	fireDaemonPath = a_fireDaemonPath;
	fireDeamonActive = a_fireDeamonActive;

	createKeepAlive();
}

void Rcon::updateLogin(std::string address, int port, std::string password)
{
	rcon_login.address = address;
	rcon_login.port = port;
	char *passwd = new char[password.size() + 1];
	std::strcpy(passwd, password.c_str());
	rcon_login.password = passwd;
	rcon_login.auto_reconnect = true;
}

void Rcon::restart()
{
	console->log("Rcon::Restart()", ArmaConsole::LogType::DEBUG);
	//std::string command = "start \"\" \"C:\\Program Files (x86)\\FireDaemon\\Firedaemon.exe\" --restart " + serviceName;
	if (fireDeamonActive)
	{
		std::string command = "start \"\" \"" + fireDaemonPath + "\""" --restart " + serviceName;
		system(command.c_str());
	}
}
