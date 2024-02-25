
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

    Zephyr shall support formatted output.

.. item:: ZEP-13 Floating Point Math Support
    :status: not reviewed

    Zephyr shall support floating point math libraries for processors where floating point is available.

.. item:: ZEP-14 Boolean Primitives Support
    :status: not reviewed

    Zephyr shall support boolean primitives.

.. item:: ZEP-15 Standard Unix time interface
    :status: not reviewed

    Zephyr shall support the standard unix time interface.

.. item:: ZEP-16 Strings support
    :status: not reviewed

    Zephyr shall support an interface to manage strings.

.. item:: ZEP-17 Moving/copying regions of memory
    :status: not reviewed

    Zephyr shall support an interface to move contents between regions of memory.

.. item:: ZEP-18 I/O based interface
    :status: not reviewed

    Zephyr shall support a file i/O based interface for driver communication.

.. item:: ZEP-19 C99 integer types
    :status: not reviewed

    Zephyr shall support standard C99 integer types.

.. item:: ZEP-20 Standard System Error Numbers (IEEE Std 1003.1-2017)
    :status: not reviewed

    Zephyr shall support standard system error numbers as defined by IEEE Std 1003.1-2017.

.. item:: ZEP-21 Document set of Zephyr OS required C librariy functions in Safety Manual
    :status: not reviewed

    The set of C Library functions required by Zephyr needs to be documented in the Zephyr Safety Manual.

.. item:: ZEP-22 Support external C libraries documentation in Zephyr Safety Manual
    :status: not reviewed

    The Zephyr Safety Manual needs to specify how to configure the support of external C Libraries.


Device Driver API
-----------------

.. item:: ZEP-45 Device Driver Abstraction
    :status: not reviewed

    Zephyr shall provide abstraction of device drivers with common functionalities as an intermediate interface between applications and device drivers, where such interface is implemented by individual device drivers.

Proposal for replacement: Zephyr shall provide an interface between application and individual device drivers to provide an abstraction of device drivers with common functionalities.

.. item:: ZEP-46 Expose kernel to hardware interrupts
    :status: not reviewed

    Zephyr shall provide an interface for managing a defined set of hardware exceptions (including interupts) across all systems.


Exception and Error Handling
----------------------------

.. item:: ZEP-47 Fatal Exception Error Handler
    :status: not reviewed

    Zephyr shall provide default handlers for exceptions.

.. item:: ZEP-48 Default handler for fatal errors
    :status: not reviewed

    Zephyr shall provide default handlers for fatal errors that do not have a dedicated handler.

.. item:: ZEP-49 Assigning a specific handler
    :status: not reviewed

    Zephyr shall provide an interface to assign a specific handler with an exception.

.. item:: ZEP-50 Assigning a specific handler (2)
    :status: not reviewed

    Zephyr shall provide an interface to assign a specific handler for a fatal error.


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

    Zephyr shall provide an interface functionality to access memory while ensuring mutual exclusion. Note: Implementation by atomic variables and accessing them by APIs.

.. item:: ZEP-9 Thread Context Switching
    :status: not reviewed

    Zephyr shall provide a mechanism for context switching between threads.

.. item:: ZEP-10 Software Exceptions
    :status: not reviewed

    Zephyr shall provide an interface to implement software exceptions.

.. item:: ZEP-11 Processor Mode Support
    :status: not reviewed

    Zephyr shall provide an interface for managing processor modes.


Interrupts
----------

.. item:: ZEP-58 Service routine for handling interrupts (ISR)
    :status: not reviewed

    Zephyr shall provide support a service routine for handling interrupts (ISR).

.. item:: ZEP-59 Multi-level interrupts
    :status: not reviewed

    Zephyr shall support multi-level preemptive interrupt priorities, when supported by hardware. Note: detailed analysis to demonstrate non interferenace will be needed here.

.. item:: ZEP-60 Associating application code with interrupts
    :status: not reviewed

    Zephyr shall provide an interface for associating application code with specific interrupts. (CLARIFY: Can it be a deferred procedure call at interrupt context? Would be different requirement)

