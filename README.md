## Overview

[![Build Status](https://img.shields.io/travis/redjack/varon-t/develop.svg)](https://travis-ci.org/redjack/varon-t)

The Varon-T library is a C implementation of the
[disruptor queue](http://lmax-exchange.github.io/disruptor/), a high performance
messaging library based on an efficient FIFO circular queue implementation.

API documentation can be found [here](http://varon-t.readthedocs.org/).


## Build instructions

To build Varon-T, you need the following libraries installed on your system:

  * pkg-config
  * [libcork](https://github.com/redjack/libcork)
  * [check](http://check.sourceforge.net)
  * [clogger](https://github.com/redjack/clogger)

If you want to build the documentation, you also need:

  * [Sphinx](http://sphinx.pocoo.org/)

The Varon-T library uses [CMake](http://www.cmake.org) as its build manager.
In most cases, you should be able to build the library from source code using
the following commands from the top level of your copy of the source tree:

    $ mkdir .build
    $ cd .build
    $ cmake .. \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=$PREFIX
    $ make
    $ make test
    $ make install

There are two tests. The first is a functional test and the second is a
performance test. The latter may take a couple of minutes to complete.

You might have to run the last command using sudo, if you need administrative
privileges to write to the `$PREFIX` directory.


## License

Copyright &copy; 2012, RedJack, LLC.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.

* Neither the name of RedJack Software, LLC nor the names of its
  contributors may be used to endorse or promote products derived
  from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
