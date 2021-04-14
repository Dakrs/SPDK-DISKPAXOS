#include "BankCommand.hpp"
#include <algorithm>
#include <sstream>
#include <cereal/archives/binary.hpp>
#include <iterator>

BankCommand::BankCommand(){
  this->type = "";
}

BankCommand::BankCommand(std::string type,std::vector<int> &args){
  this->type = type;
  std::copy(args.begin(), args.end(), back_inserter(this->args));
}

BankCommand::~BankCommand(){
  this->args.clear();
}

std::string BankCommand::toString(){
  std::string res = this->type;

  res.append("\n");
  for(auto i : this->args){
    res.append(std::to_string(i) + " - ");
  }

  return res;
}

std::string BankCommand::serialize(){

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive oarchive(ss);
    BankCommand bs = BankCommand(this->type,this->args);
    oarchive(bs);
  }

  return ss.str();
}

void BankCommand::deserialize(std::string str){
  std::stringstream ss;
  ss.str(str);

  BankCommand bs;
  {
    cereal::BinaryInputArchive iarchive(ss);
    iarchive(bs);
  }
  type = bs.type;
  std::copy(bs.args.begin(), bs.args.end(), back_inserter(args));
}
