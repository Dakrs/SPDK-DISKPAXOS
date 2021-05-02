#include <zmq.hpp>
#include <iostream>

int main(int argc, char const *argv[]) {


  zmq::context_t context{1};

  zmq::socket_t socket{context, zmq::socket_type::server};
  socket.bind("tcp://*:5555");

  for (;;){
    zmq::message_t request;

    // receive a request from client
    socket.recv(request, zmq::recv_flags::none);
    std::cout << "Received " << request.to_string() << " From" << request.routing_id() << std::endl;

    // send the reply to the client
    socket.send(request, zmq::send_flags::none);
  }

  return 0;
}
