
===================================
Software Requirements Specification
===================================

This is the Software Requirements Specification.

Software capabilities
=====================

C Library
---------

.. item:: ZEP-12 Formatted output
    :status: not reviewed

    The Zephyr RTOS shall support formatted output.

.. item:: ZEP-13 Floating Point Math Support
    :status: not reviewed

    The Zephyr RTOS shall support floating point math libraries for processors where floating point is available.

.. item:: ZEP-14 Boolean Primitives Support
    :status: not reviewed

    The Zephyr RTOS shall support boolean primitives.

.. item:: ZEP-15 Standard Unix time interface
    :status: not reviewed

    The Zephyr RTOS shall support the standard UNIX time interface.

.. item:: ZEP-16 Strings support
    :status: not reviewed

    The Zephyr RTOS shall support an interface to manage strings.

.. item:: ZEP-17 Moving/copying regions of memory
    :status: not reviewed

    The Zephyr RTOS shall support an interface to move contents between regions of memory.

.. item:: ZEP-18 I/O based interface
    :status: not reviewed

    The Zephyr RTOS shall support a file i/O based interface for driver communication.

.. item:: ZEP-19 C99 integer types
    :status: not reviewed

    The Zephyr RTOS shall support standard C99 integer types.

.. item:: ZEP-20 Standard System Error Numbers (IEEE Std 1003.1-2017)
    :status: not reviewed

    The Zephyr RTOS shall support standard system error numbers as defined by IEEE Std 1003.1-2017.

.. item:: ZEP-21 Document set of Zephyr OS required C library functions in Safety Manual
    :status: not reviewed

    The set of C Library functions required by Zephyr needs to be documented in the Zephyr Safety Manual.

.. item:: ZEP-22 Support external C libraries documentation in Zephyr Safety Manual
    :status: not reviewed

    The Zephyr Safety Manual needs to specify how to configure the support of external C Libraries.


Data Passing
------------

.. item:: ZEP-89 Traditional FIFO Queue
    :status: not reviewed

    The Zephyr RTOS shall provide a kernel object that implements a traditional first in, first out (FIFO) queue, allowing threads and ISRs to add and remove a limited number of 32-bit data values.

.. item:: ZEP-90 Traditional LIFO queue
    :status: not reviewed

    The Zephyr RTOS shall provide a kernel object that implements a traditional last in, first out (LIFO) queue, allowing threads and ISRs to add and remove a limited number of 32-bit data values.


Device Driver API
-----------------

.. item:: ZEP-45 Device Driver Abstraction
    :status: not reviewed

    The Zephyr RTOS shall provide abstraction of device drivers with common functionalities as an intermediate interface between applications and device drivers, where such interface is implemented by individual device drivers.

    Proposal for replacement: Zephyr shall provide an interface between application and individual device drivers to provide an abstraction of device drivers with common functionalities.

.. item:: ZEP-46 Expose kernel to hardware interrupts
    :status: not reviewed

    The Zephyr RTOS shall provide an interface for managing a defined set of hardware exceptions (including interrupts) across all systems.


Device Drivers
--------------


Exception and Error Handling
----------------------------

.. item:: ZEP-47 Fatal Exception Error Handler
    :status: not reviewed

    The Zephyr RTOS shall provide default handlers for exceptions.

.. item:: ZEP-48 Default handler for fatal errors
    :status: not reviewed

    The Zephyr RTOS shall provide default handlers for fatal errors that do not have a dedicated handler.

.. item:: ZEP-49 Assigning a specific handler
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to assign a specific handler with an exception.

.. item:: ZEP-50 Assigning a specific handler (2)
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to assign a specific handler for a fatal error.


File System
-----------

.. item:: ZEP-51 Create file
    :status: not reviewed

    Zephyr shall provide file create capabilities for files on the file system.

.. item:: ZEP-52 Open files
    :status: not reviewed

    Zephyr shall provide file open capabilities for files on the file system.

.. item:: ZEP-53 Read files
    :status: not reviewed

    Zephyr shall provide read access to files in the file system.

.. item:: ZEP-54 Write to files
    :status: not reviewed

    Zephyr shall provide write access to the files in the file system.

.. item:: ZEP-55 Close file
    :status: not reviewed

    Zephyr shall provide file close capabilities for files on the file system.

.. item:: ZEP-56 Move file
    :status: not reviewed

    Zephyr shall provide the capability to move files on the file system.

