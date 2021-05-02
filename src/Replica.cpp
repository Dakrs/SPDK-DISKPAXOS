#include <zmq.hpp>
#include <iostream>
#include <string>
#include <tuple>

int main(int argc, char const *argv[]) {

  auto ver = zmq::version();

  std::cout << "ZMQ_VERSION: " << std::get<0>(ver) << "." << std::get<1>(ver) << "." << std::get<2>(ver) << std::endl;


  zmq::context_t context{1};

  zmq::socket_t socket{context, zmq::socket_type::client};
  socket.connect("tcp://localhost:5555");

  while(1){
    std::string line;
    std::getline(std::cin, line);

    socket.send(zmq::buffer(line), zmq::send_flags::none);

    zmq::message_t reply{};
    socket.recv(reply, zmq::recv_flags::none);

    std::cout << "Received " << reply.to_string();
  }

  return 0;
}
