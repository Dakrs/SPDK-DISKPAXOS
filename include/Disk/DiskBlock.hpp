#ifndef DISKBLOCK_H
#define DISKBLOCK_H

#include <string>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>


class DiskBlock {

  public:
    int bal;
    int mbal;
    int slot;
    std::string input;

    DiskBlock();
    std::string serialize();
    void deserialize(std::string str);
    void copy(DiskBlock &db);
    DiskBlock * copy();
    ~DiskBlock();
    std::string toString();
    bool isValid();

    template <class Archive>
    void serialize(Archive & archive){
      archive( bal, mbal, slot, input);
    }
};

#endif
