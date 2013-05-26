#define BOOST_TEST_MODULE tests
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include "../src/Pool.h"
#include "stdio.h"

BOOST_AUTO_TEST_CASE( boost_tests_test ) 
{
	BOOST_CHECK_EQUAL(2*2, 4);
}

class StateChanger: public Callable<int>{
	public:
    int state;
    StateChanger(int ret):state(ret){}
    StateChanger():state(5){}
    virtual int call(){
        printf("stateChanger %d called. ret = %d\n", getTaskId(), state);
        return state;
    }
};

BOOST_AUTO_TEST_CASE( boost_threads_test ) {    
	StateChanger stateChanger;
    boost::thread thread =  boost::thread(boost::bind(&StateChanger::call, &stateChanger));
    thread.join();
    BOOST_CHECK_EQUAL(0 , stateChanger.getTaskId());
}


BOOST_AUTO_TEST_CASE(pool_trivial) {
    printf("-----------pool_trivial---------------\n");
    Pool pool(1, 5);
    BOOST_CHECK_EQUAL(1, pool.getHotThreads());
    StateChanger* stateChanger = new StateChanger();

    Future<int>* future = pool.submit(stateChanger);
    BOOST_CHECK_EQUAL(false, future->isDone());
    BOOST_CHECK_EQUAL(false, future->isCanceled());
    BOOST_CHECK_EQUAL(1, future->getTaskId());
    printf("stop test\n");    
}


BOOST_AUTO_TEST_CASE( get_result ) {
    printf("-----------get_result---------------\n");
    Pool pool(1, 5);
    BOOST_CHECK_EQUAL(1, pool.getHotThreads());
    for (int i = 0; i < 10; i++){
        StateChanger* stateChanger = new StateChanger();
        Future<int>* future = pool.submit(stateChanger);
        BOOST_CHECK_EQUAL(5, future->get());
        StateChanger* stateChanger2 = new StateChanger(i);
        Future<int>* future2 = pool.submit(stateChanger2);
        BOOST_CHECK_EQUAL(i, future2->get());
        delete stateChanger, stateChanger2;
    }
    printf("stop test\n");    
}


BOOST_AUTO_TEST_CASE( queue ) {
    printf("-----------queue---------------\n");
    Pool pool(1, 5);    
    Future<int>* future1 = pool.submit(new StateChanger(1));
    Future<int>* future2 = pool.submit(new StateChanger(2));
    Future<int>* future3 = pool.submit(new StateChanger(3));
    Future<int>* future4 = pool.submit(new StateChanger(4));
    Future<int>* future5 = pool.submit(new StateChanger(5));
    Future<int>* future6 = pool.submit(new StateChanger(6));
    Future<int>* future7 = pool.submit(new StateChanger(7));
    BOOST_CHECK_EQUAL(1, future1->get());
    BOOST_CHECK_EQUAL(2, future2->get());
    BOOST_CHECK_EQUAL(3, future3->get());
    BOOST_CHECK_EQUAL(4, future4->get());
    BOOST_CHECK_EQUAL(5, future5->get());
    BOOST_CHECK_EQUAL(6, future6->get());
    BOOST_CHECK_EQUAL(7, future7->get());
    printf("stop test\n");    
}


BOOST_AUTO_TEST_CASE( multy_workers ) {
    printf("-----------multy_workers---------------\n");
    Pool pool(20, 5);  
    for (int i = 0; i < 10000; i++){
        Future<int>* future = pool.submit(new StateChanger(i));
        BOOST_CHECK_EQUAL(i, future->get());
        delete future;
    }
    printf("stop test\n"); 
}