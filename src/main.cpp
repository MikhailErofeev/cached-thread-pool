#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include <iostream>
#include "Pool.h"
#include <string>
#include <map>
#include "stdio.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>


class MsSleeper: public Callable<int>{
    public:
    int sleepMss;
    MsSleeper(int ms):sleepMss(ms){}
    virtual int call(){
         boost::this_thread::sleep( boost::posix_time::milliseconds(sleepMss));
        return sleepMss;
    }
};

int main(int argc, char** argv) {
	int hot = 1;
	int timeout = 5;
	int max = 2;
	if (argc > 1){
		hot = boost::lexical_cast<int>(argv[1]);
	}
	if (argc > 2){
		timeout = boost::lexical_cast<int>(argv[2]);
	}
	if (argc > 3){
		max = boost::lexical_cast<int>(argv[3]);
	}
	std::cout << "hot: " << hot << "; timeout: " << timeout << "; max:" << max << "\n";
	Pool pool(hot, max, timeout);
	std::cout << "create (secs);\n" <<
	"get(task-id);\n" << 
	"is-done(task-id);\n" <<
	"actual-workers;\n" <<
	"cancel (task-id);\n" <<
	"is-cancel (task-id);\n" <<
	"exit.\n";
	std::map<int,Future<int>*> futures;
   	while(true){
   		 std::string command;
   		 std::cout << "\t >> ";
  		std::getline (std::cin,command);
  		std::vector<std::string> parsed;
		boost::split(parsed, command, boost::is_any_of("\t "));
  		if (parsed[0] == "exit"){
  			std::cout << "waiting for tasks...\n";
  			return 0;
  		}else if (parsed[0] == "create"){
  			int secs = boost::lexical_cast<int>(parsed[1]);
  			Future<int>* future = pool.submit(new MsSleeper(secs*1000));
  			std::cout << "task id =  " << future->getTaskId() << "\n";
  			futures[future->getTaskId()] = future;
  		}else if (parsed[0] == "get"){
  			int id = boost::lexical_cast<int>(parsed[1]);
  			Future<int>* future = futures[id];
  			std::cout << "sleep time was " << future->get() << " ms\n";
  		}else if (parsed[0] == "is-done"){
  			int id = boost::lexical_cast<int>(parsed[1]);
  			Future<int>* future = futures[id];
  			std::cout << future->isDone() << "\n";
  		}else if (parsed[0] == "cancel"){
  			int id = boost::lexical_cast<int>(parsed[1]);
  			Future<int>* future = futures[id];
  			future->cancel();
  			std::cout << "ok \n";
  		}else if (parsed[0] == "is-cancel"){
  			int id = boost::lexical_cast<int>(parsed[1]);
  			Future<int>* future = futures[id];
  			std::cout << future->isCanceled() << "\n";
  		}else if (parsed[0] == "actual-workers"){
  			std::cout << pool.getActualWorkersCount() << "\n";
  		}
  		else{
  			std::cout << "unknown \"" << command <<  "\"\n";
  		}
   	}
}
