#ifndef POOL_H
#define POOL_H
#include <list>
#include "stdio.h"
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <utility> 

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
	void setWorker(Worker* worker); //@FIXME friends? other encapsulating?
	void setResult(void* result); //@FIXME friends? other encapsulating?
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
	Pool(const int hotThreads, const int maxThreads, const double timeout);
	virtual ~Pool();
	template <typename T>
	Future<T>* submit(Callable<T>* c);
	int getHotThreads() const {return hotThreads;}
	int getTimeout() const {return timeout;}
	int getActualWorkersCount() const {return workers.size();}
	ExecutionUnit<void*>* findTask();
	boost::condition_variable* queue_notifier;
	boost::mutex* queueMtx;
	void tryToRemoveWorker(Worker* worker);
private:
	std::list<Worker* > workers;
	const int hotThreads;
	const int maxThreads;
	const int timeout;
	std::list<void* > tasksQueue;
};


//#################### Implementation ################################

Pool::Pool(const int hotThreadsParam, const int maxThreadsParam, const double timeoutParam): 
	hotThreads(hotThreadsParam), timeout(timeoutParam), maxThreads(maxThreadsParam) {
	queueMtx = new boost::mutex();
	queue_notifier = new boost::condition_variable();
	for (int i = 0; i < hotThreads; i++){
		Worker* worker = new Worker(this);
		boost::thread thread(boost::bind(&Worker::run, worker));
		worker->thread.swap(thread);
		//@TODO start here
		workers.push_back(worker);
	}
}

void Pool::tryToRemoveWorker(Worker* worker){
	if (getActualWorkersCount() > hotThreads){
		worker->deleted = true;
		workers.remove(worker);
	}
}

template <typename T>
Future<T>* Pool::submit(Callable<T>* task){
	printf("start submit\n");
	scoped_lock lock(*queueMtx);
	if (!this->tasksQueue.empty() && this->workers.size() < maxThreads){
		Worker* worker = new Worker(this);
		boost::thread thread(boost::bind(&Worker::run, worker));
		worker->thread.swap(thread);
		//@TODO start here
		workers.push_back(worker);
	}
	Worker* worker = 0;
	Future<T>* future = new Future<T>(task->getTaskId(), worker);
	ExecutionUnit<T>* unit = new ExecutionUnit<T>(future, task, worker);
	this->tasksQueue.push_back(unit);
	printf("notify workers\n");
	queue_notifier->notify_one();
	return future;
}

ExecutionUnit<void*>* Pool::findTask(){	
	printf("worker in finding\n");
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
	scoped_lock queueLock(*(queueMtx));
	printf("start pool destrunction. workers = %d\n", workers.size());
	for (std::list<Worker*>::iterator it = this->workers.begin();
		it != this->workers.end(); 
		++it){
		// delete &(*(it))->thread; @TODO deleting workers
		(*(it))->deleted = true;
		//(*(it))->task_cond->notify_all();
	}
	queue_notifier->notify_all();
	printf("end pool destrunction\n");
}

Worker::Worker(Pool* poolPrm): pool(poolPrm), workerId(generateWorkerId()){
	mtx = new boost::mutex();
	task_cond = new boost::condition_variable();
	deleted = false;
	executionUnit = 0;
	waiting = true;
	printf("construct worker %d\n", workerId);
}


void Worker::cleans(){
	printf("time to die worker %d\n", workerId);
	return; 
	//@TODO killing workers
	task_cond->notify_all();
	delete task_cond;
	delete mtx;
}

void Worker::run(){	
	while(true){
		waitForTask();
		if (deleted){
			cleans();
			return;
		}
		scoped_lock lock(*mtx);
		this->executionUnit->future->setWorker(this);
		printf("worker %d start calc\n", workerId);
		void* ret = this->executionUnit->task->call();
		printf("end calc\n");
		this->executionUnit->future->setResult(ret);
		printf("res setted\n");
	}
}

void Worker::waitForTask(){
	scoped_lock queueLock(*(this->pool->queueMtx));
	if (deleted){
		return;
	}
	printf("worker %d go to find task\n");
	this->executionUnit =  pool->findTask();
	while (this->executionUnit == 0){
		boost::system_time tAbsoluteTime = 
			boost::get_system_time() + boost::posix_time::milliseconds(pool->getTimeout() * 1000);
		printf("worker %d start sleeping\n", workerId);
		this->pool->queue_notifier->timed_wait(queueLock, tAbsoluteTime);
		if (boost::get_system_time() >= tAbsoluteTime){
			pool->tryToRemoveWorker(this);
		}
		if (deleted){
			return;
		}
		this->executionUnit = pool->findTask();
		printf("worker %d stop sleeping\n", workerId);

	}
	setWaiting(false);
}

template<typename T>
void Future<T>::setWorker(Worker* worker){
	this->worker = worker;
	waitingCondition->notify_one();
}

template<typename T>
void Future<T>::setResult(void* result){
	scoped_lock lock(*workerWaitingMtx);
	printf ("worker now set result = %d\n", (T)result);
	ret = (T)result;
	this->done = true;
	this->worker->setWaiting(true);
	this->worker->executionUnit = 0;
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
