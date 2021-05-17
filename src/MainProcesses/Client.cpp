#include <zmq.hpp>
#include <iostream>
#include <string>
#include <vector>
#include "message.pb.h"

int main(int argc, char const *argv[]) {
  zmq::context_t context{1};
  zmq::socket_t socket{context, zmq::socket_type::client};


  std::vector<int> v = {5555,5556};

  std::cout << "Connecting to network" << std::endl;

  for(auto i : v){
    std::string ip = "tcp://localhost:";
    ip += std::to_string(i);
    socket.connect(ip);
  }

  std::cout << "Connected to network" << std::endl;

  while (true){
    std::string line;
    std::getline(std::cin, line);

    Message::Message sampleM;
    sampleM.set_messagetype(Message::Message::REQUEST);
    Message::RequestMessage * example1 = sampleM.mutable_req();
    example1->set_userid(2);
    example1->set_requestid(2);
    example1->set_query(line);

    std::string s;
    sampleM.SerializeToString(&s);

    auto buffer = zmq::buffer(s);

    for(auto i: v){
      socket.send(buffer, zmq::send_flags::none);
    }

    sampleM.Clear();
  }

  return 0;
}
