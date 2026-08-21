#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_fd == -1) {
    std::cerr << "Socket creation has failed due to " << std::strerror(errno)
              << "\n";
    return 1;
  }
  int opt = -1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(3000);

  if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) == -1) {
    std::cerr << "bind() failed due to " << std::strerror(errno) << "\n";
    return 1;
  }
  if (listen(server_fd, 500) == -1) {
    std::cerr << "listening on port 3000 failed due to " << std::strerror(errno)
              << "\n";
    return 1;
  }

  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);
  int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
  if (client_fd == -1) {
    std::cerr << "Accepting client request failed: " << std::strerror(errno)
              << "\n";
    return 1;
  }
  std::cout << "Client connected!\n";
  close(client_fd);
  close(server_fd);
}
