#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <stack>
#include <map>
#include <list>

using namespace std;


//-------------------- STEP 1 : Create Processes objects --------------------
// First we write the Process class and the input reader function that builds the Process queue
struct Process {

    // Input attributes
    int pid; // Needed to differentiate processes with same arrival time
    int arrivalTime;
    int totalCPUTime;
    int cpuBurst;
    int ioBurst;
    
    // Scheduler dependant
    int static_prio; 
    int dynamic_prio;

    // Dynamic attributes
    double remainingCPUtime; // total job remaining
    int remainingBurstTime; // remaining CPU burst time in case of preemption. Used to check if the process comes from preemption
    int clock; // dynamic clock of the process
    int stopRunningTime; // Time when the process will stop being run. Needed for E scheduler preemption test

    // Output attributes
    int finishingTime;
    int turnaroundTime;
    int ioWaitingTime; // Time in Block state. Dynamically updated
    int cpuWaitingTime; // Time in Ready state. Dynamically updated

    Process(int pid_, int at, int totCPU, int cpuB, int ioB) {
        pid = pid_;
        arrivalTime = at;
        totalCPUTime = totCPU;
        cpuBurst = cpuB;
        ioBurst = ioB;

        // Default initialization is 0 so we must make sure the static and dynamic_prio are properly initialized to an unobtainable value
        static_prio = -2; 
        dynamic_prio = -2;
        finishingTime = 0;
        turnaroundTime = 0;
        ioWaitingTime = 0; 
        cpuWaitingTime = 0;

        remainingCPUtime = totalCPUTime;
        remainingBurstTime = -1;
        clock = 0; 
        stopRunningTime = -1;
    }

};


//-------------------- STEP 2 : Schedulers definitions --------------------
// Scheduler Base class
class Scheduler {

   public:
        // pure virtual function providing interface framework.
        virtual void add_process(Process* process) = 0;
        virtual Process* get_next_process() = 0;
        virtual void print_scheduler() = 0;
        virtual bool test_preempt(Process* running_process, Process* process, int curr_time) = 0;
        bool preprio_flag = false; // True only for preprio scheduler. Used to check if preemption is possible when a new process becomes READY
        int quantum = 10000;
        int maxprio= 4;
        
};

class FCFS: public Scheduler {
    public:
        queue<Process*> ready_processes; // FIFO queue

        void add_process(Process* process) { 
            ready_processes.push(process);
        }

        Process* get_next_process() {
            if (ready_processes.empty()) {
                return nullptr;
            } else {
                Process* next_process = ready_processes.front();
                ready_processes.pop();
                return next_process;
            }
        }

        // For E scheduler only
        bool test_preempt(Process* running_process, Process* process, int curr_time) {
            return false;
        }

        void print_scheduler() {
            cout << "FCFS" << endl;
        }

        FCFS():Scheduler() {}

};

class LCFS: public Scheduler {
    public:
        stack<Process*> ready_processes; // LIFO queue

        void add_process(Process* process) { 
            ready_processes.push(process);
        }

        Process* get_next_process() {
            if (ready_processes.empty()) {
                return nullptr;
            } else {
                Process* next_process = ready_processes.top();
                ready_processes.pop();
                return next_process;
            }
        }

        void print_scheduler() {
            cout << "LCFS" << endl;
        }

        // For E scheduler only
        bool test_preempt(Process* running_process, Process* process, int curr_time){
            return false;
        }

        LCFS():Scheduler() {}

};

class SRTF: public Scheduler {
    public:
        // Linked list of Processes where the head is the process with shortest cpu time remaining
        list<Process*> ready_processes; 

        void add_process(Process* process) { 
            std::list<Process*>::iterator it = ready_processes.begin();
            while(it != ready_processes.end()) {
                // We use large inequalities to make sure we respect the order of arrival of the processes in the ready queue
                // (Deterministic Behavior rule)
                if ((*it)->remainingCPUtime <= process->remainingCPUtime) {
                    it++;
                } else {
                    ready_processes.insert(it, process);
                    return;
                }
            }

            // If we are here, it means we didn't insert the process so it must have the largest remaining time of the list of ready processes
            // ==> We add it at the back
            ready_processes.push_back(process);
        }

