#include <zmq.hpp>
#include <iostream>
#include "message.pb.h"

int main(int argc, char const *argv[]) {

  if (argc < 2){
    std::cout << "Wrong number of arguments" << std::endl;
    exit(-1);
  }

  int port = std::stoi(argv[1]);

  zmq::context_t context{1};

  zmq::socket_t socket{context, zmq::socket_type::server};

  std::string ip = "tcp://*:";
  ip += std::to_string(port);

  socket.bind(ip);

  for (;;){
    zmq::message_t request;

    // receive a request from client
    socket.recv(request, zmq::recv_flags::none);

    std::string s = request.to_string();
    Message::Message sampleB1;
    sampleB1.ParseFromString(s);
    Message::RequestMessage sampleB = sampleB1.req();

    std::cout << "userId: " << sampleB.userid() << " requestId: " << sampleB.requestid() << " Query: " << sampleB.query() << " From" << request.routing_id() << std::endl;

    // send the reply to the client
    //socket.send(request, zmq::send_flags::none);
  }

  return 0;
}