.. item:: ZEP-61 Enabling interrupts
    :status: not reviewed

    Zephyr shall provide mechanisms to enable interrupts.

.. item:: ZEP-62 Disabling interrupts
    :status: not reviewed

    Zephyr shall provide mechanisms to disable interrupts.


Logging
-------

.. item:: ZEP-63 Dedicated Logging Thread Support
    :status: not reviewed

    Zephyr shall support isolation of logging from other functionality.

.. item:: ZEP-64 Logs available for post processing
    :status: not reviewed

    Zephyr logging shall produce logs that are capable of being post processed.

.. item:: ZEP-65 Formatting log messages
    :status: not reviewed

    Zephyr logging shall support formatting of log messages to enable filtering.

.. item:: ZEP-66 Logging Filtering Support
    :status: not reviewed

    Zephyr logging system shall support filtering based on severity level.

.. item:: ZEP-67 Multiple Backend Logging Support
    :status: not reviewed

    Zephyr shall support logging messages to multiple system resources.

.. item:: ZEP-68 Deferred Logging Support
    :status: not reviewed

    Zephyr shall support deferred logging (TODO: need more detail about the constraints and limits on what can be deferred).


Memory Management
-----------------


Memory Objects
--------------

.. item:: ZEP-87 Dynamic Memory Allocation
    :status: not reviewed

    Zephyr shall allow threads to dynamically allocate variable-sized memory regions from a specified range of memory.

.. item:: ZEP-88 Memory Slab Object
    :status: not reviewed

    Zephyr shall allow threads to dynamically allocate fixed-sized memory regions from a specified range of memory.


Memory Protection
-----------------

.. item:: ZEP-69 Memory Protection
    :status: not reviewed

    Zephyr shall support memory protection features to isolate a thread's memory region.

.. item:: ZEP-70 Granting access to kernel objects
    :status: not reviewed

    Zephyr shall provide a mechanism to grant user threads access to kernel objects.

.. item:: ZEP-71 Separation between user and kernel threads for memory access
    :status: not reviewed

    Zephyr shall be able to differentiate between user threads and kernel threads for memory access.

.. item:: ZEP-72 Safely handle unimplemented calls or invalid system calls
    :status: not reviewed

    Zephyr shall have a defined behaviour when an invocation of an unimplemented system call is made.

.. item:: ZEP-73 Response to invalid system call IDs
    :status: not reviewed

    Zephyr shall have a defined behavior when an invalid system call ID is used.

.. item:: ZEP-74 Prevent user threads creating higher priority threasds
    :status: not reviewed

    Zephyr shall prevent user threads from creating new threads that are higher priority than the caller.

.. item:: ZEP-75 Revoking threads permissions on a kernel object
    :status: not reviewed

    Zephyr shall support revoking permission to a kernel object. User mode threads may only revoke their own access to an object.

.. item:: ZEP-78 User Mode Threads Performing Privileged Operations
    :status: not reviewed

    Zephyr shall provide system calls to allow user mode threads to perform privileged operations.

.. item:: ZEP-79 User mode handling of detected stack overflow
    :status: not reviewed

    Zephyr shall support a defined mechanism for user mode handling a of detected stack overflow.

.. item:: ZEP-80 Stack Overflow Detection
    :status: not reviewed

    Zephyr shall support detection of stack overflows.

.. item:: ZEP-81 Boot Time Memory Access Policy
    :status: not reviewed

    Zephyr shall support configurable access to memory during boot time.

.. item:: ZEP-82 System Call Handler Functions
    :status: not reviewed

    Zephyr shall provide helper functions for system call handler functions to validate the inputs passed in from user mode before invoking the implementation function to protect the kernel.

.. item:: ZEP-83 System Call C strings in user mode
    :status: not reviewed

    Zephyr shall support system calls to be able to safely accept C strings passed in from user mode.

.. item:: ZEP-84 Tracking kernel objects in used by usermode threads
    :status: not reviewed

    Zephyr shall track kernel objects that are used by user mode threads.