        Process* get_next_process() {
            if (ready_processes.empty()) {
                return nullptr;
            } else {
                // By construction of the linked list ready_processes, the shortest remaining time process is in the front
                Process* next_process = ready_processes.front();
                ready_processes.pop_front();
                return next_process;
            }
        }

        void print_scheduler() {
            cout << "SRTF" << endl;
        }

        // For E scheduler only
        bool test_preempt(Process* running_process, Process* process, int curr_time){
            return false;
        }

        SRTF():Scheduler() {}

};

class RR: public Scheduler {
    public:
        queue<Process*> ready_processes; // FIFO queue

        void add_process(Process* process) { 
            ready_processes.push(process);
        }

        Process* get_next_process() {
            if (ready_processes.empty()) {
                return nullptr;
            } else {
                Process* next_process = ready_processes.front();
                ready_processes.pop();
                return next_process;
            }
        }

        void print_scheduler() {
            cout << "RR " << quantum << endl;
        }

        // For E scheduler only
        bool test_preempt(Process* running_process, Process* process, int curr_time){
            return false;
        }

        RR(int quantum_):Scheduler() {
            quantum = quantum_;
        }

};

class PRIO: public Scheduler {
    public:
        vector<queue<Process*>>* activeQ = new vector<queue<Process*>>(); // POINTER to the active queue
        vector<queue<Process*>>* expiredQ = new vector<queue<Process*>>(); // POINTER to the expired queue

        void add_process(Process* process) {
            // First we check if the process comes from preemption
            if (process->remainingBurstTime > 0) {
                // If preemption, we decrease the dynamic priority
                process->dynamic_prio--;
                // If the dynamic priority is -1 ...
                if (process->dynamic_prio == -1) {
                    // ... We reset the dynamic priority to the static priority - 1 ...
                    process->dynamic_prio = process->static_prio - 1;
                    // ... and we add the process to the expiredQ
                    (*expiredQ)[process->dynamic_prio].push(process);
                    // End of add_process in this case so we return
                    return;
                }
            }
            else {
                // If no preemption, it means the process comes from CREATED or BLOCKED 
                // so we reset its dynamic prio to his static prio - 1
                process->dynamic_prio = process->static_prio - 1;
            }
            // Finally, we add the process to the active Queue (we dealt with the case of expired queue before)
            (*activeQ)[process->dynamic_prio].push(process);
        }

        Process* get_next_process() {

            Process* next_process = nullptr;
            // We iterate through the active queue vector ot find the next process
            for (vector<queue<Process*>>::reverse_iterator it = (*activeQ).rbegin(); it != (*activeQ).rend(); it++) {
                if (!(*it).empty()) {
                    next_process = (*it).front();
                    (*it).pop();
                    return next_process;
                }
            }

            // If we are here, it means we have no processes left in active queue so we swap the active and expired queue
            swap(activeQ, expiredQ);

            // And now we iterate again in the new active queue
            for (vector<queue<Process*>>::reverse_iterator it = (*activeQ).rbegin(); it != (*activeQ).rend(); it++) {
                if (!(*it).empty()) {
                    next_process = (*it).front();
                    (*it).pop();
                    return next_process;
                }
            }

            // If we are here, it means we have no more processes in the active and expired queue so we return null pointer
            return nullptr;

        }

        void print_scheduler() {
            cout << "PRIO " << quantum << endl;
        }

        // For E scheduler only
        bool test_preempt(Process* running_process, Process* process, int curr_time){
            return false;
        }

        PRIO(int quantum_, int maxprio_):Scheduler() {
            quantum = quantum_;
            maxprio = maxprio_;

            // We initialize the vectors of queue with empty queues
            for (int i = 0; i < maxprio; i++) {
                activeQ->push_back(queue<Process*>());
                expiredQ->push_back(queue<Process*>());
            }
        }

};

class PREPRIO: public Scheduler {
    public:
        vector<queue<Process*>>* activeQ = new vector<queue<Process*>>(); // POINTER to the active queue
        vector<queue<Process*>>* expiredQ = new vector<queue<Process*>>(); // POINTER to the expired queue

