.. zephyr:code-sample:: ztest_integration
   :name: Ztest assertions

   Write a minimal Ztest suite exercising the assertion macros.

Overview
********

This is the smallest useful Ztest application: one test suite holding one test
case, which calls each of the common assertion macros with an input that
passes.

It is a starting point for writing a test rather than a demonstration of a
Zephyr feature. Between them, :file:`src/main.c` and :file:`CMakeLists.txt`
show everything a new test needs: declaring a suite with
:c:macro:`ZTEST_SUITE`, adding a case to it with :c:macro:`ZTEST`, and building
the result against the Ztest library.

The assertions it uses are the ones most tests are built from:

* :c:macro:`zassert_true` and :c:macro:`zassert_false` for conditions
* :c:macro:`zassert_is_null` and :c:macro:`zassert_not_null` for pointers
* :c:macro:`zassert_equal` and :c:macro:`zassert_equal_ptr` for comparisons

Each is passed a message that is printed only if that assertion fails.

Building and Running
********************

The sample is built by Twister as a build only test:

.. code-block:: console

   west twister -p native_sim -s sample.testing.ztest

It can also be built and run directly, in which case it reports one passing
test case:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/testsuite/integration
   :board: native_sim
   :goals: build run
   :compact:
