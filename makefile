export LD_LIBRARY_PATH :=/usr/local/lib/

pool:  src/main.o
	g++ src/main.o -o bin/pool -L$(LD_LIBRARY_PATH) -lboost_thread -lboost_system 
main.o: main.cpp 
	g++ -c src/main.cpp


tests: test/tests.o test/test_framework.o
	g++ test/tests.o test/test_framework.o -o bin/tests -L$(LD_LIBRARY_PATH) -lboost_thread -lboost_system
#lazy to build lib %)
test_framework.o: test_framework.cpp 
	g++ -c test/test_framework.o 
tests.o:
	g++ -c test/tests.cpp 
clean:
	find . -type f ! -name 'test_framework.o' -name '*.o' -delete
	find . -type f -name 'pool' -delete
	find . -type f -name 'tests' -delete
recompill-and-run: 
	make clean
	make pool
	./bin/pool
recompill-and-run-tests: 
	make clean
	make tests
	./bin/tests
run:
	./bin/pool
love:
	@echo "<3"
war:
	@echo make love, not war!