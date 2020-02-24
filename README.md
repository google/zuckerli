# Zuckerli
Compression format and data structures for large graphs.

## Cloning and compiling

``` shell
git clone https://github.com/google/zuckerli
cd zuckerli
git clone https://github.com/abseil/abseil-cpp.git
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 12
```

Note that to compile and run tests `googletest` should be installed on your
system.