Note: this means Zephyr shall track the resources used by the user mode thread (associate this with a user story).

.. item:: ZEP-85 Granting threads access to specific memory
    :status: not reviewed

    Zephyr shall have an interface to request access to specific memory after initial allocation.

.. item:: ZEP-86 Assigning memory pools to act as a thread resource pool
    :status: not reviewed

    Zephyr shall support assigning a memory pool to act as that thread's resource pool.


Mutex
-----

.. item:: ZEP-91 Mutex Kernel Object
    :status: not reviewed

    Zephyr shall support resource synchronization. (Note synchronization can be for memory access, and mutex may be one implementation, but not the only one).


Power Management
----------------

.. item:: ZEP-92 Power State Control
    :status: not reviewed

    Zephyr shall provide control over changes to system power states.

.. item:: ZEP-93 Power Management - TBD
    :status: not reviewed

    TBD

.. item:: ZEP-94 Notification of changes to system power states
    :status: not reviewed

    Zephyr shall provide notification of changes to system power states.


TBD: Memory Protection --> Thread
---------------------------------

.. item:: ZEP-76 Prevent user threads creating supervisor threads
    :status: not reviewed

    Zephyr shall prevent user threads from creating kernel threads.

.. item:: ZEP-77 Reduced Privilege Level Threads
    :status: not reviewed

    Zephyr shall allow the creation of threads that run in reduced privilege level.


Thread Communication
--------------------

.. item:: ZEP-95 Mailbox
    :status: not reviewed

    Zephyr shall provide mechanisms for thread synchronization.

.. item:: ZEP-96 Exchanging data between threads
    :status: not reviewed

    Zephyr shall provide a mechanism for exchanging data between threads.

.. item:: ZEP-97 Waiting for results during communication
    :status: not reviewed

    Zephyr shall provide mechanisms to enable waiting for results during communication between threads. (NOTE:  waiting for results is really bad and dangerous, want to avoid if at all possible).

.. item:: ZEP-98 Stack
    :status: not reviewed

    The Zephyr kernel shall provide a stack.

.. item:: ZEP-99 Traditional Counting Semaphore
    :validated_by: test_k_sem_init 
    :status: not reviewed

    Zephyr shall provide a counting semaphore abstraction for queuing and mutual exclusion.

.. item:: ZEP-100 Poll Operation Support
    :status: not reviewed

    Zephyr shall support a poll operation which enables waiting concurrently for any one of multiple conditions to be fulfilled.

.. item:: ZEP-101 Pipe Kernel Object
    :status: not reviewed

    Zephyr shall provide a kernel object that allows a thread to transfer a block of data to another thread.

.. item:: ZEP-102 Message Queue Kernel Object
    :status: not reviewed

    Zephyr shall provide a kernel object that implements a simple message queue, allowing threads and ISRs to asynchronously send and receive fixed-size data items.

.. item:: ZEP-103 Mailbox Abstraction
    :status: not reviewed

    Zephyr shall support a mailbox abstraction to enable targeted message passing between threads.


Thread Mapping (should it just be scheduling)
---------------------------------------------

.. item:: ZEP-105 Running threads on specific CPUs
    :status: not reviewed

    Zephyr shall provide an interface for running threads on specific sets of CPUs ( default is 1 CPU).


Thread Mapping (should it just be scheduling) -
-----------------------------------------------

.. item:: ZEP-104 Support operation on more than one CPU
    :status: not reviewed

    The Zephyr kernel shall support operation on more than one physical CPU sharing the same kernel state.


Thread Mapping (should it just be scheduling?)
----------------------------------------------

.. item:: ZEP-106 Exclusion between physical CPUs
    :status: not reviewed

    Zephyr shall provide an interface for mutual exclusion between multiple physical CPUs.


Thread Scheduling
-----------------

.. item:: ZEP-23 Creating threads
    :validated_by: test_kdefine_coop_thread test_kinit_coop_thread test_kdefine_preempt_thread test_kinit_preempt_thread 
    :status: not reviewed

    Zephyr shall provide an interface to create (start) a thread.

.. item:: ZEP-107 Setting thread priority
    :status: not reviewed

    Zephyr shall provide an interface to set a thread's priority.

.. item:: ZEP-108 Suspending a thread
    :status: not reviewed

    Zephyr shall provide an interface to suspend a thread.

.. item:: ZEP-109 Resuming a suspended thread
    :validated_by: test_threads_suspend_resume_cooperative test_threads_suspend test_threads_suspend_timeout test_resume_unsuspend_thread test_threads_suspend_resume_preemptible 
    :status: not reviewed

    Zephyr shall provide an interface to resume a suspended thread.

.. item:: ZEP-111 Deleting a thread
    :status: not reviewed

    Zephyr shall provide an interface to delete (end) a thread.

.. item:: ZEP-112 Scheduling a thread based on an event
    :status: not reviewed

    Zephyr shall provide an interface to schedule a thread based on an event.

.. item:: ZEP-113 Meta-IRQ Priorities
    :status: not reviewed

    The Zephyr kernel shall support running threads in Meta-IRQ Priorities.

.. item:: ZEP-114 Deadline Scheduling Priorities
    :status: not reviewed

    Zephyr shall organize running threads by earliest deadline first priority.

.. item:: ZEP-115 Work Queue utility capable of running preemptible work items
    :status: not reviewed

    Zephyr shall provide a thread-pooled work queue utility capable of running preemptible work items with specific scheduler priorities.

.. item:: ZEP-24 Run user supplied functions in-order in a separate thread(s)
    :status: not reviewed

    Zephyr shall provide an interface for running user-supplied functions.

.. item:: ZEP-116 Organize running threads into a fixed list
    :status: not reviewed

    Zephyr shall organize running threads into a fixed list of numeric priorities (see: https://docs.zephyrproject.org/latest/kernel/services/threads/index.html#thread-priorities).

.. item:: ZEP-117 Preemption support
    :status: not reviewed

    Zephyr shall support preemption of a running thread by a higher priority thread.

.. item:: ZEP-118 Un-preemptable thread priorities
    :status: not reviewed

    Zephyr shall support thread priorities which cannot be preempted by other user threads.

.. item:: ZEP-119 Time sharing of CPU resources
    :status: not reviewed

    Zephyr shall support time sharing of CPU resources among threads of the same priority.


Threads
-------

.. item:: ZEP-120 Thread Support
    :status: not reviewed

    Zephyr shall support multiple threads of execution.

.. item:: ZEP-25 Thread privileges
    :status: not reviewed

    Zephyr shall provide an interface to create threads with defined privilege.

.. item:: ZEP-26 Scheduling multiple threads
    :status: not reviewed

    Zephyr shall provide an interface to schedule multiple threads.

.. item:: ZEP-121 Threads - TBD
    :status: not reviewed

    ??? Can Zephyr change priviledge level of a thread once created??


Timers
------

.. item:: ZEP-27 Kernel Clock
    :status: not reviewed

    Zephyr shall provide a interface for checking the current value of the real-time clock.

.. item:: ZEP-28 Call functions in interrupt context
    :status: not reviewed

    Zephyr shall provide an interface to schedule user mode call back function triggered by a real time clock value.


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


Utilities Library - Data Structures
-----------------------------------


see: https://docs.zephyrproject.org/latest/reference/kernel/index.html - Data Passing
-------------------------------------------------------------------------------------

.. item:: ZEP-89 Traditional FIFO Queue
    :status: not reviewed

    Zephyr shall provide a kernel object that implements a traditional first in, first out (FIFO) queue, allowing threads and ISRs to add and remove a limited number of 32-bit data values.

.. item:: ZEP-90 Traditional LIFO queue
    :status: not reviewed

    Zephyr shall provide a kernel object that implements a traditional last in, first out (LIFO) queue, allowing threads and ISRs to add and remove a limited number of 32-bit data values.