        void add_process(Process* process) {
            // First we check if the process comes from preemption
            if (process->remainingBurstTime > 0) {
                // If preemption, we decrease the dynamic priority
                process->dynamic_prio--;
                // If the dynamic priority is -1 ...
                if (process->dynamic_prio == -1) {
                    // ... We reset the dynamic priority to the static priority - 1 ...
                    process->dynamic_prio = process->static_prio - 1;
                    // ... and we add the process to the expiredQ
                    (*expiredQ)[process->dynamic_prio].push(process);
                    // End of add_process in this case so we return
                    return;
                }
            }
            else {
                // If no preemption, it means the process comes from CREATED or BLOCKED 
                // so we reset its dynamic prio to his static prio - 1
                process->dynamic_prio = process->static_prio - 1;
            }
            // Finally, we add the process to the active Queue (we dealt with the case of expired queue before)
            (*activeQ)[process->dynamic_prio].push(process);
        }

        Process* get_next_process() {

            Process* next_process = nullptr;
            // We iterate through the active queue vector ot find the next process
            for (vector<queue<Process*>>::reverse_iterator it = (*activeQ).rbegin(); it != (*activeQ).rend(); it++) {
                if (!(*it).empty()) {
                    next_process = (*it).front();
                    (*it).pop();
                    return next_process;
                }
            }

            // If we are here, it means we have no processes left in active queue so we swap the active and expired queue
            swap(activeQ, expiredQ);

            // And now we iterate again in the new active queue
            for (vector<queue<Process*>>::reverse_iterator it = (*activeQ).rbegin(); it != (*activeQ).rend(); it++) {
                if (!(*it).empty()) {
                    next_process = (*it).front();
                    (*it).pop();
                    return next_process;
                }
            }

            // If we are here, it means we have no more processes in the active and expired queue so we return null pointer
            return nullptr;

        }

        // For E scheduler only
        bool test_preempt(Process* CURRENT_RUNNING_PROCESS, Process* process, int CURRENT_TIME) {
            bool prioTestPreemption = CURRENT_RUNNING_PROCESS->dynamic_prio < process->dynamic_prio;
            bool timeTestPreemption = CURRENT_TIME < CURRENT_RUNNING_PROCESS->stopRunningTime;
            return (prioTestPreemption && timeTestPreemption);
        }

        void print_scheduler() {
            cout << "PREPRIO " << quantum << endl;
        }

        PREPRIO(int quantum_, int maxprio_):Scheduler() {
            quantum = quantum_;
            maxprio = maxprio_;
            preprio_flag = true;

            // We initialize the vectors of queue with empty queues
            for (int i = 0; i < maxprio; i++) {
                activeQ->push_back(queue<Process*>());
                expiredQ->push_back(queue<Process*>());
            }
        }

};

//-------------------- STEP 3 : Create random number array and random number function --------------------

int* random_nums; // This array will store all the random numbers
int total_random_num;
void initialize_random_array(istream& rand_file){

    rand_file >> total_random_num; // Read first line where there is the total number of random numbers

    random_nums = new int[total_random_num] ;
    int curr_num;
    int incr = 0;
    while (rand_file >> curr_num) {
        random_nums[incr] = curr_num;
        incr++;
    }

}

int ofs = 0;

int get_random_number(int burst) { 

    int randomVal = 1 + (random_nums[ofs] % burst);
    ofs++;
    if (ofs == total_random_num) {
        ofs = 0;
    }
    return randomVal;

}


//-------------------- STEP 4 : Create the Event class --------------------

enum State {CREATED, READY, RUNNING, BLOCKED, DONE}; // enum of states
struct Event {

    int timestamp;
    Process* process;
    State old_state;
    State new_state;

    Event(int timestamp_, Process* process_, State old_state_, State new_state_) {
        timestamp = timestamp_;
        process = process_;
        old_state = old_state_;
        new_state = new_state_;
    }

};

//-------------------- STEP 5 : Create the DES layer --------------------
// We implement the DES as a linked list but the most optimal would be a prioqueue with the timestamp as the priority
// But I'm not sure the assignement allows us to use a prio queue for the DES

list<Event*> events;
struct DES_Layer {

    static void put_event(Event* new_event_ptr) {
        std::list<Event*>::iterator it = events.begin();
        while(it != events.end()) {
            Event* curr_event_ptr = *it;
            if (curr_event_ptr->timestamp <= new_event_ptr->timestamp) {
                it++;
            } else {
                events.insert(it, new_event_ptr);
                return;
            }
        }

        // If we are here, it means we didn't insert the event so it must have the largest timestamp of the list of events
        // ==> We add it at the back
        events.push_back(new_event_ptr);
    }

