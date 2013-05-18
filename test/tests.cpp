#define BOOST_TEST_MODULE tests
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include "../src/Pool.h"

BOOST_AUTO_TEST_CASE( boost_tests_test ) 
{
	BOOST_CHECK_EQUAL(2*2, 4);
}

class StateChanger: public Callable<int>{
	public:
    int state;
    virtual int call(){
        state = 1;
        return state;
    }
};

BOOST_AUTO_TEST_CASE( boost_threads_test ) {
	StateChanger stateChanger;
    boost::thread thread =  boost::thread(boost::bind(&StateChanger::call, &stateChanger));
    thread.join();
    BOOST_CHECK_EQUAL(1, stateChanger.state);
}


BOOST_AUTO_TEST_CASE(pool_trivial) {
    Pool pool(2, 5);
    BOOST_CHECK_EQUAL(2, pool.getHotThreads());
    StateChanger* stateChanger = new StateChanger();

    Future<int> future = pool.submit(stateChanger);
    BOOST_CHECK_EQUAL(false, future.isDone());
    BOOST_CHECK_EQUAL(false, future.isCanceled());
    BOOST_CHECK_EQUAL(1, future.getTaskId());
    
}