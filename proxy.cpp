#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>

#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

int main(){
    try{
        net::io_context   ioc;
        tcp::acceptor acceptor(
        ioc,
        tcp::endpoint(tcp::v4(), 3000)
        );
        std::cout<<"Server Listening on http://localhost:3000\n";
        while(true){
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            beast::flat_buffer buffer;
            http::request<http::string_body> request;
            http::read(socket, buffer, request);
            std::cout<<request.method_string()<<" "<< request.target() << "\n";
            http::response<http::string_body> response{
                http::status::ok,
                request.version()
            };
            response.set(http::field::content_type,"text/plain");
            response.body() = "Hello from boost!\n";
            response.prepare_payload();
            http::write(socket,response);
            beast::error_code ec;
            socket.shutdown(tcp::socket::shutdown_send, ec);
        }
    } catch(const std::exception & e){
        std::cerr<<"Error: "<< e.what() <<"\n";
    }
}