    static Event* get_event() {
        Event* event = events.front();
        if (event == 0) {
            return 0;
        }
        events.pop_front();
        return event;
    }

    static void remove_event(Process* preempted_process) {
        // Need to remove the RUNNING->BLOCKED or RUNNING->READY event from preempted_process
        // Used by E scheduler only
        // Loop is valid because we are 100% sure that the event exist 
        // (it must have been created during the RUNNING event of this process)
        // This event is the unique one that concerns the preempted_process so we use the PID as filter
        std::list<Event*>::iterator it = events.begin();
        while( ((*it)->process)->pid != preempted_process->pid ) {
            it++;
        }
        // Now it points to the event we want to delete
        events.erase(it);

        
    }

    static int get_next_time_event(){
        if (events.empty()) {
            return -1; // No events remaining (!!! =/= end of simulation, we have to check if the ready processes queue is empty !!!)
        }
        else {
            int next_time = events.front()->timestamp;
            return next_time;
        }
    }


};


//-------------------- STEP 6 : Create processes queue in order of their appearance in the input file + create first events --------------------
// We assume the input files are not tricky so we do not check if the arrival time are properly increasing

queue<Process*> processes; // Processes queue (in order of the file)
void createProcesses(istream& input_file, Scheduler* scheduler) {
    double at, totCPU, cpuB, ioB;
    int static_prio;

    int count= 0; // Same as pid. We use the order of arrival as the pid of the process
    while (input_file >> at >> totCPU >> cpuB >> ioB) {
        Process* process = new Process(count, at, totCPU, cpuB, ioB);
        process->static_prio = get_random_number(scheduler->maxprio);
        process->dynamic_prio = process->static_prio - 1;
        processes.push(process);
        events.push_back(new Event(at, process, CREATED, READY));
        count++;
    }
};


//-------------------- STEP 8 : Make Simulation --------------------


struct Simulator {
    // Track performance stats for printing ouput
    struct Output {
        int finishingTimeOfLastEvent = 0;

        double cpuUtilization = 0;

        double ioUtilization = 0;
        // ioUtilization is harder to compute because multiple processes can be in blocked state.
        int number_io_processes = 0; 
        int start_of_IO_utilization; 
        // When number_io_processes goes from 0 to 1, we store the time in start_of_IO_utilization
        // When number_io_processes goes back to 0, we can compute the total duration of io utilization as CURRENT_TIME - start_of_IO_utilization

        double avgTurnaroundTime = 0;
        double avgCPUWaitingTime = 0;
        double throughputPer100TimeUnits = 0;
    };
    Output output;

    // Track current running process
    Process* CURRENT_RUNNING_PROCESS = nullptr;

    // track current time of simulation
    int CURRENT_TIME; 

