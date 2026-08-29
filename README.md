## QuickFAST -- An implementation of the FAST protocol for native C++ and .NET

QuickFAST is an Open Source native C++ implementation of the FAST Protocol [SM]. FAST(Fix Adapted for STreaming) protocol 
was developed by FIX Protocol Limited [FPL] (http://www.fixprotocol.org/fast/) as a way to reduce the bandwidth and network-latency 

Because FAST not specific to market data or the financial industry, there are opportunities for using FAST in a wide variety of situations.

QuickFAST is written to be portable to many platforms. It is routinely tested on Windows and Linux. The project also includes a .NET wrapper 
which supports using QuickFAST in the .NET environment. Ask if you want support for other platforms.

### Linux build (CMake + C++23)

Native library, examples, and tests no longer depend on Boost. Dependencies:

- C++23 compiler (GCC 13+ / Clang 16+)
- [Xerces-C++](https://xerces.apache.org/xerces-c/)
- Standalone [Asio](https://github.com/chriskohlhoff/asio) and [GoogleTest](https://github.com/google/googletest) / GoogleMock (fetched by CMake)

Ubuntu/Debian packages:

```bash
sudo apt-get install -y cmake build-essential libxerces-c-dev
```

Configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Optional flags: `-DQUICKFAST_BUILD_TESTS=OFF`, `-DQUICKFAST_BUILD_EXAMPLES=OFF`.

The legacy MPC/`setup.sh` flow remains available for older toolchains; prefer CMake on modern Linux.

Instructions for [getting started with QuickFAST are here](https://github.com/objectcomputing/quickfast/wiki/GettingStarted)

QuickFAST was developed by Object Computing Inc.(OCI) St. Louis Missouri USA. OCI has made QuickFAST available as open source software 
which may be used without payment of development or runtime license fees. OCI offers commercial support for QuickFAST.

For questions and discussion of QuickFAST, visit the [QuickFAST users mailing list](https://groups.google.com/forum/#!forum/quickfast_users)

###List Rules:
* Normal mailing list rules apply on the list. Discussions should be civil and on-topic. Offensive messages, off-topic chatter, and spam will not be tolerated.

* Messages from new members will be moderated due to the high volume of spam postings that are sent to this (and any) mailing list. For practical purposes this means there may be a delay before your first message to the list is published. Once you have est

* Messages on the list must be posted in English. It is acceptable to have the message in another language as well, but an English translation must appear first. For more information about this, click this link

## See also:

* For a open source Java implementation of FAST, see https://sourceforge.net/projects/openfast/
* For an open source C++ implementation of the FIX protocol, see http://www.quickfixengine.org/
* For an open source Java implementation of the FIX protocol, see http://www.quickfixj.org/
