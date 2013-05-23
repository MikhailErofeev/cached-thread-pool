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
			done(false), canceled(false), taskId(id), worker(workerPrm){
		workerWaitingMtx = new boost::mutex();
		waitingCondition = new boost::condition_variable();
	}
	bool isDone() const {return done;}
	bool isCanceled() const {return canceled;}
	int getTaskId() const {return taskId;}
	void cancel();
	void setWorker(Worker* worker); //@FIXME FUCK FUCK FUCK
	T get();
private:
	bool done;
	bool canceled;
	const int taskId;
	Worker* worker;
	boost::mutex* workerWaitingMtx;
	boost::condition_variable* waitingCondition;
	void waitingForWorker();
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
	void setWaiting(bool waiting){this->waiting = waiting;}
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

template<typename T>
class ExecutionUnit{
public:
	Future<T>* future;
	Callable<T>* task;
	Worker* worker;
	ExecutionUnit(Future<T>* futureParam, Callable<T>* taskParam, Worker* workerParam):
		future(futureParam), task(taskParam), worker(workerParam){}
};



class Pool{
public:
	Pool(const int hotThreads, const double timeout);
	virtual ~Pool();
	template <typename T>
	Future<T> submit(Callable<T>* c);
	int getHotThreads() const {return hotThreads;}
	double getTimeout() const {return timeout;}
private:
	std::list<Worker* > workers;
	const int hotThreads;
	const double timeout;
	std::list<void* > tasksQueue;
	boost::mutex* queueMtx;
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
	queueMtx = new boost::mutex();
}

template <typename T>
Future<T> Pool::submit(Callable<T>* task){
	scoped_lock lock(*queueMtx);
	for (std::list<Worker*>::iterator it = this->workers.begin();
		it != this->workers.end(); 
		++it){
		if (!(*it)->isWaiting()){
			return (*it)->setTask(task);
		}
	}
	Worker* none = 0;
	Future<T>* future = new Future<T>(task->getTaskId(), none);
		ExecutionUnit<T>* unit = new ExecutionUnit<T>(future, task, none);
	this->tasksQueue.push_back(unit);
	return (*workers.begin())->setTask(task);
}

Pool::~Pool(){
	printf("start pool destrunction\n");
	for (std::list<Worker*>::iterator it = this->workers.begin();
		it != this->workers.end(); 
		++it){
		delete &(*(it))->thread;
		(*(it))->deleted = true;
		(*(it))->task_cond->notify_all();
	}
	printf("end pool destrunction\n");
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
	ret = (void*)0;
	currentTask = 0;
}


void Worker::cleans(){
	printf ("run worker\n");
	task_cond->notify_all();
	delete task_cond;
	delete mtx;
}

void Worker::run(){
	while(true){
		scoped_lock lock(*mtx);
		while (this->currentTask == 0){
			printf("worker start sleeping\n");
			task_cond->wait(lock);
			printf("worker stop sleeping\n");
			if (deleted){
				//cleans(); @FIXME run Worker cleans after end of thread!!!!
				return;
			}
		}
		// mtx->lock();
		printf("start calc\n");
		ret = this->currentTask->call();
		this->currentTask = 0;
		printf("end calc, notify\n");
		task_cond->notify_all();
		// mtx->unlock();
	}
}

template<typename T>
void Future<T>::setWorker(Worker* worker){
	this->worker = worker;
	waitingCondition->notify_one();
}

template<typename T>
void Future<T>::waitingForWorker(){
	scoped_lock lock(*workerWaitingMtx);
	while(worker == 0){
		waitingCondition->wait(lock);
	}
}

template<typename T>
T Future<T>::get(){	
	waitingForWorker();
	scoped_lock lock(*(this->worker->mtx));
	while(this->worker->currentTask != 0){
		printf("start waiting for res\n");
		this->worker->task_cond->wait(lock);
	}
	void* ret = this->worker->ret;
	this->worker->setWaiting(true);
	//printf("get called %d \n", (T)ret);	
	return (T)ret;
}


#endif
