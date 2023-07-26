# Multiprocessor Scheduling with Synchronization

- Application that simulates multiprocessor scheduling with synchronization needs
- Two approaches for for multiprocessor scheduling are implemented namely, single-queued and multi-queued approaches
- FCFS (First Come First Served), SJF (Shortes Job First), and RR (Round Robin) algorithms are used as scheduling algorithms
- The application uses task bursts and interarrival times given in the input file
- If the input file is nor specified, the bursts and interarrival times are randomly generated according to the command line arguments
- All the command line arguments are optional and there are default values for them. The arguments can be provided in any order
- For the synchronization purposes, mutex locks and condition variables are used
- The application is developed on Linux operating system using C programming language

## Contents

- Project2.pdf (Project Description)
- mpsk.c (Source File)
- mps_cv.c (Source File)
- Makefile (Makefile to Compile the Project)
- report.pdf (Experimental Results)

## How to Run

- cd to the project directory

##### Compilation and linking

```
$ make
```

##### Recompile

```
$ make clean
$ make
```

##### Running the mps program

```
$ ./mps [-n N] [-a SAP QS] [-s ALG Q] [-i INFILE] [-m OUTMODE] [-o OUTFILE] [-r T T1 T2 L L1 L2 PC]
```

##### Example mps run

```
$ make
$ ./mps -n 4 -a M LM -s RR 50 -r 150 10 1000 80 10 250 10
```

##### Running the mps_cv program

```
$ ./mps_cv [-n N] [-a SAP QS] [-s ALG Q] [-i INFILE] [-m OUTMODE] [-o OUTFILE] [-r T T1 T2 L L1 L2 PC]
```

##### Example mps_cv run

```
$ make
$ ./mps_cv -n 4 -a M LM -s SJF 0 -i infile.txt -m 2 -o out.txt
```
