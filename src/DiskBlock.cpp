#include "Disk/DiskBlock.hpp"
#include <sstream>
#include <cereal/archives/binary.hpp>


DiskBlock::DiskBlock(){
  this->bal = 0;
  this->slot = 0;
  this->mbal = 0;
  this->input = "";
}

void DiskBlock::copy(DiskBlock & db){
  db.bal = this->bal;
  db.slot = this->slot;
  db.mbal = this->mbal;
  db.input = this->input;
}

std::string DiskBlock::serialize(){

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive oarchive(ss);
    DiskBlock db = DiskBlock();
    this->copy(db);
    oarchive(db);
  }

  return ss.str();
}

void DiskBlock::deserialize(std::string str){
  std::stringstream ss;
  ss.str(str);

  DiskBlock db;

  {
    cereal::BinaryInputArchive iarchive(ss);
    iarchive(db);
  }

  this->bal = db.bal;
  this->slot = db.slot;
  this->mbal = db.mbal;
  this->input = db.input;
}

DiskBlock::~DiskBlock(){}

std::string DiskBlock::toString(){
  std::string res = "";

  res.append("bal: " + std::to_string(bal) + " ");
  res.append("mbal: " + std::to_string(mbal) + " ");
  res.append("slot: " + std::to_string(slot) + "\n");
  res.append("inp: " + input);

  return res;
}
