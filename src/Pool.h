#ifndef POOL_H
#define POOL_H

template <typename T>
class Future{
public:
	bool isDone();
	void cancel();
	T get();
};

template <typename T>
class Callable{
public:
	T call();
};


class Pool{
public:
	Pool(const int hotThreads, const double timeout);
	virtual ~Pool();
	template <typename T>
	Future<T>& submit(const Callable<T>& c);
	inline int getHotThreads() const {return hotThreads;}
	inline double getTimeout() const {return timeout;}
private:
	const int hotThreads;
	const double timeout;
};



Pool::Pool(const int hotThreadsParam, const double timeoutParam): hotThreads(hotThreadsParam), timeout(timeoutParam){
}

Pool::~Pool(){}





#endif