    void simulation(Scheduler* scheduler){

        bool CALL_SCHEDULER; // Decide when scheduler needs to choose another process to run
        Event* event = DES_Layer::get_event();
        // while loop stop when event == 0 which happens at the end of the DES layer
        while (event) {
            Process* process = event->process;
            CURRENT_TIME = event->timestamp;
            State trans_to = event->new_state; // next transition state of the event

            // Update the performance stats
            if (event->old_state == RUNNING) { 
                // By construction, process->clock is the time when the process entered the running state
                output.cpuUtilization += (double) CURRENT_TIME - (double) process->clock; 
            }
            if (event->old_state == BLOCKED) { 
                output.number_io_processes--;
                if (output.number_io_processes == 0) {
                    // No more processes in block state so we can compute the duration of io utilization between
                    // last process and first process
                    output.ioUtilization += (double) CURRENT_TIME - (double) output.start_of_IO_utilization;
                }
            }

            // Check next state transition and do actions accordingly
            switch (trans_to) {
            case READY : {
                // Add process to the runqueue
                scheduler->add_process(process);

                // Check if preempted from RUNNING state or not
                if (process->remainingBurstTime > 0) {
                    CURRENT_RUNNING_PROCESS = nullptr; // We stop the process from running
                }
                // Else the process comes from BLOCKED or CREATED. 
                // We need to deal with the special preemption case from E scheduler
                // CURRENT_RUNNING_PROCESS != nullptr means that it doesn't come from the first CREATED process of the program
                else if (scheduler->preprio_flag && CURRENT_RUNNING_PROCESS != nullptr) {
                    // This checks the dynamic priorities and if the current running process wasn't going to stop now
                    bool isPreemption = scheduler->test_preempt(CURRENT_RUNNING_PROCESS, process, CURRENT_TIME);

                    if (isPreemption) {
                        // If preemption happens, we need to remove the obsolete RUNNING->READY or RUNNING->BLOCKED event from the running process
                        DES_Layer::remove_event(CURRENT_RUNNING_PROCESS);
                        // We need to add the preemption event RUNNING -> READY
                        DES_Layer::put_event( new Event(CURRENT_TIME,
                                                        CURRENT_RUNNING_PROCESS,
                                                        RUNNING,
                                                        READY));
                        // We need to update the CURRENT_RUNNING_PROCESS attributes
                        // Since we preempt it, we store the remaining burst time it has
                        CURRENT_RUNNING_PROCESS->remainingBurstTime += CURRENT_RUNNING_PROCESS->stopRunningTime - CURRENT_TIME;
                        // We need to add the lost burst time to its remamining CPU time
                        CURRENT_RUNNING_PROCESS->remainingCPUtime += CURRENT_RUNNING_PROCESS->stopRunningTime - CURRENT_TIME;
                        // Finally, we set its stop running time to the current time since we preempt it now
                        CURRENT_RUNNING_PROCESS->stopRunningTime = CURRENT_TIME;
                    }

                }

                CALL_SCHEDULER = true;
                break;
            }

            case RUNNING : {

                // Update CPU waiting time to compute average performance later
                // process->clock is the time the process was added in READY state
                process->cpuWaitingTime += CURRENT_TIME - process->clock;

                int cpu_burst_duration;
                bool TO_BE_PREEMPTED;

                // 4 CASES of cpu burst duration:
                // CASE 1 : Process was preempted and the remaining cpu burst time is HIGHER than the quantum
                //      => We give a quantum burst time and we reduce the remaining cpu burst time
                if (process->remainingBurstTime > 0 && process->remainingBurstTime > scheduler->quantum) {
                    cpu_burst_duration = scheduler->quantum;
                    process->remainingBurstTime -= scheduler->quantum;
                    TO_BE_PREEMPTED = true;
                }
                // CASE 2 : Process was preempted and the remaining cpu burst time is LOWER than the quantum
                //      => We give the remaining cpu burst time
                else if (process->remainingBurstTime > 0 && process->remainingBurstTime <= scheduler->quantum) {
                    cpu_burst_duration = process->remainingBurstTime;
                    process->remainingBurstTime = 0;
                    TO_BE_PREEMPTED = false;
                }
                // CASE 3 AND 4 : Process was not preempted so we compute a new random cpu burst duration
                else {
                    cpu_burst_duration = get_random_number(process->cpuBurst);
                    // CASE 3 : random cpu burst duration is HIGHER than quantum
                    //      => We give a quantum burst duration and we mark the process to be preempted
                    if (cpu_burst_duration > scheduler->quantum) {
                        process->remainingBurstTime = cpu_burst_duration - scheduler->quantum;
                        cpu_burst_duration = scheduler->quantum;
                        TO_BE_PREEMPTED = true;
                    }
                    // CASE 4 : random cpu burst duration is LOWER than quantum
                    //      => We don't do anything
                    else {
                        TO_BE_PREEMPTED = false;
                    }
                }

                // Now we check if it's going to be blocked, preempted or if it's going to be done
                //// Check if the job will be done
                if (process->remainingCPUtime <= cpu_burst_duration) {
                    cpu_burst_duration = process->remainingCPUtime;
                    DES_Layer::put_event( new Event(CURRENT_TIME+cpu_burst_duration, 
                                                    process,
                                                    RUNNING,
                                                    DONE));
                    //// We reset the remaining burst time and cpu time since the job will be done
                    process->remainingBurstTime = 0;
                    process->remainingCPUtime = 0;
                }
                //// Check if the job will be preempted
                else if (TO_BE_PREEMPTED) {
                    // Create preemption event RUNNING -> READY
                    DES_Layer::put_event( new Event(CURRENT_TIME + cpu_burst_duration,
                                                    process,
                                                    RUNNING,
                                                    READY));
                    process->remainingCPUtime -= cpu_burst_duration;
                }
                //// If not preempted or done, it means it's gonna be blocked
                else {
                    // Create block event RUNNING -> BLOCKED
                    DES_Layer::put_event( new Event(CURRENT_TIME + cpu_burst_duration, 
                                                    process,
                                                    RUNNING,
                                                    BLOCKED));
                    process->remainingCPUtime -= cpu_burst_duration;
                }

                // Finally, we store the time when the process will stop running
                // We need to store this value to check if the E scheduler will preempt the process with a new READY process
                process->stopRunningTime = CURRENT_TIME + cpu_burst_duration;
                break;
            }
            
            case BLOCKED : {
                CURRENT_RUNNING_PROCESS = nullptr; // We stop the process from running
                // Increment number of processes in BLOCKED state. Used to compute the average IO utilization
                output.number_io_processes += 1; 
                // If it's the first process that uses IO, we record the start time. Used to compute the average IO utilization
                if (output.number_io_processes == 1) {
                    output.start_of_IO_utilization = CURRENT_TIME;
                }
                // Compute random io burst time
                int io_burst_duration = get_random_number(process->ioBurst);

                // update IO waiting time
                process->ioWaitingTime += io_burst_duration;

                // Create BLOCKED -> RDY event
                DES_Layer::put_event( new Event(CURRENT_TIME+io_burst_duration, 
                                    process,
                                    BLOCKED,
                                    READY));
                // Call scheduler for next running process
                CALL_SCHEDULER = true;

                break;
            }

            case DONE : {
                CURRENT_RUNNING_PROCESS = nullptr; // We stop the process form running
                process->finishingTime = CURRENT_TIME;
                process->turnaroundTime = CURRENT_TIME - process->arrivalTime;
                CALL_SCHEDULER = true; // Call scheduler for next running process
                break;
            }
            case CREATED : {
                cout << "hello, i'm here bcs I get a warning if i don't include CREATED in the switch statement" << endl;
            }

            }

            process->clock = CURRENT_TIME; // update clock of process

            if (CALL_SCHEDULER) {
                // process the same time occuring events in order of appearance
                if (DES_Layer::get_next_time_event() == CURRENT_TIME) { 
                    event = DES_Layer::get_event(); 
                    continue;
                } 
                // reset flag
                CALL_SCHEDULER = false;
                // Check if we need to fidn a new running process
                if (CURRENT_RUNNING_PROCESS == nullptr) {
                    CURRENT_RUNNING_PROCESS = scheduler->get_next_process();
                    // If ready queue is empty, we get next event 
                    // (!!! =/= end of simulation : maybe next event is CREATED->READY and we'll get a new running process !!!)
                    if (CURRENT_RUNNING_PROCESS == nullptr) { 
                        // If event is null then it's really the end of simulation
                        event = DES_Layer::get_event(); 
                        continue; // go to next while iteration
                    }
                    // put the ready->running event for current time
                    DES_Layer::put_event( new Event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, READY, RUNNING) );
                }
            }
            // get next event
            event = DES_Layer::get_event(); 

        } // end of while (event) loop


    }; // end of simulation function

    //-------------------- STEP 9 : Print Summary --------------------

    void print_summary() {

        // total number of processes (used to compute the averages)
        double number_of_processes = processes.size();

        while (!processes.empty()) {
            Process* process = processes.front();
            output.avgTurnaroundTime += (double) process->turnaroundTime; // to compute average later
            output.avgCPUWaitingTime += (double) process->cpuWaitingTime; // to comput average later
            printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", 
                    process->pid,
                    process->arrivalTime,
                    process->totalCPUTime,
                    process->cpuBurst,
                    process->ioBurst,
                    process->static_prio,
                    process->finishingTime,
                    process->turnaroundTime,
                    process->ioWaitingTime,
                    process->cpuWaitingTime
                );
                processes.pop();
        }

        // Compute average turn around time and average cpu waiting time
        output.avgTurnaroundTime /= number_of_processes;
        output.avgCPUWaitingTime /= number_of_processes;

        // By construciton, the finishing time of last event is stored in the CURRENT_TIME of last while loop iteration
        output.finishingTimeOfLastEvent = CURRENT_TIME;

        // Compute the CPU and IO utlization ratio
        output.cpuUtilization = 100 * output.cpuUtilization / (double) output.finishingTimeOfLastEvent;
        output.ioUtilization = 100 * output.ioUtilization / (double) output.finishingTimeOfLastEvent;
        
        // Compute the throughput per 100 time units
        output.throughputPer100TimeUnits = 100 * number_of_processes / (double) output.finishingTimeOfLastEvent;


        // Print summary
        printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
                output.finishingTimeOfLastEvent,
                output.cpuUtilization,
                output.ioUtilization,
                output.avgTurnaroundTime,
                output.avgCPUWaitingTime,
                output.throughputPer100TimeUnits
            );

    } // end of print_summary function

}; // End of simulator struct