.. item:: ZEP-57 Delete file
    :status: not reviewed

    Zephyr shall provide file delete capabilities for files on the file system.


File Systems
------------


Hardware Architecture Interface
-------------------------------

.. item:: ZEP-8 Atomic Operations
    :status: not reviewed

    The Zephyr RTOS shall provide an interface functionality to access memory while ensuring mutual exclusion. Note: Implementation by atomic variables and accessing them by APIs.

.. item:: ZEP-9 Thread Context Switching
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism for context switching between threads.

.. item:: ZEP-10 Software Exceptions
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to implement software exceptions.

.. item:: ZEP-11 Processor Mode Support
    :status: not reviewed

    The Zephyr RTOS shall provide an interface for managing processor modes.


Interrupts
----------

.. item:: ZEP-58 Service routine for handling interrupts (ISR)
    :status: not reviewed

    The Zephyr RTOS shall provide support a service routine for handling interrupts (ISR).

.. item:: ZEP-59 Multi-level interrupts
    :status: not reviewed

    The Zephyr RTOS shall support multi-level preemptive interrupt priorities, when supported by hardware. Note: detailed analysis to demonstrate non interference will be needed here.

.. item:: ZEP-60 Associating application code with interrupts
    :status: not reviewed

    The Zephyr RTOS shall provide an interface for associating application code with specific interrupts. (CLARIFY: Can it be a deferred procedure call at interrupt context? Would be different requirement)

.. item:: ZEP-61 Enabling interrupts
    :status: not reviewed

    The Zephyr RTOS shall provide mechanisms to enable interrupts.

.. item:: ZEP-62 Disabling interrupts
    :status: not reviewed

    The Zephyr RTOS shall provide mechanisms to disable interrupts.


Logging
-------

.. item:: ZEP-63 Dedicated Logging Thread Support
    :status: not reviewed

    The Zephyr RTOS shall support isolation of logging from other functionality.

.. item:: ZEP-64 Logs available for post processing
    :status: not reviewed

    The Zephyr RTOS logging shall produce logs that are capable of being post processed.

.. item:: ZEP-65 Formatting log messages
    :status: not reviewed

    The Zephyr RTOS logging shall support formatting of log messages to enable filtering.

.. item:: ZEP-66 Logging Filtering Support
    :status: not reviewed

    The Zephyr RTOS logging system shall support filtering based on severity level.

.. item:: ZEP-67 Multiple Backend Logging Support
    :status: not reviewed

    The Zephyr RTOS shall support logging messages to multiple system resources.

.. item:: ZEP-68 Deferred Logging Support
    :status: not reviewed

    The Zephyr RTOS shall support deferred logging (TODO: need more detail about the constraints and limits on what can be deferred).


Memory Management
-----------------


Memory Objects
--------------

.. item:: ZEP-87 Dynamic Memory Allocation
    :status: not reviewed

    The Zephyr RTOS shall allow threads to dynamically allocate variable-sized memory regions from a specified range of memory.

.. item:: ZEP-88 Memory Slab Object
    :status: not reviewed

    The Zephyr RTOS shall allow threads to dynamically allocate fixed-sized memory regions from a specified range of memory.


Memory Protection
-----------------

.. item:: ZEP-69 Memory Protection
    :status: not reviewed

    The Zephyr RTOS shall support memory protection features to isolate a thread's memory region.

.. item:: ZEP-70 Granting access to kernel objects
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism to grant user threads access to kernel objects.

.. item:: ZEP-71 Separation between user and kernel threads for memory access
    :status: not reviewed

    The Zephyr RTOS shall be able to differentiate between user threads and kernel threads for memory access.

.. item:: ZEP-72 Safely handle unimplemented calls or invalid system calls
    :status: not reviewed

    The Zephyr RTOS shall have a defined behaviour when an invocation of an unimplemented system call is made.

.. item:: ZEP-73 Response to invalid system call IDs
    :status: not reviewed

    The Zephyr RTOS shall have a defined behaviour when an invalid system call ID is used.

.. item:: ZEP-74 Prevent user threads creating higher priority threads
    :status: not reviewed

    The Zephyr RTOS shall prevent user threads from creating new threads that are higher priority than the caller.

.. item:: ZEP-75 Revoking threads permissions on a kernel object
    :status: not reviewed

    The Zephyr RTOS shall support revoking permission to a kernel object. User mode threads may only revoke their own access to an object.

.. item:: ZEP-76 Prevent user threads creating supervisor threads
    :status: not reviewed

    The Zephyr RTOS shall prevent user threads from creating kernel threads.

.. item:: ZEP-77 Reduced Privilege Level Threads
    :status: not reviewed

    The Zephyr RTOS shall allow the creation of threads that run in reduced privilege level.

.. item:: ZEP-78 User Mode Threads Performing Privileged Operations
    :status: not reviewed

    The Zephyr RTOS shall provide system calls to allow user mode threads to perform privileged operations.

.. item:: ZEP-79 User mode handling of detected stack overflow
    :status: not reviewed

    The Zephyr RTOS shall support a defined mechanism for user mode handling a of detected stack overflow.

.. item:: ZEP-80 Stack Overflow Detection
    :status: not reviewed

    The Zephyr RTOS shall support detection of stack overflows.

.. item:: ZEP-81 Boot Time Memory Access Policy
    :status: not reviewed

    The Zephyr RTOS shall support configurable access to memory during boot time.

.. item:: ZEP-82 System Call Handler Functions
    :status: not reviewed

    The Zephyr RTOS shall provide helper functions for system call handler functions to validate the inputs passed in from user mode before invoking the implementation function to protect the kernel.

.. item:: ZEP-83 System Call C strings in user mode
    :status: not reviewed

    The Zephyr RTOS shall support system calls to be able to safely accept C strings passed in from user mode.

.. item:: ZEP-84 Tracking kernel objects in used by user mode threads
    :status: not reviewed

    The Zephyr RTOS shall track kernel objects that are used by user mode threads.

    Note: this means Zephyr shall track the resources used by the user mode thread (associate this with a user story).

.. item:: ZEP-85 Granting threads access to specific memory
    :status: not reviewed

    The Zephyr RTOS shall have an interface to request access to specific memory after initial allocation.

.. item:: ZEP-86 Assigning memory pools to act as a thread resource pool
    :status: not reviewed

    The Zephyr RTOS shall support assigning a memory pool to act as that thread's resource pool.


Multi Core
----------

.. item:: ZEP-104 Support operation on more than one CPU
    :status: not reviewed

    The Zephyr RTOS shall support operation on more than one physical CPU sharing the same kernel state.

.. item:: ZEP-105 Running threads on specific CPUs
    :status: not reviewed

    The Zephyr RTOS shall provide an interface for running threads on specific sets of CPUs ( default is 1 CPU).

.. item:: ZEP-106 Exclusion between physical CPUs
    :status: not reviewed

    The Zephyr RTOS shall provide an interface for mutual exclusion between multiple physical CPUs.


Mutex
-----

.. item:: ZEP-91 Mutex Kernel Object
    :status: not reviewed

    The Zephyr RTOS shall support resource synchronization. (Note synchronization can be for memory access, and mutex may be one implementation, but not the only one).


Power Management
----------------

.. item:: ZEP-92 Power State Control
    :status: not reviewed

    The Zephyr RTOS shall provide control over changes to system power states.

.. item:: ZEP-93 Power Management
    :status: not reviewed

    TBD

.. item:: ZEP-94 Notification of changes to system power states
    :status: not reviewed

    The Zephyr RTOS shall provide notification of changes to system power states.


SMP and Multi core
------------------


Semaphore
---------

.. item:: ZEP-12001 Counting Semaphore Definition At Compile Time
    :validated_by: test_k_sem_define
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism to define and initialize a semaphore at compile time.

.. item:: ZEP-12002 Counting Semaphore Definition At Run Time
    :validated_by: test_k_sem_init test_sem_thread2thread
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism to define and initialize a semaphore at runtime.

.. item:: ZEP-12004 Initialialization with maximum count value
    :validated_by: test_k_sem_correct_count_limit
    :status: not reviewed

    When initializing a counting semaphore, the maximum permitted count a semaphore
    can have shall be set.

.. item:: ZEP-12005 Initialialization with initial semaphore value
    :validated_by: test_k_sem_define
    :status: not reviewed

    When initializing a counting semaphore, the initial semaphore value shall be set.

.. item:: ZEP-12006 Semaphore acquisition mechanism
    :validated_by: test_k_sem_init test_sem_take_multiple test_sem_take_timeout test_sem_take_timeout_forever test_sem_take_timeout_isr
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism allowing threads to acquire a semaphore.

