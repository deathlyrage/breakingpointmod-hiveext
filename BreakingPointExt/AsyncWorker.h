
#pragma once

//#include <boost/thread/mutex.hpp>
//#include <boost/lockfree/spsc_queue.hpp>

#include "readerwriterqueue.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <atomic>

#include "Poco/Runnable.h"
#include "Poco/Mutex.h"

using namespace moodycamel;

class AsyncWorker : public Poco::Runnable
{
	public:
		struct WorkerData
		{
			bool ready = false;
			std::string data = "";
		};

		AsyncWorker(bool a_debug, int a_size, int a_delay);
		~AsyncWorker();
		void run();
		void stop();
		void execute(std::string cmd, bool important = true);

		long int createTicket(std::string cmd);
		bool ticketExists(long int ticketID);
		bool ticketReady(long int ticketID);
		std::string ticketResult(long int ticketID);

	private:
		std::atomic<bool> *thread_run_flag;
		ReaderWriterQueue<std::string> * commands;
		ReaderWriterQueue<std::string> * low_commands;

		Poco::Mutex mutex;

		std::unordered_map<long int, WorkerData> tickets;
		long int id = 0; // global ticket id
		long int cur_id = 0; // current ticket id

		bool debug;
		int delay;
};