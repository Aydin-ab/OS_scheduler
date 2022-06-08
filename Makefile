mmy: sched.cpp
	bash -c "module load gcc-9.2"
	g++ -std=c++11 -g sched.cpp -o sched

clean:
	rm -f sched *~