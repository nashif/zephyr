.. zephyr:code-sample:: pytest_basic
   :name: Pytest custom arguments

   Pass custom command line arguments from Twister to a pytest harness.

Overview
********

This sample shows how a test that uses the pytest harness receives arguments
from Twister, and how it declares the arguments it is willing to accept.

The Zephyr side is deliberately minimal: a single ztest case that prints a
line. Everything the sample demonstrates is on the pytest side, in
:zephyr_file:`samples/subsys/testsuite/pytest/basic/pytest/`.

Twister passes arguments to pytest through ``pytest_args`` in
:file:`tests.yaml`. Pytest rejects options it does not know, so each one has to
be declared in ``conftest.py`` with ``pytest_addoption()`` and then exposed as a
fixture. This sample declares two:

* ``--cmdopt``, which Twister sets to the directory holding the artifacts the
  ztest run produced. The test asserts that the directory exists, which is the
  usual reason to want this argument: it is how a pytest test reaches the output
  of the build and run that preceded it.
* ``--custom-pytest-arg``, an arbitrary value carried straight through from
  :file:`tests.yaml` to the test, showing that the mechanism is not limited to
  the arguments Twister supplies itself.

The sample also shows an autouse fixture, which every test requests
automatically without naming it, used here to re-export an argument into the
environment.

For a sample that drives an application through its shell instead of inspecting
build artifacts, see :zephyr:code-sample:`pytest_shell`.

Building and Running
********************

The sample is run by Twister, which builds the application, runs it and then
runs pytest against it:

.. code-block:: console

   west twister -p native_sim -s sample.twister.pytest

Both pytest test cases pass when the artifact directory Twister points
``--cmdopt`` at exists, and when ``--custom-pytest-arg`` arrives with the value
:file:`tests.yaml` set. The fixtures print the argument values as they run:

.. code-block:: console

   handle cmdopt:
   .
   run test cases in:
   .
