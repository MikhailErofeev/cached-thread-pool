#ifndef POOL_H
#define POOL_H
#include <list>
#include "stdio.h"
#include <boost/thread/thread.hpp>
#include <utility> 

template <typename T>
class Future{
public:
	Future(int);
	bool isDone() const {return done;}
	bool isCanceled() const {return canceled;}
	int getTaskId() const {return taskId;}
	void cancel();
	T get();
private:
	bool done;
	bool canceled;
	const int taskId;
};

template <typename T>
class Callable{
public:
	Callable():taskId(generateTaskId()){}
	virtual T call() = 0;
	int getTaskId() const{return taskId;}
private:
	int generateTaskId(){
		static volatile int N = 0;
		return N++;
	}
	
	const int taskId;
};

class Worker{
public:
	Worker();
	~Worker();
	template <typename T>
	Future<T> setTask(const Callable<T>* task);
	Callable<void*>* getCurrentTask(){return currentTask;}
	bool isWaiting() const {return waiting;}
	void run();
	boost::thread thread;
private:
	bool waiting;
	Callable<void*>* currentTask;
	boost::condition_variable* cond;
	boost::unique_lock<boost::mutex>* mtx_;
};



class Pool{
public:
	Pool(const int hotThreads, const double timeout);
	virtual ~Pool();
	template <typename T>
	Future<T> submit(const Callable<T>* c);
	int getHotThreads() const {return hotThreads;}
	double getTimeout() const {return timeout;}
private:
	std::list<Worker* > workers;
	const int hotThreads;
	const double timeout;
};


//################ Implementation ################################

Pool::Pool(const int hotThreadsParam, const double timeoutParam): 
	hotThreads(hotThreadsParam), timeout(timeoutParam) {
	for (int i = 0; i < hotThreads; i++){
		Worker* worker = new Worker();
		boost::thread thread(boost::bind(&Worker::run, worker));
		worker->thread.swap(thread);
		//@TODO start here
		workers.push_back(worker);
	}
}

template <typename T>
Future<T> Pool::submit(const Callable<T>* task){
	return (*workers.begin())->setTask(task);
}

Pool::~Pool(){
	printf("start destrunction\n");
	for (std::list<Worker*>::iterator it = this->workers.begin();  //@todo use C++x11?
		it != this->workers.end(); ++it){
		printf("prepare to interrupt\n");
		delete *(it);
		//while(!(*(it))->interrupted());
		//(*(it)).second->cond.notify_all();
	}
	printf("end destrunction\n");
}



template <typename T>
Future<T>::Future(int id):done(false), canceled(false), taskId(id){}

template <typename T>
Future<T> Worker::setTask(const Callable<T>* task){
	waiting = false;
	this->currentTask = (Callable<void*>*)task;
	cond->notify_one();
	return Future<T>(task->getTaskId());
}


Worker::Worker(){
	mtx_ = new boost::unique_lock<boost::mutex>(); //@fixme memory leak!!!!
	cond = new boost::condition_variable(); //@fixme memory leak!!!!
}

Worker::~Worker(){
}

void Worker::run(){
	while(true){
		printf("worker run 1\n");
		while (this->currentTask != 0){
			printf("worker - go wait\n");
			cond->wait(*mtx_);
		}
		void* ret = this->currentTask->call();
	}
}


#endif




