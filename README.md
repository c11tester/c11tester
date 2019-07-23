C11Fuzzer: A Fuzzer for C11 and C++11 Atomics
=====================================================

C11Fuzzer is a fuzzer for C11/C++11 which randomly explores the
behaviors of code under the C/C++ memory model.

C11Fuzzer is constructed as a dynamically-linked shared library which
implements the C and C++ atomic types and portions of the other thread-support
libraries of C/C++ (e.g., std::atomic, std::mutex, etc.).

C11Fuzzer should compile on Linux OSX.  Instrumenting programs
requires using our LLVM pass.  It likely can be ported to other \*NIX
flavors.


Getting Started
---------------

If you haven't done so already, you may download C11Fuzzer using git:

      git clone git://demsky.eecs.uci.edu/c11fuzzer.git

Get the benchmarks (not required; distributed separately):

      git clone git://demsky.eecs.uci.edu/concurrency-benchmarks.git

Compile the fuzzer:

      make

To see the help message on how to run C11Fuzzer, execute:

      ./run.sh -h


Useful Options
--------------

`-v`

  > Verbose: show all executions and not just buggy ones.

`-u num`

  > Value to provide to atomics loads from uninitialized memory locations. The
  > default is 0, but this may cause some programs to throw exceptions
  > (segfault) before the model checker prints a trace.

Benchmarks
-------------------

Many simple tests are located in the `tests/` directory. You may also want to
try the larger benchmarks (distributed separately), which can be placed under
the `benchmarks/` directory. After building C11Fuzzer, you can build and run
the benchmarks as follows:

>     make benchmarks
>     cd benchmarks
>
>     # run barrier test with fairness/memory liveness
>     ./run.sh barrier/barrier -y -m 2
>
>     # Linux reader/write lock test with fairness/memory liveness
>     ./run.sh linuxrwlocks/linuxrwlocks -y -m 2
>
>     # run all benchmarks and provide timing results
>     ./bench.sh


Running your own code
---------------------

You likely want to test your own code, not just our simple tests. To do so, you
need to perform a few steps.

First, because C11Fuzzer executes your program dozens (if not hundreds or
thousands) of times, you will have the most success if your code is written as a
unit test and not as a full-blown program.

Second, because C11Fuzzer must be able to manage your program for you, your
program should declare its main entry point as `user_main(int, char**)` rather
than `main(int, char**)`.

Third, test programs must use the standard C11/C++11 library headers (see below
for supported APIs) and must compile against the versions provided in
C11Fuzzer's `include/` directory. Notably, we only support C11 thread syntax
(`thrd_t`, etc. from `<thread.h>`).

Test programs may also use our included happens-before race detector by
including <librace.h> and utilizing the appropriate functions
(`store_{8,16,32,64}()` and `load_{8,16,32,64}()`) for storing/loading data
to/from non-atomic shared memory.

C11Fuzzer can also check boolean assertions in your test programs. Just
include `<model-assert.h>` and use the `MODEL_ASSERT()` macro in your test program.
C11Fuzzer will report a bug in any possible execution in which the argument to
`MODEL_ASSERT()` evaluates to false (that is, 0).

Test programs should be compiled against our shared library (libmodel.so) using
the headers in the `include/` directory. Then the shared library must be made
available to the dynamic linker, using the `LD_LIBRARY_PATH` environment
variable, for instance.


### Supported C11/C++11 APIs ###

To model-check multithreaded code properly, C11Fuzzer needs to instrument any
concurrency-related API calls made in your code. Currently, we support parts of
the following thread-support libraries. The C versions can be used in either C
or C++.

* `<atomic>`, `<cstdatomic>`, `<stdatomic.h>`
* `<condition_variable>`
* `<mutex>`
* `<threads.h>`

Because we want to extend support to legacy (i.e., non-C++11) compilers, we do
not support some new C++11 features that can't be implemented in C++03 (e.g.,
C++ `<thread>`).

Reading an execution trace
--------------------------

When C11Fuzzer detects a bug in your program (or when run with the `--verbose`
flag), it prints the output of the program run (STDOUT) along with some summary
trace information for the execution in question. The trace is given as a
sequence of lines, where each line represents an operation in the execution
trace. These lines are ordered by the order in which they were run by C11Fuzzer
(i.e., the "execution order"), which does not necessarily align with the "order"
of the values observed (i.e., the modification order or the reads-from
relation).

