# Command file to set QuickFAST environment
# Preferred Linux build: CMake + C++23 (see README.md / BUILD.md).
# Runtime deps: Xerces-C++ (>= 3.2.5; CMake may FetchContent 3.3.0, or use Conan/vcpkg).
# Asio and GoogleTest: find_package, else FetchContent when QUICKFAST_FETCH_DEPS=ON.
# Customize this file by setting variables to suit your environment
SOURCE="${BASH_SOURCE[0]}"
SOURCE_DIR=`dirname $SOURCE`
export QUICKFAST_ROOT=`readlink -f $SOURCE_DIR`

if test "$MPC_ROOT" = ""
then
  if test "$ACE_ROOT" != ""
  then
    export MPC_ROOT=$ACE_ROOT/MPC
  fi
fi

# Prefer system Xerces when XERCES_ROOT is unset.
if test "$XERCES_ROOT" = ""
then
  if test -d /usr/include/xercesc
  then
    export XERCES_ROOT=/usr
  else
    # Prefer >= 3.2.5 (CVE-2024-23807); 3.3.0 is the FetchContent fallback.
    export XERCES_ROOT=~/xerces/xerces-c-3.3.0
  fi
fi

if test "$XERCES_LIBPATH" = ""
then
  if test -d /usr/lib/x86_64-linux-gnu
  then
    export XERCES_LIBPATH=/usr/lib/x86_64-linux-gnu
  else
    export XERCES_LIBPATH=$XERCES_ROOT/src/.libs
  fi
fi

if test "$XERCES_LIBNAME" = ""
then
  export XERCES_LIBNAME=xerces-c
fi

if test "$XERCES_INCLUDE" = ""
then
  if test -d /usr/include/xercesc
  then
    export XERCES_INCLUDE=/usr/include
  else
    export XERCES_INCLUDE=$XERCES_ROOT/src
  fi
fi

export PATH=$QUICKFAST_ROOT/bin:$QUICKFAST_ROOT/build/bin${MPC_ROOT:+:$MPC_ROOT}:$PATH
export LD_LIBRARY_PATH=$XERCES_LIBPATH:$QUICKFAST_ROOT/lib:$QUICKFAST_ROOT/build/lib:$LD_LIBRARY_PATH
