# Group46_OS_P1 - COSC1114 Project 1

Group: group46
Members: Oisin Forde (s4094143), Thisul Deven (s3988824)

## What is done

| Task | Description | Status |
|------|-------------|--------|
| Task 1 | Multiple file copying, `mmcopier` | Done |
| Task 2 subtask 1 | Reader and writer teams over a shared queue, `mscopier` | Done |
| Task 2 subtask 2 | `pthread_mutex` on the critical sections | Done |
| Task 2 subtask 3 | Busy waiting removed with `pthread_cond` | Done |

Subtask 3 uses condition variables, not `sleep()`.

## Building

```bash
make clean
make all
```

That builds both programs. `make clean` deletes them again. Both compile with
`-Wall -Werror` and no warnings. Tested on jupiter.csit.rmit.edu.au.

## Running

```bash
./mmcopier n source_dir destination_dir
./mscopier n source_file destination_file
```

`n` is between 2 and 10 in both. For mmcopier it is the number of files and
threads, so thread `i` copies `source<i>.txt`. For mscopier it is the size of
each team, so `./mscopier 10 input output` runs 10 readers and 10 writers.

mmcopier creates the destination directory if it is missing. mscopier takes
the two paths exactly as typed.

```bash
unzip source_dir.zip
./mmcopier 3 source_dir destination_dir

bash generate_text.sh 10000 > input
./mscopier 10 input output
diff input output
```

## Task 1, mmcopier

Each thread gets a `CopyJob` struct: where to read from, where to write to,
and a flag it flips when the copy worked. Nothing crosses between threads, so
there is no mutex in mmcopier anywhere. Every thread opens its own two streams
and handles its own file, which is the point of the task.

`main` counts how many threads actually started. If `pthread_create` gives up
halfway, only the threads that exist get joined.

## Task 2, mscopier

**The queue.** A 20 slot array used as a ring, with a head, a tail, and a
count. Wrapping the indexes with `%` means nothing ever shuffles along when a
line comes off the front.

**The lock.** One mutex covers everything shared: the queue, both file
streams, and the reader bookkeeping. One lock means there is no second lock to
deadlock against.

Both the `getline` and the write happen while the lock is held, and that is
deliberate. The readers share one input stream, so taking turns is the only
thing that works. It also keeps "read a line" and "add that line" as a single
step. Split them and two readers could swap places on the way to the queue,
which scrambles the copy. Same reasoning for the writers.

**No busy waiting.** Two condition variables do the waiting:

- `hasRoom` puts a reader to sleep when the queue is full. A writer signals it
  after freeing a slot.
- `hasWork` puts a writer to sleep when the queue is empty. A reader signals
  it after adding a line.

Both waits sit in `while` loops rather than `if`, because a condition variable
is allowed to wake a thread for no reason. When the last reader reaches the
end of the file it broadcasts on `hasWork`, so writers waiting on an empty
queue wake up, finish what is left, and exit instead of hanging.

The lecture slides cover threads but not mutexes or condition variables, so
the `pthread_cond` calls come from section 12 of the assignment brief and the
man pages.

## Newlines

`getline` throws away the newline it stopped on. Both programs add it back,
except on the last line of a file that never had one, which `eof()` picks out.
Without that check every copy comes out a byte short.

## Errors and memory

Every `pthread` call, file open, and write gets its return value checked, and
anything that fails prints to stderr and exits 1. If a write fails partway,
mscopier sets a flag and broadcasts on both condition variables so the threads
stop rather than hang. The mutex and both condition variables get destroyed
before the program returns.

Neither program allocates anything on the heap. `n` never goes above 10, so
the threads and jobs live in fixed arrays.

## Testing

mmcopier ran with every n from 2 to 10 against the supplied `source_dir`.
`diff -r` came back clean each time and the file count always matched n.

mscopier ran 30 times with random n over a 50,000 line file built from
`wordlist.10000`. Every run matched the input byte for byte.

I put both through an empty file, a file of blank lines, and a file with no
newline on the end. Bad arguments get their own pass: wrong count, a
non-numeric n, n out of range, a missing source, a destination that cannot be
written.

valgrind on jupiter reports nothing for either program.

```
valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./mscopier 10 input output
```

```
in use at exit: 0 bytes in 0 blocks
total heap usage: 10,027 allocs, 10,027 frees
ERROR SUMMARY: 0 errors from 0 contexts
```
