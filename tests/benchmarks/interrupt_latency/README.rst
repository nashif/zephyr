Interrupt Handling Benchmark
############################

This benchmark measures the individual components of interrupt handling in
Zephyr, with the goal of characterizing a platform for interrupt-heavy
applications. Where possible it uses a *real asynchronous interrupt* raised
through the platform interrupt controller, rather than the synchronous
``irq_offload()`` trap used by the ``latency_measure`` benchmark, so that the
hardware interrupt entry path is part of what is measured.

Scenarios
*********

* ``int.entry.trigger_to_isr`` -- time from the software write that raises
  the interrupt to the first timestamp taken inside the ISR (entry latency).
* ``int.exit.resume_interrupted`` -- time from the end of the ISR body back
  to the interrupted thread.
* ``int.exit.reschedule`` -- time from the end of an ISR that wakes a higher
  priority thread to that thread running (exit plus context switch).
* ``int.locked.unlock_to_isr`` -- the interrupt is raised while interrupts
  are locked and kept pending for a configurable window; measured is the
  time from ``irq_unlock()`` to ISR entry (latency after a critical
  section).
* ``int.throughput.round_trip`` -- the ISR re-triggers itself so interrupts
  are serviced back to back; the average cost of one full entry + ISR +
  exit round trip is reported. Its inverse is the maximum sustainable
  interrupt rate.
* ``int.dynamic.connect`` -- cost of installing an ISR at runtime with
  ``irq_connect_dynamic()`` (needs ``CONFIG_DYNAMIC_INTERRUPTS``).

Each sampled scenario reports ``min``, ``avg``, ``max`` and ``p99``
statistics over ``CONFIG_INT_BENCH_NUM_ITERATIONS`` iterations, in the same
console format used by the ``latency_measure`` benchmark so that twister
records the values (``twister.json`` / ``recording.csv``).

Trigger backends
****************

Interrupt generation is abstracted behind a small backend API
(``src/trigger.h``) selected with Kconfig:

``CONFIG_INT_BENCH_TRIGGER_SW_IRQ``
   Raises a real interrupt through the interrupt controller. Currently
   implemented for Cortex-M (NVIC STIR/ISPR), Arm GIC v2/v3 (SGI) and ARC
   (IRQ_HINT), using the same mechanisms as ``tests/arch/common/interrupt``.
   The IRQ line is auto-selected (an SGI on GIC, ``CONFIG_NUM_IRQS - 1``
   otherwise) and can be overridden with ``CONFIG_INT_BENCH_IRQ_LINE`` for
   SoCs where the automatic choice is not a free, implemented line.

``CONFIG_INT_BENCH_TRIGGER_OFFLOAD``
   Fallback for every other architecture, based on ``irq_offload()``. Since
   this is a synchronous trap, only the exit-path scenarios run with this
   backend; entry latency, critical section and throughput scenarios are
   not available.

Running
*******

.. code-block:: console

   west build -p -b qemu_cortex_m3 tests/benchmarks/interrupt_latency -t run

or via twister, which also collects the metrics:

.. code-block:: console

   scripts/twister -p qemu_cortex_m3 -T tests/benchmarks/interrupt_latency

Sample output::

   int.entry.trigger_to_isr.min - Trigger write to ISR entry (min)   :  25 cycles ,  208 ns :
   int.entry.trigger_to_isr.avg - Trigger write to ISR entry (avg)   :  26 cycles ,  219 ns :
   int.entry.trigger_to_isr.max - Trigger write to ISR entry (max)   :  61 cycles ,  512 ns :
   int.entry.trigger_to_isr.p99 - Trigger write to ISR entry (p99)   :  28 cycles ,  237 ns :

Notes on methodology
********************

* Timestamps use the timing subsystem (``CONFIG_TIMING_FUNCTIONS``); the
  average overhead of one ``timing_counter_get()`` call is calibrated at
  startup and subtracted from every sample.
* The system tick rate is reduced to one tick per second so timer
  interrupts do not perturb most samples; residual hits show up in ``max``,
  which is why ``p99`` is reported alongside it.
* Entry latency includes the cost of the trigger write itself and, on some
  interrupt controllers, the propagation delay of the software-generated
  interrupt. Numbers are therefore comparable across Zephyr versions and
  configurations on the same platform, and indicative across platforms.

Future work
***********

* sw-irq trigger backends for x86 (LOAPIC self-IPI), RISC-V (CLIC/mip) and
  Xtensa (INTSET).
* Nested interrupt preemption latency (two lines, two priorities).
* Direct ISR (``IRQ_DIRECT_CONNECT``) and zero-latency IRQ comparison
  scenarios.
* Latency distribution histograms and background-load variants.
* SMP scenarios (IPI latency, ISR on non-boot CPU).
