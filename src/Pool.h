#ifndef POOL_H
#define POOL_H
#include <list>
#include "stdio.h"
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <utility> 


typedef boost::unique_lock<boost::mutex> scoped_lock;

class Worker;

template <typename T>
class Future{
public:
	Future(int id, Worker* workerPrm):
			done(false), canceled(false), taskId(id), worker(workerPrm){}
	bool isDone() const {return done;}
	bool isCanceled() const {return canceled;}
	int getTaskId() const {return taskId;}
	void cancel();
	T get();
private:
	bool done;
	bool canceled;
	const int taskId;
	const Worker* worker;
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
	template <typename T>
	Future<T> setTask(const Callable<T>* task);
	Callable<void*>* getCurrentTask(){return currentTask;}
	bool isWaiting() const {return waiting;}
	void run();
	boost::thread thread;
	bool deleted;
	boost::condition_variable* task_cond;
	void cleans();
	void* ret;	
	boost::mutex* mtx;
	Callable<void*>* currentTask;
private:
	bool waiting;
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
		delete &(*(it))->thread;
		(*(it))->deleted = true;
		(*(it))->task_cond->notify_all();
	}
	printf("end destrunction\n");
}


template <typename T>
Future<T> Worker::setTask(const Callable<T>* task){
	scoped_lock lock(*mtx);
	waiting = false;
	this->currentTask = (Callable<void*>*)task;
	task_cond->notify_all();
	printf("task setted. notify\n");
	return Future<T>(task->getTaskId(), this);
}


Worker::Worker(){
	mtx = new boost::mutex();
	task_cond = new boost::condition_variable();
	deleted = false;
	ret = (void*)100;
	currentTask = 0;
}


void Worker::cleans(){
	printf ("run cleans\n");
	delete task_cond;
	delete mtx;
}

void Worker::run(){
	while(true){
		scoped_lock lock(*mtx);
		while (this->currentTask == 0){
			printf("start sleeping\n");
			task_cond->wait(lock);
			printf("stop sleeping\n");
			if (deleted){
				cleans();
				return;
			}
		}
		// mtx->lock();
		printf("start calc\n");
		ret = this->currentTask->call();
		this->currentTask = 0;
		task_cond->notify_all();
		printf("end calc, notify\n");
		// mtx->unlock();
	}
}

template<typename T>
T Future<T>::get(){
	scoped_lock lock(*(this->worker->mtx));
	while(this->worker->currentTask != 0){
		printf("start waiting for res\n");
		this->worker->task_cond->wait(lock);
	}
	void* ret = this->worker->ret;
	printf("get called %d \n", (T)ret);
	return (T)ret;
}


#endif
