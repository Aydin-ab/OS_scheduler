# OS scheduler
Homework for OS course at NYU. Basic scheduler to replicate OS behavior in C++

**Grade : 100/100**

## HOW TO USE
Compile the code with the ```make``` command
Execute the program with ```sched [-s<schedspec>] input_file rand_file```. The output goes to the standard output
Given a list of input files and a random file, you can use the ```runit.sh``` script to run the program on each of them and put the outputs in a output directory. Use the runit.sh script inside the input directory like that : ```runit.sh <your_output_dir> sched```


## CONTEXT
I explore the implementation and effects of different scheduling policies discussed in class on a set of processes/threads executing on a system. The system is to be implemented using Discrete Event Simulation (DES) (http://en.wikipedia.org/wiki/Discrete_event_simulation). In discrete-event simulation, the operation of a system is represented as a chronological sequence of events. Each event occurs at an instant in time and marks a change of state in the system. This implies that the system progresses in time through defining and executing the events (state transitions) and by progressing time discretely between the events as opposed to incrementing time continuously (e.g. don’t do “sim_time++”). Events are removed from the event queue in chronological order, processed and might create new events at the current or future time.

Note that, I am not implementing this as a multi-program or multi-threaded application. By using DES, a process is simply the PCB object that goes through discrete state transitions. In the PCB object you maintain the state and statistics of the process as any OS would do. 
In reality, the OS doesn’t get involved during the execution of the program (other than system calls), only at the scheduling events and that is what we are addressing in this lab.

Any process essentially involves processing some data and then storing / displaying it (on Hard drive, display etc). (A process which doesn’t store/display processed information is practically meaningless). For instance: when creating a zip file, a chunk of data is first read, then compressed, and finally written to disk, this is repeated until all of the file is compressed. Hence, an execution timeline of any process will contain alternating discrete periods of time which are either dedicated for processing (computations involving CPU aka cpu_burst) or for doing IO (aka ioburst). I assume that our system has only 1 CPU core without hyperthreading - meaning that only 1 process can run at any given time; all processes are single threaded - i,e, they are either in compute/processing mode or IO mode. These discrete periods will therefore be non-overlapping. There could be more than 1 process running (concurrently) on the system at a given time though, and a process could be waiting for the CPU, therefore the execution timeline for any given process can/will contain 3 types of non-overlapping discrete time periods representing (i) Processing / Computation, (ii) Performing IO and (iii) Waiting to get scheduled on the CPU.


The simulation works as follows:
Various processes will arrive / spawn during the simulation. Each process has the following 4 parameters:
1) Arrival Time (AT) - The time at which a process arrives / is spawned / created.
2) Total CPU Time (TC) - Total duration of CPU time this process requires
3) CPU Burst (CB) – A parameter to define the upper limit of compute demand (further described below)
4) IO Burst (IO) - A parameter to define the upper limit of I/O time (further described below)

The processes during its lifecycle will follow the following state diagram :
Initially when a process arrives at the system it is put into CREATED state. The processes’ CPU and the IO bursts are statistically defined. When a process is scheduled (becomes RUNNING (transition 2)) the cpu_burst is defined as a random number between [ 1 .. CB ]. If the remaining execution time is smaller than the cpu_burst compute, reduce it to the remaining execution time. When a process finishes its current cpu_burst (assuming it has not yet reached its total CPU time TC), it enters into a period of IO (aka BLOCKED) (transition 3) at which point the io_burst is defined as a random number between [ 1 .. IO ]. If the previous CPU burst was not yet exhausted due to preemption (transition 5), then no new cpu_burst shall be computed yet in transition 2 and you continue with the remaining cpu burst.

The scheduling algorithms to be simulated are:
FCFS, LCFS, SRTF, RR (RoundRobin), PRIO (PriorityScheduler) and PREemptive PRIO (PREPRIO). In RR, PRIO and PREPRIO, my program should accept the time quantum and for PRIO/PREPRIO optionally the number of priority levels maxprio as an input (see below “Execution and Invocation Format”).
The proper “scheduler object” is selected at program starttime based on the “-s” parameter. 

The rest of the simulation must stay the same (e.g. event handling mechanism and Simulation()).
