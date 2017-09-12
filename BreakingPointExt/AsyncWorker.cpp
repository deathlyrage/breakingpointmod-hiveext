
#include "AsyncWorker.h"
#include "BreakingPointExt.h"

extern BreakingPointExt * breakingPointExt;
//extern ArmaConsole * console;

AsyncWorker::AsyncWorker(bool a_debug, int a_size, int a_delay)
{
	thread_run_flag = new std::atomic<bool>(false);
	commands = new ReaderWriterQueue<std::string>(a_size);
	low_commands = new ReaderWriterQueue<std::string>(a_size);
	delay = a_delay;
	debug = a_debug;
}

AsyncWorker::~AsyncWorker()
{
	delete commands;
	delete thread_run_flag;
}

void AsyncWorker::run()
{
	std::string command;
	*thread_run_flag = true;

	while (*thread_run_flag)
	{
		//Process Async Tickets
		if (commands->peek())
		{
			//Fetch Item From Queue
			if (commands->try_dequeue(command))
			{
				//Call Extension Command
				breakingPointExt->callExtension(command);
			}
		} else {
			//Process Tickets That Require A Result
			if (id > cur_id && mutex.tryLock())
			{
				//Fetch Ticket
				WorkerData ticket = tickets[++cur_id];
				mutex.unlock();

				//Process Ticket
				ticket.data = breakingPointExt->callExtension(ticket.data);
				ticket.ready = true;

				//Save Ticket
				mutex.lock();
				tickets[cur_id] = ticket;
				mutex.unlock();
			} else {
				//Check If There is No Important Tickets so we can do unimportant ones.
				if (commands->peek() == nullptr && id <= cur_id)
				{
					//Process Unimportant Tickets
					if (low_commands->peek())
					{
						//Fetch Item From Queue
						if (low_commands->try_dequeue(command))
						{
							//Call Extension Command
							breakingPointExt->callExtension(command);
						}
					} else {
						//Only Sleep If There Is Nothing To Do.
						Poco::Thread::sleep(delay);
					}
				}
			}
		}
	}
}

long int AsyncWorker::createTicket(std::string cmd)
{
	WorkerData ticket;
	ticket.data = cmd; // extract params
	mutex.lock();
	tickets.insert(std::pair<long int, WorkerData>(++id, ticket)); // add ticket to the queue
	mutex.unlock();
	return id;
}

void AsyncWorker::stop()
{
	*thread_run_flag = false;
}

void AsyncWorker::execute(std::string cmd, bool important)
{
	if (important)
	{
		commands->enqueue(cmd);
	} else {
		low_commands->enqueue(cmd);
	}
}

bool AsyncWorker::ticketExists(long int ticketID)
{
	return (tickets.find(ticketID) != tickets.end());
}

bool AsyncWorker::ticketReady(long int ticketID)
{
	return (tickets[ticketID].ready);
}

std::string AsyncWorker::ticketResult(long int ticketID)
{
	mutex.lock();
	string result = tickets[ticketID].data;
	tickets.erase(ticketID);
	mutex.unlock();
	return result;
}