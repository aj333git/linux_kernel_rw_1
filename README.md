# RWLock Contention Experiment

A small Linux kernel module demonstrating **reader-writer lock behavior**, **multiple concurrent readers**, **exclusive writer access**, and **writer lock-acquisition latency**.

The experiment uses `rwlock_t` with four kernel threads acting as readers and one kernel thread acting as a writer.

---

## 1. Objective

This module demonstrates:

- `rwlock_t`
- `DEFINE_RWLOCK()`
- `read_lock()` / `read_unlock()`
- `write_lock()` / `write_unlock()`
- Multiple readers executing concurrently
- Exclusive writer access
- Reader-writer contention
- Measuring writer lock wait time using `ktime`
- Kernel threads using `kthread_run()`
- Graceful thread termination using `kthread_stop()`

---

## 2. Core Idea

An `rwlock` provides two different locking modes:

| Operation | Readers | Writers |
|---|---:|---:|
| Read lock | Multiple allowed | Blocks |
| Write lock | Blocks | Only one allowed |

The important property is:

```text
              RWLOCK
                |
       +--------+--------+
       |                 |
   READ LOCK         WRITE LOCK
       |                 |
 Multiple readers     Exclusive
 can enter            access
 simultaneously
 
 
 3. Experiment Architecture

The module creates:

                 shared_value
                      |
                      |
                +-----------+
                | test_lock |
                |  rwlock   |
                +-----------+
                      |
        +-------------+-------------+
        |             |             |
        v             v             v
   Reader-0      Reader-1      Reader-2
        |             |             |
        +-------------+-------------+
                      |
                   Reader-3

                      |
                      v

                   Writer

There are:

#define NUM_READERS 4

So the module creates:

4 Reader Kernel Threads
1 Writer Kernel Thread
4. Shared Resource

The readers and writer access:

static unsigned long shared_value;

The reader only observes the value:

read_lock(&test_lock);

pr_info("reader-%ld: value=%lu\n",
        id, shared_value);

read_unlock(&test_lock);

The writer modifies it:

write_lock(&test_lock);

shared_value++;

write_unlock(&test_lock);

The rwlock protects this shared state.

5. Reader Behavior

Each reader performs:

        |
        v
   read_lock()
        |
        v
   Read shared_value
        |
        v
     udelay()
        |
        v
   read_unlock()
        |
        v
     msleep()
        |
        +-------> repeat

The important point is that:

read_lock(&test_lock);

does not prevent another reader from acquiring the same read lock.

Therefore:

Reader-0 ----\
Reader-1 -----\
Reader-2 ------> READ LOCK ---> shared_value
Reader-3 -----/

Multiple readers can be inside the read-side critical section concurrently.

6. Why udelay(100) Is Used

The reader intentionally executes:

udelay(100);

while holding the read lock.

This makes the read-side critical section long enough to make contention easier to observe.

Conceptually:

Reader-0: [------ READ LOCK ------]
Reader-1: [------ READ LOCK ------]
Reader-2: [------ READ LOCK ------]
Reader-3: [------ READ LOCK ------]

Writer:              WAITING
                     |
                     |
                     v
              write_lock()

The writer cannot acquire the write lock while active readers are holding the lock.

7. Writer Behavior

The writer measures how long it takes to acquire the lock.

start = ktime_get();

write_lock(&test_lock);

end = ktime_get();

The difference is calculated:

wait_ns = ktime_to_ns(ktime_sub(end, start));

The result is printed:

writer: acquired lock, wait=XXXX ns value=Y

Therefore:

                  write_lock()
                       |
                       v
              +----------------+
              | Are readers   |
              | holding lock? |
              +----------------+
                 |          |
                YES         NO
                 |           |
                 v           v
              WAITING     ACQUIRE
                 |           |
                 +-----+-----+
                       |
                       v
                  increment
                  shared_value
8. What Does wait_ns Represent?

wait_ns represents the approximate time between:

start = ktime_get();

and successful acquisition of:

write_lock(&test_lock);

So it includes the time the writer spends waiting for incompatible lock holders to leave.

For example:

writer: acquired lock, wait=1500 ns value=1
writer: acquired lock, wait=87000 ns value=2

A larger value indicates that the writer spent more time waiting before obtaining the write lock.

This is a measurement of lock acquisition latency, not simply the duration of the writer's critical section.

9. Reader vs Writer Semantics
Reader
read_lock(&test_lock);

/* read shared data */

read_unlock(&test_lock);

Multiple readers can execute simultaneously.

Writer
write_lock(&test_lock);

/* modify shared data */

write_unlock(&test_lock);

The writer requires exclusive access.

Therefore:

READ + READ       = allowed
READ + READ + READ = allowed

READ + WRITE      = blocked
WRITE + READ      = blocked
WRITE + WRITE     = blocked

This is the fundamental distinction between reader and writer access.

10. Kernel Threads

Readers are created using:

kthread_run(reader_fn,
            (void *)(long)i,
            "rw-reader-%d",
            i);

The writer is created using:

kthread_run(writer_fn,
            NULL,
            "rw-writer");

The resulting execution model is:

                 Kernel Module
                      |
          +-----------+-----------+
          |                       |
       Readers                  Writer
          |                       |
   +------+------+                |
   |  |  |  |                    |
   v  v  v  v                    v
  R0 R1 R2 R3                    W
11. Timing Model

Readers run frequently:

msleep(10);

The writer runs less frequently:

msleep(100);

This creates repeated periods where:

Readers active
      |
      v
Writer requests write lock
      |
      v
Writer waits
      |
      v
Readers release lock
      |
      v
Writer acquires lock
      |
      v
shared_value++

This makes the module useful for observing contention experimentally.

12. Important Kernel APIs
API	Purpose
DEFINE_RWLOCK()	Statically initializes an rwlock
read_lock()	Acquires read side
read_unlock()	Releases read side
write_lock()	Acquires exclusive write side
write_unlock()	Releases write side
kthread_run()	Creates and starts kernel thread
kthread_should_stop()	Checks whether thread should terminate
kthread_stop()	Requests kernel thread termination
ktime_get()	Gets kernel time
ktime_sub()	Calculates time difference
ktime_to_ns()	Converts time to nanoseconds
udelay()	Short busy-wait delay
msleep()	Sleep for milliseconds
13. Important Observation

This experiment is intentionally different from a mutex experiment.

With a mutex:

Thread-1 ---- LOCK ---- critical section
                         |
Thread-2 ---- WAIT -----+

Only one owner exists.

With an rwlock:

Reader-1 ---- READ ----\
Reader-2 ---- READ -----+--> concurrent read access
Reader-3 ---- READ ----/
Reader-4 ---- READ ----/

Writer ------------- WAIT

The rwlock allows parallel read-side execution, while maintaining exclusive write-side access.

14. Expected Learning

After running this module, you should be able to connect the following concepts:

                rwlock_t
                   |
          +--------+--------+
          |                 |
       Readers            Writer
          |                 |
   shared read        exclusive access
          |                 |
          +--------+--------+
                   |
              contention
                   |
                   v
             wait latency
                   |
                   v
              ktime measurement

The experiment therefore connects reader-writer synchronization, kernel threads, shared data protection, contention, and lock-acquisition timing in one runnable kernel module.

15. Limitations

This is an educational contention experiment.

The measured wait_ns should not be treated as a precise benchmark of Linux rwlock performance. Scheduling, CPU topology, interrupts, preemption, kernel configuration, and system load can all affect observed timings.

The purpose is to understand the behavior and mechanics of contention, not to produce a controlled microbenchmark.

16. Summary

The central experiment is:

       4 Readers
           |
           | read_lock()
           v
    +--------------+
    |   RW LOCK    |
    +--------------+
           ^
           |
           | write_lock()
           |
        1 Writer
           |
           v
   Measure waiting time
           |
           v
      shared_value++

The key takeaway is:

An rwlock permits concurrent readers but requires exclusive access for a writer.

This module makes the resulting reader/writer interaction observable through kernel logs and measures the writer's lock-acquisition wait time.
