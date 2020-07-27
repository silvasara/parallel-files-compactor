# Parallel Files Compressor 

Given a source directory, the program performs recursive and parallel compression of all files belonging to the source directory and its subdirectories using **bzip2**.

The program has a result equivalent to the following sequence of commands:

``` bash
$ cp -ax SOURCE_DIR DEST_DIR.bz2
$ cd DESTINO.bz2
$ find . -type f -exec bzip2 "{}" \;
$ cd ..
$ tar cf DEST_DIR.bz2.tar DEST_DIR.bz2
$ rm -rf DEST_DIR.bz2
```

## Implementation 

A **pool of consumer** is used, this is a set of threads(8) that perform the function of compressing the files with bzip2. The pool consumes a buffer with the files.
The **pool of producers** is responsible for producing this buffer, that is, browsing the source directory, opening the source file and creating the destination file feeding the buffer.
The final directory is compressed with tar.

## How To Use

Compile the program with:

``` bash
$ gcc -o bin -lpthread compressor.c
```

Execute the binary with:

``` bash
$ ./bin <source_directory> <destination_directory>.bz2.tar
```

[Work for Operating Systems Fundamentals discipline.](http://www.brunoribas.com.br/so/2019-2/trabalho/)
