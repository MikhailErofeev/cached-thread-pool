#ifndef POOL_H
#define POOL_H
#include <list>
#include "stdio.h"
#include <boost/thread/thread.hpp>

template <typename T>
class Future{
public:
	Future(int);
	inline bool isDone() const {return done;}
	inline bool isCanceled() const {return canceled;}
	inline int getTaskId() const {return taskId;}
	void cancel();
	T get();
private:
	bool done; //can't name to isDone????
	bool canceled;
	const int taskId;
};

template <typename T>
class Callable{
public:
	Callable():taskId(generateTaskId()){}
	virtual T call() = 0;
	inline int getTaskId() const{return taskId;}
private:
	int generateTaskId(){
		static volatile int N = 0;
		return N++;
	}
	
	const int taskId;
};

class Worker{
public:
	template <typename T>
	Future<T> setTask(const Callable<T>* task);
	inline Callable<void*>* getCurrentTask(){return currentTask;}
	inline bool isWaiting() const {return waiting;}
	void run();
private:
	bool waiting;
	Callable<void*>* currentTask;
	boost::unique_lock<boost::mutex> mtx_;
	boost::condition_variable cond;
};



class Pool{
public:
	Pool(const int hotThreads, const double timeout);
	virtual ~Pool();
	template <typename T>
	Future<T> submit(const Callable<T>* c);
	inline int getHotThreads() const {return hotThreads;}
	inline double getTimeout() const {return timeout;}
private:
	std::list<Worker*> workers;
	const int hotThreads;
	const double timeout;
};



Pool::Pool(const int hotThreadsParam, const double timeoutParam): 
	hotThreads(hotThreadsParam), timeout(timeoutParam) {
	for (int i = 0; i < hotThreads; i++){
		Worker* worker = new Worker();
		boost::thread(boost::bind(&Worker::run, worker));
		workers.push_back(worker);
	}
}

Pool::~Pool(){}

template <typename T>
Future<T> Pool::submit(const Callable<T>* task){
	return (*workers.begin())->setTask(task);
}

template <typename T>
Future<T>::Future(int id):done(false), canceled(false), taskId(id){}

template <typename T>
Future<T> Worker::setTask(const Callable<T>* task){
	waiting = false;
	this->currentTask = (Callable<void*>*)task;
	cond.notify_one();
	return Future<T>(task->getTaskId());
}

void Worker::run(){
	while(true){
		printf("worker run 1\n");
		while (this->currentTask != 0){
			printf("worker - go wait\n");
			cond.wait(mtx_);
		}
		void* ret = this->currentTask->call();
	}
}


#endif




