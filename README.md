# Thread Pool Implementation in C
This project provides a simple implementation of a thread pool in C using pthreads. Thread pools are useful for managing and executing a large number of tasks concurrently by reusing a fixed number of threads.

## Features
- **Dynamic Thread Management**: Adjusts the number of threads based on workload.
- **Task Queue**: Manages tasks efficiently using a linked list queue.
- **Concurrency**: Executes tasks concurrently using pthreads.
- **Join and Destroy**: Provides functions to wait for tasks to complete and safely shut down the thread pool.

## Usage
### Prerequisites
- GCC compiler
- pthreads library

### Compilation
```bash
make
```

### Execution
```bash
./bin/use_threadpool NUM_THREADS NUM_TASKS
```
- **NUM_THREADS**: Number of threads in the thread pool.
- **NUM_TASKS**: Number of tasks to execute.

### Example
```bash
./bin/use_threadpool 4 10
```
This command runs the program with 4 threads and schedules 10 tasks to be executed concurrently.

## Structure
- **src/**: Contains the source files (threadpool.c, use_threadpool.c).
- **bin/**: Output directory for compiled files (threadpool.o, use_threadpool.o, use_threadpool executable).

## Contributing
Contributions are welcome! Fork the repository and submit a pull request with your improvements.
