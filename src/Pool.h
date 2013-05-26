#ifndef POOL_H
#define POOL_H
#include <list>
#include "stdio.h"
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <utility> 
#include <iostream>

typedef boost::unique_lock<boost::mutex> scoped_lock;

class Worker;
class Pool;

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
	void setResult(void* result); //@FIXME FUCK FUCK FUCK
	T get();
private:
	bool done;
	bool canceled;
	const int taskId;
	Worker* worker;
	T ret;
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

template<typename T>
class ExecutionUnit{
public:
	Future<T>* future;
	Callable<T>* task;
	Worker* worker;
	ExecutionUnit(Future<T>* futureParam, Callable<T>* taskParam, Worker* workerParam):
		future(futureParam), task(taskParam), worker(workerParam){}
};

class Worker{
public:
	Worker(Pool* poolPrm);
	template <typename T>
	void setTask(const ExecutionUnit<T>*);
	bool isWaiting() const {return waiting;}
	void setWaiting(bool waiting){this->waiting = waiting;}
	void run();
	boost::thread thread;
	bool deleted;
	boost::condition_variable* task_cond;
	void cleans();
	void* ret; //@FIXME don't store it here - store in Future obj!
	Pool* pool;
	boost::mutex* mtx;
	ExecutionUnit<void*>* executionUnit;
private:
	bool waiting;
	void waitForTask();
	int generateWorkerId(){
		static volatile int N = 0;
		return (int)this;
	}
	
	const int workerId;
};


class Pool{
public:
	Pool(const int hotThreads, const double timeout);
	virtual ~Pool();
	template <typename T>
	Future<T>* submit(Callable<T>* c);
	int getHotThreads() const {return hotThreads;}
	double getTimeout() const {return timeout;}
	ExecutionUnit<void*>* findTask();
	boost::mutex* queueMtx;
private:
	std::list<Worker* > workers;
	const int hotThreads;
	const double timeout;
	std::list<void* > tasksQueue;
};


//################ Implementation ################################

Pool::Pool(const int hotThreadsParam, const double timeoutParam): 
	hotThreads(hotThreadsParam), timeout(timeoutParam) {
	queueMtx = new boost::mutex();
	for (int i = 0; i < hotThreads; i++){
		Worker* worker = new Worker(this);
		boost::thread thread(boost::bind(&Worker::run, worker));
		worker->thread.swap(thread);
		//@TODO start here
		workers.push_back(worker);
	}
}

template <typename T>
Future<T>* Pool::submit(Callable<T>* task){
	printf("start submit\n");
	scoped_lock lock(*queueMtx);
	Worker* worker = 0;
	printf("choose worker for task %d\n", task->getTaskId());
	for (std::list<Worker*>::iterator it = this->workers.begin();
		it != this->workers.end(); 	++it){			
		if ((*it)->isWaiting()){
			worker = (*it);
			break;
		}
	}
	
	Future<T>* future = new Future<T>(task->getTaskId(), worker);
	ExecutionUnit<T>* unit = new ExecutionUnit<T>(future, task, worker);
	if (worker != 0){
		printf("set free worker\n");
		worker->setTask(unit);
	}else{
		printf ("add to queue, return nullable future\n");
		this->tasksQueue.push_back(unit);
	}	
	return future;
}

ExecutionUnit<void*>* Pool::findTask(){
	ExecutionUnit<void*>* ret;
	if (tasksQueue.empty()){
		printf("no tasks in queue\n");
		ret = 0;
	}else{
		std::list<void*>::iterator firstIt = tasksQueue.begin();
		ret = (ExecutionUnit<void*>*) *(firstIt);
		tasksQueue.erase(firstIt);
	}
	return ret;
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
void Worker::setTask(const ExecutionUnit<T>* execUnit){
	scoped_lock lock(*mtx);
	waiting = false;
	this->executionUnit = (ExecutionUnit<void*>*)execUnit;
	task_cond->notify_all();
	printf("task setted to %d. notify\n", workerId);
}


Worker::Worker(Pool* poolPrm): pool(poolPrm), workerId(generateWorkerId()){
	mtx = new boost::mutex();
	task_cond = new boost::condition_variable();
	deleted = false;
	ret = (void*)0;
	executionUnit = 0;
	waiting = true;
	printf("construct worker %d\n", workerId);
}


void Worker::cleans(){
	printf ("run worker\n");
	task_cond->notify_all();
	delete task_cond;
	delete mtx;
}

void Worker::run(){	
	while(true){
		printf("worker %d start find task\n", workerId);
		waitForTask();
		if (deleted){
			return;
		}
		scoped_lock lock(*mtx);
		 std::cout << "worker "<< workerId << "  start calc in thread " << thread.get_id() << "\n";
		//printf("worker %d start calc in thread %d\n", workerId, thread.get_id());
		ret = this->executionUnit->task->call();
		printf("end calc\n");
		this->executionUnit->future->setResult(ret);
		printf("res setted\n");
	}
}

void Worker::waitForTask(){
	ExecutionUnit<void*>* exec;
	{
		scoped_lock queueLock(*pool->queueMtx);
		if (this->executionUnit != 0){
			setWaiting(false);
			return;
		}
		 exec = pool->findTask();
	}
	if (exec != 0){
		printf("worker start exec finded task\n");
		this->executionUnit = exec;
		exec->future->setWorker(this);
		setWaiting(false);
	}else{		
		scoped_lock lock(*mtx);
		while (this->executionUnit == 0){
			// printf("pool size: %d\n", pool->tasksQueue.size());
			printf("worker %d start sleeping\n", workerId);
			task_cond->wait(lock);
			printf("worker %d stop sleeping\n", workerId);
			if (deleted){
				//clean();
				printf("time to worker %d die\n", workerId);
				return;
			}
		}
		setWaiting(false);
	}
}

template<typename T>
void Future<T>::setWorker(Worker* worker){
	this->worker = worker;
	waitingCondition->notify_one();
}

template<typename T>
void Future<T>::setResult(void* result){
	scoped_lock lock(*workerWaitingMtx);
	ret = (T)result;
	this->worker->executionUnit = 0;
	this->done = true;
	this->worker->setWaiting(true);
	printf ("worker set ret %d and notify future\n", ret);
	this->waitingCondition->notify_all();	
}

template<typename T>
T Future<T>::get(){	
	printf (">>start getting\n");
	if (isDone()){
		return ret;
	}
	waitingForWorker();
	printf (">>worker waiting ok\n");
	scoped_lock lock(*workerWaitingMtx);
	while (!isDone()) {
		printf (">>start waiting for ret\n");
		this->waitingCondition->wait(lock);
	}
	printf (">>res resived\n");
	return ret;
}

template<typename T>
void Future<T>::waitingForWorker(){
	scoped_lock lock(*workerWaitingMtx);
	while(worker == 0){
		printf (">>start waiting for worker\n");
		waitingCondition->wait(lock);
	}
}

#endif
