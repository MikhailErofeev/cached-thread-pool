#ifndef POOL_H
#define POOL_H
#include <list>
template <typename T>
class Future{
public:
	Future();
	inline bool isDone() const {return done;}
	inline bool isCanceled() const {return canceled;}
	void cancel();
	T get();
private:
	bool done; //can't set isDone????
	bool canceled;
};

template <typename T>
class Callable{
public:
	virtual T call() = 0;
};


class Worker{
public:
	template <typename T>
	Future<T> setTask(const Callable<T>& task);
	inline bool isWaiting() const {return waiting;}
	void run();
private:
	bool waiting;
};


class Pool{
public:
	Pool(const int hotThreads, const double timeout);
	virtual ~Pool();
	template <typename T>
	Future<T> submit(const Callable<T>& c);
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
		workers.push_back(new Worker());
	}
}

template <typename T>
Future<T> Pool::submit(const Callable<T>& task){
	return (*workers.begin())->setTask(task);
}

Pool::~Pool(){}

template <typename T>
Future<T>::Future():done(false), canceled(false){}

template <typename T>
Future<T> Worker::setTask(const Callable<T>& task){
	return Future<T>();
}


#endif




