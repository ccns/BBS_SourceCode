#!/bin/sh
if [ $OSTYPE = "FreeBSD" ]; then
ostype="freebsd"
else if [ $OSTYPE = "linux-gnu" ]; then
ostype="linux"
else if [ $OSTYPE = "Linux" ]; then
ostype="linux"
    fi
  fi
fi
echo $ostype

make $ostype install
