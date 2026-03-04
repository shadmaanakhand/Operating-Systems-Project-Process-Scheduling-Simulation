# OS Process Scheduling Simulation
The goal of this project is to simulate process scheduling algorithms, in this case First-Come First-Served (FSFC) and Round Robin. Additionally, this program for this project also contains memory management and paging capabilities. For outputs, it displays Gantt charts, process statistics, and information related to memory.

## Compilation

To compile the C program, run this command in your terminal:

```
gcc -std=c99 process_scheduling_simulation.c -o process_scheduling_simulation
```
After entering the command, you will get an executable `process_scheduling_simulation` from which you can use by `./process_scheduling_simulation`.

## Input File

Make a file named `processes.txt` in the same directory as `process_scheduling_simulation`. The text file should have this format:

```
PID  Arrival_Time  Burst_Time  Priority
X    X             X           X
```
- The row with "PID  Arrival_Time  Burst_Time  Priority" are the names of the columns
- The `X` underneath each column is where you will place information for each process

## How to use

To run the program, you will have to follow this format in your terminal:
```
./process_scheduling_simulation <process_file_name> <mode(fcfs, rr, mem-alloc, paging, -h)>
```

## Mode Options

To see the list of options (just in case you are not sure what the correct commands are), use this:
```
./process_scheduling_simulation processes.txt -h
```
To run First-Come First-Served `fcfs`, use this command:
```
./process_scheduling_simulation processes.txt fcfs
```
To run Round Robin `rr`, use this command:
```
./process_scheduling_simulation processes.txt rr
```
To do Memory Allocation `mem-alloc` (you will be prompted to choose between First/Best/Worst fit, and give memory size), use this command:
```
./process_scheduling_simulation processes.txt mem-alloc
```
To do Paging `paging` (you will be prompted to choose betwen FIFO or LRU, give number of frames, reference string length, and page numbers), use this command:
```
./process_scheduling_simulation processes.txt paging
```

## Outputs
Outputs depend on mode, and will look like this:
- For FCFS and Round Robin, the outputs will include the processes being displayed, execution order, Gantt chart, process statistics, and average times (including CPU utilization).
- For Memory Allocation, the outputs include the processes displayed, process allocation by block, and the memory blocks.
- For Paging, the outputs include the processes displayed and the page table.
- For the help menu, it will display the various commands that can be used along with an example of how to correctly use them.
