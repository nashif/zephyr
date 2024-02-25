
=====================
System  Requirements
=====================

This is the System Requirements Specification.

System capabilities
=====================

C Library
---------

.. item:: ZEP-2 Support Subset of Standard C Library
    :fulfilled_by: ZEP-12 ZEP-13 ZEP-14 ZEP-15 ZEP-16 ZEP-17 ZEP-18 ZEP-19 ZEP-20 ZEP-21 ZEP-22 

    Zephyr shall support a subset of the standard C library.


Device Driver API
-----------------

.. item:: ZEP-36 Device Driver Abstracting

    Zephyr shall provide a framework for managing device driver behavior (note: device drivers includes peripherals).


Exception and Error Handling
----------------------------

.. item:: ZEP-37 Fatal error and exception handling

    The Zephyr kernel shall provide a framework for error and exception handling.


File System
-----------


File Systems
------------

.. item:: ZEP-38 Common File system operation support

    Zephyr shall provide a framework for managing file system access.


Hardware Architecture Interface
-------------------------------

.. item:: ZEP-1 Architecture Layer Interface
    :fulfilled_by: ZEP-3 ZEP-8 ZEP-9 ZEP-10 ZEP-11 

    Zephyr shall provide a framework to communicate with a set of hardware architectural services.

.. item:: ZEP-3 Support multiprocessor management

    Zephyr shall support symmetric multiprocessing on multiple cores.


Interrupts
----------

.. item:: ZEP-39 Interrupt Service Routine

    Zephyr shall provide a framework for interrupt management.


Logging
-------

.. item:: ZEP-40 Logging

    Zephyr shall provide a framework for logging events.


Memory Management
-----------------

.. item:: ZEP-41 Memory Management framework

    Zephyr shall support a memory management framework.


Memory Objects
--------------


Memory Protection
-----------------


Mutex
-----


Power Management
----------------

.. item:: ZEP-42 Power Management

    Zephyr shall provide an interface to control hardware power states.


TBD: Memory Protection --> Thread
---------------------------------


Thread Communication
--------------------

.. item:: ZEP-43 Mutex

    Zephyr shall provide an interface for managing communcation between threads.


Thread Mapping (should it just be scheduling)
---------------------------------------------

.. item:: ZEP-44 Multiple CPU scheduling

    Zephyr shall support scheduling of threads on multiple hardware CPUs.


Thread Mapping (should it just be scheduling) -
-----------------------------------------------


Thread Mapping (should it just be scheduling?)
----------------------------------------------


Thread Scheduling
-----------------

.. item:: ZEP-4 Scheduling

    Zephyr shall provide an interface to assign a thread to a specific CPU.


Threads
-------

.. item:: ZEP-5 Managing threads

    Zephyr shall provide a framework for managing multiple threads of execution.


Timers
------

.. item:: ZEP-6 Timers
    :fulfilled_by: ZEP-27 ZEP-28 

    Zephyr shall provide a framework for managing time-based events.


Tracing
-------

.. item:: ZEP-7 Tracing

    Zepyhr shall provide a framework mechanism for tracing low level system operations  (NOTE: system calls, interrupts, kernel calls, thread, synchronization, etc.).


Utilities Library - Data Structures
-----------------------------------

.. item:: ZEP-35 Data Structures Library Utilities

    Zephyr shall provide common container data structures as library utilities   (ring buffer, linked list, red black trees, ....   see document from Anas)


see: https://docs.zephyrproject.org/latest/reference/kernel/index.html - Data Passing
-------------------------------------------------------------------------------------

