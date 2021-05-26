#!/bin/sh

#protoc -I=protobuf --cpp_out=src protobuf/message.proto
#mv src/message.pb.h include/message.pb.h

mkdir -p build
cd build
cmake ..
cmake --build .