int main(int argc, char *argv[]) {
    int sflag = 0;
    char *svalue = NULL;
    int index;
    int o;

    Simulator simulator; // Our simulator

    opterr = 0;
    while ((o = getopt (argc, argv, "vteps:")) != -1) {
        switch (o)
        {
        case 'v':
            break;
        case 't':
            break;
        case 'e':
            break;
        case 'p':
            break;
        case 's':
            sflag = 1;
            svalue = optarg;
            break;
        case '?':
            if (optopt == 's') {
                fprintf (stderr, "Option -%c requires a scheduler argument.\n", optopt);
            }
            else if (isprint (optopt)) {
                fprintf (stderr, "Unknown option '-%c'.\n", optopt);
            }
            else {
                fprintf (stderr,
                        "Unknown option character '\\x%x'.\n",
                        optopt);
            }
            return 1;
        default:
            abort ();
        }
    }

    if (sflag != 1) { printf("You must indicate the Scheduler\n"); return -1; }

    Scheduler* scheduler;
    switch (string(svalue)[0]) {
        case 'F' : {
            scheduler = new FCFS();
            break;
        }
        case 'L' : {
            scheduler = new LCFS();
            break;
        }
        case 'S' : {
            scheduler = new SRTF();
            break;
        }
        case 'R' : {
            int quantum = -1;
            sscanf(string(svalue).c_str(), "R%d", &quantum);
            if (quantum == -1) {
                cout << "You must give a quantum for Round Robin scheduler" << endl;
                return -1;
            }            
            scheduler = new RR(quantum);
            break;
        }
        case 'P' : {
            int quantum = -1;
            int maxprio = 4;
            if (string(svalue).find(':') != string::npos){ 
                sscanf(string(svalue).c_str(), "P%d:%d", &quantum, &maxprio); 
            }
            else { 
                sscanf(string(svalue).c_str(), "P%d", &quantum); 
            }

            if (quantum == -1) {
                cout << "You must give a quantum for PRIO scheduler" << endl;
                return -1;
            }
            scheduler = new PRIO(quantum, maxprio);
            break;
        }
        case 'E' : {
            int quantum = -1;
            int maxprio = 4;
            if (string(svalue).find(':') != string::npos){ 
                sscanf(string(svalue).c_str(), "E%d:%d", &quantum, &maxprio); 
            }
            else { 
                sscanf(string(svalue).c_str(), "E%d", &quantum); 
            }

            if (quantum == -1) {
                cout << "You must give a quantum for PREPRIO scheduler" << endl;
                return -1;
            }
            scheduler = new PREPRIO(quantum, maxprio);
            break;
        }
        default : {
            cout << "Scheduler doesn't exist. Choose between F,L,S,RR,P and E" << endl;
            return -1;
            break;
        }
    }


    if (argc - optind < 2 ) { printf("Please give an input file AND a random file\n"); return -1; }
    else if (argc - optind > 2) { printf("Please put only 1 input file and only 1 random file\n"); return -1; }
    // Now we know we have an input file and a random file as non-option arguments
    ifstream input_file ( argv[optind] ); // input file
    ifstream rand_file ( argv[optind + 1] ); // rand file

    // Check if file opening succeeded
    if ( !input_file.is_open() ) {cout<< "Could not open the input file \n"; return -1;}
    else if ( !rand_file.is_open() ) {cout<< "Could not open the rand file \n"; return -1;}

    // Create random numbers array
    initialize_random_array(rand_file);

    // Create the processes queue and the first CREATE events in the DES layer
    createProcesses(input_file, scheduler);

    // Start the simulation
    simulator.simulation(scheduler);

    // Print the summary
    scheduler->print_scheduler(); // print scheduler name (and quantum)
    simulator.print_summary(); // print the summary

}