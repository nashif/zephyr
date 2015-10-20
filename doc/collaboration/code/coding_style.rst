.. _coding_style:

Coding Style
############

Use this coding guideline to ensure that your development complies with
the project's style and naming conventions.

In general, follow the `Linux kernel coding style`_, with the following
exceptions:

* Add braces to every ``if`` and ``else`` body, even for single-line code
  blocks. Use the :option:`--ignore BRACES` flag to make :program:`checkpatch`
  stop complaining.
* Use hard tab stops. Set the tab width to either 4 or 8 spaces. Train
  :program:`checkpatch` to only warn when lines are over 100
  characters. In general, break lines at 80 characters where possible.
  If you are trying to align comments after declarations, use spaces
  instead of tabs to align them.
* Use C89-style single line comments, :literal:`/* */`. The C99-style
  single line comment, //, is not allowed.
* Use :literal:`/**  */` for any comments that need to appear in the
  documentation.

Checking for Conformity Using Checkpatch
****************************************

The Linux kernel GPL-licensed tool :program:`checkpatch` is used to
check coding style conformity. :program:`Checkpatch` is available in the
scripts directory. To invoke it when committing code, edit your
:file:`.git/hooks/pre-commit` file to contain:

.. code-block:: bash

   #!/bin/sh

   set -e exec

   exec git diff --cached | ${ZEPHYR_BASE}/scripts/checkpatch.pl - || true

.. _Linux kernel coding style: https://www.kernel.org/doc/Documentation/CodingStyle
