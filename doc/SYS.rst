
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

    The Zephyr RTOS shall support a subset of the standard C library.


Data Passing
------------


Device Driver API
-----------------


Device Drivers
--------------

.. item:: ZEP-36 Device Driver Abstraction

    The Zephyr RTOS shall provide a framework for managing device drivers and peripherals.


Exception and Error Handling
----------------------------

.. item:: ZEP-37 Fatal error and exception handling

    The Zephyr RTOS shall provide a framework for error and exception handling.


File System
-----------


File Systems
------------

.. item:: ZEP-38 Common File system operation support

    The Zephyr RTOS shall provide a framework for managing file system access.


Hardware Architecture Interface
-------------------------------

.. item:: ZEP-1 Architecture Layer Interface
    :fulfilled_by: ZEP-8 ZEP-9 ZEP-10 ZEP-11 ZEP-3 

    The Zephyr RTOS shall provide a framework to communicate with a set of hardware architectural services.

.. item:: ZEP-3 Support multiprocessor management

    The Zephyr RTOS shall support symmetric multiprocessing on multiple cores.


Interrupts
----------

.. item:: ZEP-39 Interrupt Service Routine

    The Zephyr RTOS shall provide a framework for interrupt management.


Logging
-------

.. item:: ZEP-40 Logging

    The Zephyr RTOS shall provide a framework for logging events.


Memory Management
-----------------

.. item:: ZEP-41 Memory Management framework

    The Zephyr RTOS shall support a memory management framework.


Memory Objects
--------------


Memory Protection
-----------------


Multi Core
----------


Mutex
-----

.. item:: ZEP-43 Mutex

    The Zephyr RTOS shall provide an interface for managing communication between threads.


Power Management
----------------

.. item:: ZEP-42 Power Management

    The Zephyr RTOS shall provide an interface to control hardware power states.


SMP and Multi core
------------------

.. item:: ZEP-44 Multiple CPU scheduling

    The Zephyr RTOS shall support scheduling of threads on multiple hardware CPUs.

.. item:: ZEP-4 Scheduling

    The Zephyr RTOS shall provide an interface to assign a thread to a specific CPU.


Semaphore
---------

.. item:: ZEP-99 Counting Semaphore
    :fulfilled_by: ZEP-12001 ZEP-12002 ZEP-12004 ZEP-12005 ZEP-12006 ZEP-12007 ZEP-12008 ZEP-12009 ZEP-12010 ZEP-12011 ZEP-12012 ZEP-12013 ZEP-12014 ZEP-12015 ZEP-12023 ZEP-12024 ZEP-12025 

    The system shall implement a semaphore synchronization primitive for coordinating access to shared resources among multiple threads.


Thread Communication
--------------------


Thread Scheduling
-----------------


Threads
-------

.. item:: ZEP-123 Thread support
    :fulfilled_by: ZEP-112 

    The Zephyr RTOS shall support threads.

.. item:: ZEP-5 Thread management
    :fulfilled_by: ZEP-23 ZEP-108 ZEP-109 ZEP-110 ZEP-111 ZEP-124 ZEP-98 ZEP-25 ZEP-26 ZEP-125 ZEP-126 

    The Zephyr RTOS shall provide a framework for managing multiple threads of execution.

.. item:: ZEP-122 Thread priority
    :fulfilled_by: ZEP-107 

    Threads shall have a priority.


Timers
------

.. item:: ZEP-6 Timers
    :fulfilled_by: ZEP-27 ZEP-28 

    The Zephyr RTOS shall provide a framework for managing time-based events.


Tracing
-------

.. item:: ZEP-7 Tracing

    Zepyhr shall provide a framework mechanism for tracing low level system operations  (NOTE: system calls, interrupts, kernel calls, thread, synchronization, etc.).

