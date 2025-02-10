# Detecting data races 

This repository contains code for implementations of the DJIT+ and Fasttrack algorithms for detecting data races from a logfile.

As described in the research papers:

- ["MultiRace: Efficient On-the-Fly Data Race Detection in Multithreaded C++ Programs" by Eli Pozniansky and Assaf Schuster](https://onlinelibrary.wiley.com/doi/pdf/10.1002/cpe.1064)
- ["FastTrack: Efficient and Precise Dynamic Race Detection" by Cormac Flanagan, Stephen N. Freund](https://dl.acm.org/doi/pdf/10.1145/1543135.1542490)

## Compilation

```bash
g++ -o racedetector main.cpp djit.cpp fasttrack.cpp parse.cpp
```

## Running

For DJIT+
```bash
./racedetector djit <log_file>
```

For Fasttrack
```bash
./racedetector fasttrack <log_file>
```

## Sample lines in log file

Type of inputs thשא can be there in the file :

Assuming, only these are possible (for now)

```
After lock release: TID: 2, Lock address: 0x74538c83aa08
After lock acquire: TID: 1, Lock address: 0x74538c83aa08
Before pthread_create(): Parent: 0
Before lock release: TID: 2, Lock address: 0x74538c83aa08
Before lock acquire: TID: 1, Lock address: 0x74538c83aa08
Thread begin: 0
TID: 0, IP: 0x74538c822211, ADDR: 0x74538c83ae06, Size (B): 1, isRead: 1
TID: 0, IP: 0x74538c83aab0, ADDR: 0x74538c82222f, Size (B): 8, isRead: 0
```