The following list describes each of the columns in the execution trace output:

 * \#: The sequence number within the execution. That is, sequence number "9"
   means the operation was the 9th operation executed by C11Fuzzer. Note that
   this represents the execution order, not necessarily any other order (e.g.,
   modification order or reads-from).

 * t: The thread number

 * Action type: The type of operation performed

 * MO: The memory-order for this operation (i.e., `memory_order_XXX`, where `XXX` is
   `relaxed`, `release`, `acquire`, `rel_acq`, or `seq_cst`)

 * Location: The memory location on which this operation is operating. This is
   well-defined for atomic write/read/RMW, but other operations are subject to
   C11Fuzzer implementation details.

 * Value: For reads/writes/RMW, the value returned by the operation. Note that
   for RMW, this is the value that is *read*, not the value that was *written*.
   For other operations, 'value' may have some C11Fuzzer-internal meaning, or
   it may simply be a don't-care (such as `0xdeadbeef`).

 * Rf: For reads, the sequence number of the operation from which it reads.
   [Note: If the execution is a partial, infeasible trace (labeled INFEASIBLE),
   as printed during `--verbose` execution, reads may not be resolved and so may
   have Rf=? or Rf=Px, where x is a promised future value.]

 * CV: The clock vector, encapsulating the happens-before relation (see our
   paper, or the C/C++ memory model itself). We use a Lamport-style clock vector
   similar to [1]. The "clock" is just the sequence number (#). The clock vector
   can be read as follows:

   Each entry is indexed as CV[i], where

            i = 0, 1, 2, ..., <number of threads>

   So for any thread i, we say CV[i] is the sequence number of the most recent
   operation in thread i such that operation i happens-before this operation.
   Notably, thread 0 is reserved as a dummy thread for certain C11Fuzzer
   operations.

See the following example trace:

    ------------------------------------------------------------------------------------
    #    t    Action type     MO       Location         Value               Rf  CV
    ------------------------------------------------------------------------------------
    1    1    thread start    seq_cst  0x7f68ff11e7c0   0xdeadbeef              ( 0,  1)
    2    1    init atomic     relaxed        0x601068   0                       ( 0,  2)
    3    1    init atomic     relaxed        0x60106c   0                       ( 0,  3)
    4    1    thread create   seq_cst  0x7f68fe51c710   0x7f68fe51c6e0          ( 0,  4)
    5    2    thread start    seq_cst  0x7f68ff11ebc0   0xdeadbeef              ( 0,  4,  5)
    6    2    atomic read     relaxed        0x60106c   0                   3   ( 0,  4,  6)
    7    1    thread create   seq_cst  0x7f68fe51c720   0x7f68fe51c6e0          ( 0,  7)
    8    3    thread start    seq_cst  0x7f68ff11efc0   0xdeadbeef              ( 0,  7,  0,  8)
    9    2    atomic write    relaxed        0x601068   0                       ( 0,  4,  9)
    10   3    atomic read     relaxed        0x601068   0                   2   ( 0,  7,  0, 10)
    11   2    thread finish   seq_cst  0x7f68ff11ebc0   0xdeadbeef              ( 0,  4, 11)
    12   3    atomic write    relaxed        0x60106c   0x2a                    ( 0,  7,  0, 12)
    13   1    thread join     seq_cst  0x7f68ff11ebc0   0x2                     ( 0, 13, 11)
    14   3    thread finish   seq_cst  0x7f68ff11efc0   0xdeadbeef              ( 0,  7,  0, 14)
    15   1    thread join     seq_cst  0x7f68ff11efc0   0x3                     ( 0, 15, 11, 14)
    16   1    thread finish   seq_cst  0x7f68ff11e7c0   0xdeadbeef              ( 0, 16, 11, 14)
    HASH 4073708854
    ------------------------------------------------------------------------------------

Now consider, for example, operation 10:

This is the 10th operation in the execution order. It is an atomic read-relaxed
operation performed by thread 3 at memory address `0x601068`. It reads the value
"0", which was written by the 2nd operation in the execution order. Its clock
vector consists of the following values:

        CV[0] = 0, CV[1] = 7, CV[2] = 0, CV[3] = 10

End of Execution Summary
------------------------

C11Fuzzer prints summary statistics at the end of each execution. These
summaries are based off of a few different properties of an execution, which we
will break down here:

* An _infeasible_ execution is an execution which is not consistent with the
  memory model. Such an execution can be considered overhead for the
  model-checker, since it should never appear in practice.

* A _buggy_ execution is an execution in which C11Fuzzer has found a real
  bug: a data race, a deadlock, failure of a user-provided assertion, or an
  uninitialized load, for instance. C11Fuzzer will only report bugs in feasible
  executions.

* A _redundant_ execution is a feasible execution that is exploring the same
  state space explored by a previous feasible execution. Such exploration is
  another instance of overhead, so C11Fuzzer terminates these executions as
  soon as they are detected. C11Fuzzer is mostly able to avoid such executions
  but may encounter them if a fairness option is enabled.

Now, we can examine the end-of-execution summary of one test program:

    $ ./run.sh test/rmwprog.o
    + test/rmwprog.o
    ******* Model-checking complete: *******
    Number of complete, bug-free executions: 6
    Number of redundant executions: 0
    Number of buggy executions: 0
    Number of infeasible executions: 29
    Total executions: 35

* _Number of complete, bug-free executions:_ these are feasible, non-buggy, and
  non-redundant executions. They each represent different, legal behaviors you
  can expect to see in practice.

* _Number of redundant executions:_ these are feasible but redundant executions
  that were terminated as soon as C11Fuzzer noticed the redundancy.

* _Number of buggy executions:_ these are feasible, buggy executions. These are
  the trouble spots where your program is triggering a bug or assertion.
  Ideally, this number should be 0.

* _Number of infeasible executions:_ these are infeasible executions,
  representing some of the overhead of model-checking.

* _Total executions:_ the total number of executions explored by C11Fuzzer.
  Should be the sum of the above categories, since they are mutually exclusive.


Other Notes and Pitfalls
------------------------

* Many programs require some form of fairness in order to terminate in a finite
  amount of time. C11Fuzzer supports the `-y num` and `-f num` flags for these
  cases. The `-y` option (yield-based fairness) is preferable, but it requires
  careful usage of yields (i.e., `thrd_yield()`) in the test program. For
  programs without proper `thrd_yield()`, you may consider using `-f` instead.

* Deadlock detection: C11Fuzzer can detect deadlocks. For instance, try the
  following test program.

  >     ./run.sh test/deadlock.o

  Deadlock detection currently detects when a thread is about to step into a
  deadlock, without actually including the final step in the trace. But you can
  examine the program to see the next step.

* C11Fuzzer has to speculatively explore many execution behaviors due to the
  relaxed memory model, and many of these turn out to be infeasible (that is,
  they cannot be legally produced by the memory model). C11Fuzzer discards
  these executions as soon as it identifies them (see the "Number of infeasible
  executions" statistic); however, the speculation can occasionally cause
  C11Fuzzer to hit unexpected parts of the unit test program (causing a
  division by 0, for instance). In such programs, you might consider running
  C11Fuzzer with the `-u num` option.

* Related to the previous point, C11Fuzzer may report more than one bug for a
  particular candidate execution. This is because some bugs may not be
  reportable until C11Fuzzer has explored more of the program, and in the
  time between initial discovery and final assessment of the bug, C11Fuzzer may
  discover another bug.

* Data races may be reported as multiple bugs, one for each byte-address of the
  data race in question. See, for example, this run:

        $ ./run.sh test/releaseseq.o
        ...
        Bug report: 4 bugs detected
          [BUG] Data race detected @ address 0x601078:
            Access 1: write in thread  2 @ clock   4
            Access 2:  read in thread  3 @ clock   9
          [BUG] Data race detected @ address 0x601079:
            Access 1: write in thread  2 @ clock   4
            Access 2:  read in thread  3 @ clock   9
          [BUG] Data race detected @ address 0x60107a:
            Access 1: write in thread  2 @ clock   4
            Access 2:  read in thread  3 @ clock   9
          [BUG] Data race detected @ address 0x60107b:
            Access 1: write in thread  2 @ clock   4
            Access 2:  read in thread  3 @ clock   9


See Also
--------

The C11Fuzzer project page:

>   <http://demsky.eecs.uci.edu/c11modelchecker.html>

The C11Fuzzer source and accompanying benchmarks on Gitweb:

>   <http://demsky.eecs.uci.edu/git/?p=model-checker.git>
>
>   <http://demsky.eecs.uci.edu/git/?p=model-checker-benchmarks.git>


Contact
-------

Please feel free to contact us for more information. Bug reports are welcome,
and we are happy to hear from our users. We are also very interested to know if
C11Fuzzer catches bugs in your programs.

Contact Weiyu Luo at <weiyul7@uci.edu> or Brian Demsky at <bdemsky@uci.edu>.


Copyright
---------

Copyright &copy; 2013 and 2019 Regents of the University of California. All rights reserved.

C11Fuzzer is distributed under the GPL v2. See the LICENSE file for details.


References
----------

[1] L. Lamport. Time, clocks, and the ordering of events in a distributed
    system. CACM, 21(7):558-565, July 1978.
