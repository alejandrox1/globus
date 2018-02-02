# Random File Transfer

This small project implements a server and a client that communicate via tcp.

The server will create `NFILES` (1000 in this case) from output coming from
`/dev/urandom`. These files will vary in size ranging from 1 MB to 10 MB and
will be written to the current working directory from where the server is being
ran.
All filenames have a hardcoded file extension of `.out` and have a name that
can be stored in a 10 byte array.

Both the client and the server will generate a log file in their respective
working directories detailing
```
<filename> <size in bytes> <md5 checksum>
```

The server has parameters to handle connection backlog and how many connections
to manage concurrently.
The client, on the other hand, was designed with minimal functionality, having
a small CPU and memory footprint with easy to use options to specify the server
to which it is to connect to. As such, the client creates only one connection
during its lifetime.

# Performance and Design
While genrating the files to be transfered, the server will output to stdout
how long it takes to write all the required files (ASUS ZENBOOK UX303UB
Intel(R) Core(TM) i7-6700HQ CPU @ 2.60GHz running Ubuntu 16.04.3 LTS, it takes
about 30 seconds).

For more detailed information on how to test and time this process run 
`integrationtest.sh`. Using this script, in the same system, the transfer of
1000 files over tcp took about 30 seconds.
On average, 1000 files following the above specifications will take up about 5
GB of space, resulting in an effective transfer of about 150 MB/sec.

In order to improve off of this design it would be beneficial to perform
further benchmarks testing the size of the bufer used to read/write data.

In terms of design, the author tried to be consistent only using buffered vs
unbuffered IO calls, however this may not be the case in many places.
Refactoring the code to provide a clean and consistent interface should be a
goal. In terms of performance, analysing the effect these calls have on
different places throughout the code will result in possibly a more efficient
and performant server/client set up.

Memory management wise, one of the most demanding tasks is done by the server
storing a list of all the generated files. This was implemented using a scandir
call which uses dynamically allocated memory, this will cause problems when the
total number of files surpasses a couple thousand files.

Furthermore, the use of threads is rather limited and better concurrency
patterns may arise with future refactoring.

Looking at the optimal use of IO calls, bufer sizes, calls per thread, overall
work distribution will give insights into how to improve transfer rates between
server and client.

# Portability
The code presented here has been thoroughly tested on Ubuntu 16.04.3 LTS. 
It is intended to work with little modifications on most linux like systems, 
incuding Free BSD, and Open BSD, although this has not been tested.

### [source](https://github.com/alejandrox1/Algorithmscpp/tree/master/servers/filetransfer)