.. item:: ZEP-12007 Semaphore acquisition with count greater than zero
    :validated_by: test_sem_take_no_wait
    :status: not reviewed

    If the semaphore's count is greater than zero, the requesting thread shall acquire
    the semaphore and decrement its count.

.. item:: ZEP-12008 Semaphore acquisition with zero count
    :validated_by: test_sem_take_timeout_fails
    :status: not reviewed

    If the semaphore's count is zero, the requesting thread shall be blocked until the semaphore is released by another thread.

.. item:: ZEP-12009 Semaphore acquisition timeout
    :validated_by: test_k_sem_correct_count_limit test_sem_measure_timeout_from_thread test_sem_measure_timeouts test_sem_multiple_take_and_timeouts
    :status: not reviewed

    When attempting to acquire a semaphore, the Zephyr RTOS shall accept options that specify timeout periods, allowing threads to set a maximum wait time for semaphore acquisition.

.. item:: ZEP-12010 Semaphore acquisition timeout error handling
    :validated_by: test_sem_measure_timeouts test_sem_take_timeout test_sem_take_timeout_fails
    :status: not reviewed

    When attempting to acquire a semaphore, where the semaphore is not acquired within the
    specified time, the Zephyr RTOS shall return an error indicating a timeout.

.. item:: ZEP-12011 Semaphore acquisition no wait error handling
    :validated_by: test_sem_measure_timeouts test_sem_take_no_wait test_sem_take_no_wait_fails
    :status: not reviewed

    When attempting to acquire a semaphore, where the current count is zero and no waiting time was provided, the Zephyr RTOS
    shall return an error indicating the semaphore is busy.

.. item:: ZEP-12012 Semaphore release
    :validated_by: test_sem_give_from_isr test_sem_give_from_thread
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism allowing threads to release a semaphore.

.. item:: ZEP-12013 Semaphore release
    :validated_by: test_sem_give_from_thread
    :status: not reviewed

    The Zephyr RTOS shall increment the semaphore's count upon release.

.. item:: ZEP-12014 Semaphore release with priority inheritance
    :validated_by: test_sem_take_multiple
    :status: not reviewed

    If there are threads waiting on the semaphore, the highest-priority waiting thread
    shall be unblocked and acquire the semaphore.

.. item:: ZEP-12015 Checking semaphore count
    :validated_by: test_sem_count_get
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism for threads to check the current count of a semaphore without acquiring it.

.. item:: ZEP-12023 Semaphore reset
    :validated_by: test_sem_give_from_thread test_sem_reset
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism that resets the semaphore count to zero.

.. item:: ZEP-12024 Semaphore acquisitions abort after reset
    :validated_by: test_sem_reset_waiting
    :status: not reviewed

    When a semaphore is reset, the Zephyr RTOS shall abort all existing acquisitions
    of the semaphore returning a resource contention error code.

.. item:: ZEP-12025 Semaphore Initialization Option Validation
    :validated_by: test_k_sem_init
    :status: not reviewed

    When initializing a counting semaphore, where the maximum permitted count of a semaphore is invalid,
    the Zephyr RTOS shall return an error indicating invalid values.

.. item:: ZEP-12003 Maximum limit of a semaphore
    :validated_by: test_k_sem_init_max
    :status: not reviewed

    The Zephyr RTOS shall define the maximum limit of a semaphore when the semaphore is used for counting purposes and does not have an explicit limit.


Thread Communication
--------------------

.. item:: ZEP-96 Exchanging data between threads
    :status: not reviewed

    The Zephyr RTOS shall provide a mechanism for exchanging data between threads.

.. item:: ZEP-97 Waiting for results during communication
    :status: not reviewed

    The Zephyr RTOS shall provide mechanisms to enable waiting for results during communication between threads. (NOTE:  waiting for results is really bad and dangerous, want to avoid if at all possible).

.. item:: ZEP-100 Poll Operation Support
    :status: not reviewed

    The Zephyr RTOS shall support a poll operation which enables waiting concurrently for any one of multiple conditions to be fulfilled.

.. item:: ZEP-101 Pipe Communication Primitive
    :status: not reviewed

    The Zephyr RTOS shall provide a communication primitive that allows a thread to transfer a block of
    data to another thread.

.. item:: ZEP-102 Message Queue
    :status: not reviewed

    The Zephyr RTOS shall provide a a communication primitive that allow threads and ISRs to asynchronously exchange fixed-size data items.

.. item:: ZEP-103 Mailbox Kernel Primitive
    :status: not reviewed

    The Zephyr RTOS shall provide a communication primitive that allows threads to exchange messages of varying sizes asynchronously or synchronously.


Thread Scheduling
-----------------

.. item:: ZEP-112 Scheduling a thread based on an event
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to schedule a thread based on an event.

.. item:: ZEP-114 Deadline Scheduling Priorities
    :status: not reviewed

    The Zephyr RTOS shall organize running threads by earliest deadline first priority.

.. item:: ZEP-115 Work Queue utility capable of running preemptible work items
    :status: not reviewed

    The Zephyr RTOS shall provide a thread-pooled work queue utility capable of running preemptible work items with specific scheduler priorities.

.. item:: ZEP-24 Run user supplied functions in-order in a separate thread(s)
    :status: not reviewed

    The Zephyr RTOS shall provide an interface for running user-supplied functions.

.. item:: ZEP-116 Organize running threads into a fixed list
    :status: not reviewed

    The Zephyr RTOS shall organize running threads into a fixed list of numeric priorities.

.. item:: ZEP-117 Preemption support
    :status: not reviewed

    The Zephyr RTOS shall support preemption of a running thread by a higher priority thread.

.. item:: ZEP-118 Un-preemptible thread priorities
    :status: not reviewed

    The Zephyr RTOS shall support thread priorities which cannot be preempted by other user threads.

.. item:: ZEP-119 Time sharing of CPU resources
    :status: not reviewed

    The Zephyr RTOS shall support time sharing of CPU resources among threads of the same priority.


Threads
-------

.. item:: ZEP-23 Creating threads
    :validated_by: test_kdefine_coop_thread test_kdefine_preempt_thread
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to create (start) a thread.

.. item:: ZEP-108 Suspending a thread
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to suspend a thread.

.. item:: ZEP-109 Resuming a suspended thread
    :validated_by: test_resume_unsuspend_thread test_threads_suspend test_threads_suspend_resume_cooperative test_threads_suspend_resume_preemptible test_threads_suspend_timeout
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to resume a suspended thread.

.. item:: ZEP-110 Resuming a suspended thread after a timeout
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to resume a suspended thread after a timeout.

.. item:: ZEP-111 Deleting a thread
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to delete (end) a thread.

.. item:: ZEP-124 Thread states
    :status: not reviewed

    Threads shall have different states to fulfill the Life-cycle of a thread

.. item:: ZEP-98 Thread stack objects
    :status: not reviewed

    Every Thread shall have it's own stack.

.. item:: ZEP-25 Thread privileges
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to create threads with defined privilege.

.. item:: ZEP-26 Scheduling multiple threads
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to schedule multiple threads.

.. item:: ZEP-125 Thread Options
    :status: not reviewed

    The Zephyr RTOS shall support a set of thread options.

.. item:: ZEP-126 Thread Custom Data
    :status: not reviewed

    Every thread shall have a custom data area.

.. item:: ZEP-107 Setting thread priority
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to set a thread's priority.


Timers
------

.. item:: ZEP-27 Kernel Clock
    :status: not reviewed

    The Zephyr RTOS shall provide a interface for checking the current value of the real-time clock.

.. item:: ZEP-28 Call functions in interrupt context
    :status: not reviewed

    The Zephyr RTOS shall provide an interface to schedule user mode call back function triggered by a real time clock value.


Tracing
-------

.. item:: ZEP-29 Initializing a trace
    :status: not reviewed

    Zephyr shall provide an interface to initialize a trace.

.. item:: ZEP-30 Triggering a trace
    :status: not reviewed

    Zephyr shall provide an interface to trigger a trace.

.. item:: ZEP-31 Dumping trace results
    :status: not reviewed

    Zephyr shall provide an interface to dump results from a trace.

.. item:: ZEP-32 Removing trace data
    :status: not reviewed

    Zephyr shall provide an interface to remove trace data.

.. item:: ZEP-33 Tracing Object  Identification
    :status: not reviewed

    Zephyr shall provide an interface to identify the objects being traced.

.. item:: ZEP-34 Tracing Non-Interference
    :status: not reviewed

    Zepyhr shall prevent the tracing functionality from interfering with normal operations.

