# maxc
[![](http://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)
[![Build Status](https://travis-ci.com/k-mrm/maxc.svg?branch=master)](https://travis-ci.com/k-mrm/maxc)

maxc is a script language implemented in C.

- static typing
- comfortable coding

## Sample Code

hello.mxc
```
println("Hello, World!");
```
```
Hello, World!
```


fibo.mxc
```
fn fibo(n: int): int {
    if n <= 1 {
        return n;
    }

    return fibo(n - 2) + fibo(n - 1);
}

println(fibo(30));
30.fibo().println();
30.fibo.println;
```
```
832040
832040
832040
```

## repl

```
$ ./maxc
Welcome to maxc repl mode!
maxc Version 0.0.1
use exit(int) or Ctrl-D to exit
>> let a = 10
>> a
10 : int
>> import math
>> let n = -500
>> n.math@abs
500 : int
>> [10; 0]
[0,0,0,0,0,0,0,0,0,0] : [int]
>> 

```

## How To Build
### on Linux

1. Install gcc and make

2. get maxc from github
```
$ git clone https://github.com/k-mrm/maxc.git
$ cd maxc
```

3. Build
```
$ make
$ ./maxc                    # run repl
$ ./maxc example/hello.mxc  # run from source file
```

## Document(Japanese)
https://admarimoin.hatenablog.com/entry/2019/08/28/155346

