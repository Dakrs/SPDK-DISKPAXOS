#ifndef COMMAND_H
#define COMMAND_H

#include <tuple>

typedef unsigned char BYTE;

class Command {
  public:
    virtual ~Command() = 0;
    virtual std::string serialize() = 0;
    virtual void deserialize(std::string str) = 0;
};

#endif
