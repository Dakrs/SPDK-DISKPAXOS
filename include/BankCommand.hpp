#include "Command.hpp"
#include <string>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>


class BankCommand: public Command {
  std::string type;
  std::vector<int> args;

  public:
    BankCommand();
    BankCommand(std::string type,std::vector<int> &args);
    std::string serialize();
    void deserialize(std::string str);
    ~BankCommand();
    std::string toString();

    template <class Archive>
    void serialize(Archive & archive){
      archive( type,args);
    }
};
