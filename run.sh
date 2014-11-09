#!/bin/bash
source set_env
g++ FHLD.cpp $HELIB/src/fhe.a -I$HELIB/src -o FHLD -L/usr/local/lib -lntl
[ $? -eq 0 ] ||  exit
./FHLD
