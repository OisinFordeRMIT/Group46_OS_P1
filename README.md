# Group46_OS_P1 - COSC1114 Operating Systems Principles, Project 1

Group: group46
Members: Oisin Forde (s4094143), Thisul Deven (s3988824)

## Tasks completed

| Task | Description | Status |
|------|-------------|--------|
| Task 1 | Multithreaded multiple file copying (`mmcopier`) | Done |
| Task 2 - Subtask 1 | Reader and writer teams over a shared queue (`mscopier`) | Done |
| Task 2 - Subtask 2 | `pthread_mutex` locks on the critical sections | Done |
| Task 2 - Subtask 3 | Avoid busy waiting | In progress |

## Compiling

Tested on jupiter.csit.rmit.edu.au.

```bash
make clean
make all
```

`make all` builds `mmcopier` and `mscopier`. `make clean` removes the
executables and object files. Both compile under `-Wall -Werror` with no
warnings.

## Running

### Task 1 - mmcopier

```bash
./mmcopier n source_dir destination_dir
```

`n` is between 2 and 10 and sets both the number of files and the number of
threads. Thread `i` copies `source_dir/source<i>.txt` to
`destination_dir/source<i>.txt`. If `destination_dir` does not exist,
mmcopier creates it.

```bash
unzip source_dir.zip
./mmcopier 3 source_dir destination_dir
```

That copies source1.txt, source2.txt and source3.txt with 3 threads.

### Task 2 - mscopier

```bash
./mscopier n source_file destination_file
```

`n` readers and `n` writers copy one file through a shared queue.

## How Task 1 works

Each thread gets its own `CopyJob` struct: where to read from, where to
write to, and a flag it flips once the copy worked. That is the whole of
the state. Nothing crosses between threads, so mmcopier has no mutex in it
anywhere. Every thread opens its own two streams and gets on with its own
file, which is the point of task 1.

The copying goes line by line with `getline`, same as the file reading in
the week 5 lab. One catch: `getline` swallows the newline it stopped on.
The code puts it back, except on the last line of a file that never had
one. `eof()` tells those two apart. Miss it and every copy comes out a byte
short.

`main` keeps a count of how many threads actually started. If
`pthread_create` gives up halfway, only the threads that exist get joined,
so nothing is still running when the program returns. Every pthread call
and every file operation gets its return value checked.

`n` tops out at 10, so the threads and jobs sit in fixed arrays. No heap,
nothing to free, nothing to leak.

## Testing Task 1

I ran it with every n from 2 to 10 against the supplied `source_dir`.
`diff -r` came back clean each time and the file count always matched n.
Then the awkward ones: an empty file, a file that is mostly blank lines,
and a file with no newline at the end. All three came out identical.

Bad arguments get their own pass. Wrong number of them, a non-numeric n, n
outside 2 to 10, a source directory that is not there, a destination that
cannot be written. Each one prints a message and exits 1 instead of
falling over.

On jupiter, valgrind reports no leaks and no errors:

```
valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./mmcopier 10 source_dir dest_dir
```

```
in use at exit: 0 bytes in 0 blocks
total heap usage: 136 allocs, 136 frees, 225,397 bytes allocated
ERROR SUMMARY: 0 errors from 0 contexts
```
