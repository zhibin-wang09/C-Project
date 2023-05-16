# C-Projects
A bunch of different programs that I've worked on.
***NOTE*** The programs in this directory is only compatible with the Linux platform as it uses some of the MACROS, etc... that are only defined in the Linux system.

### Diff File Patcher
A command line program that is able to apply a .diff file to a given file and producing an edited, target version.

### Debug Legacy Patcher
Debugged a legacy 1985 patch program written by Larry Wall in K&R C with the assist of gdb and valgrind.

### Memory Allocator
Created a custom memory allocator with the idea of segregated free lists and quicklists that allows for "cached" block sizes. In addition with a few unit testing.
The memory allocator uses the following policies
1. Free lists segregated by size class, using first-fit policy within each size class,
augmented with a set of "quick lists" holding small blocks segregated by size.
2. Immediate coalescing of large blocks on free with adjacent free blocks;
delayed coalescing on free of small blocks.
3. Boundary tags to support efficient coalescing, with footer optimization that allows
footers to be omitted from allocated blocks.
4. Block splitting without creating splinters.
5. Allocated blocks aligned to "single memory row" (8-byte) boundaries.
Free lists maintained using last in first out (LIFO) discipline.
6. Use of a prologue and epilogue to achieve required alignment and avoid edge cases
at the end of the heap.


### Cryptocurrency Exchange Watcher
Built a multi-process command line interface program that enables users to speculate multiple ongoing trades while processing and storing relevant information.
Backboned with the following concepts
1. Process execution such as forking, executing, and reaping
2. Signal handling
3. I/O redirectoin


### Game Server
Designed and engineered a multi-threaded Tic-Tac-Toe game server that supports up to 65 users simultaneously with the use of network sockets.

