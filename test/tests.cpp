#define BOOST_TEST_MODULE tests
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include "../src/Pool.h"

BOOST_AUTO_TEST_CASE( boost_tests_test ) {
	BOOST_CHECK_EQUAL(2*2, 4);
}

class Runner{
	public:
    int state;
    void run(){
        state = 1;
    }
};

BOOST_AUTO_TEST_CASE( boost_threads_test ) {
	Runner runner;
    boost::thread thread =  boost::thread(boost::bind(&Runner::run, &runner));
    thread.join();
    BOOST_CHECK_EQUAL(1, runner.state);
}


BOOST_AUTO_TEST_CASE(pool_trivial) {
    Pool pool(2, 5);
    BOOST_CHECK_EQUAL(2, pool.getHotThreads());
}