#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr uint16_t kDefaultPort = 8888;
// IPv4 UDP 最大有效载荷是 65507 字节；使用更大的缓冲区避免普通报文被截断。
constexpr std::size_t kBufferSize = 65535;

void Die(const std::string& message) {
    std::cerr << message << ": " << std::strerror(errno) << '\n';
    std::exit(EXIT_FAILURE);
}

std::string PeerToString(const sockaddr_in& peer) {
    char ip[INET_ADDRSTRLEN]{};
    if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip)) == nullptr) {
        return "<invalid-address>";
    }
    return std::string(ip) + ":" + std::to_string(ntohs(peer.sin_port));
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 1 && argc != 2) {
        std::cerr << "Usage: " << argv[0] << " [port]\n";
        return EXIT_FAILURE;
    }

    uint16_t port = kDefaultPort;
    if (argc == 2) {
        try {
            const unsigned long parsed = std::stoul(argv[1]);
            if (parsed > 65535) throw std::out_of_range("port");
            port = static_cast<uint16_t>(parsed);
        } catch (const std::exception&) {
            std::cerr << "invalid port\n";
            return EXIT_FAILURE;
        }
    }

    // SOCK_DGRAM 表示 UDP 数据报 socket；返回值是可用于系统调用的文件描述符。
    const int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) Die("socket");

    // 允许程序重启时较快重新绑定端口。
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(sockfd);
        Die("setsockopt");
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);              // 主机字节序 -> 网络字节序
    local.sin_addr.s_addr = htonl(INADDR_ANY); // 接收本机所有 IPv4 网卡的数据

    // UDP 服务器必须绑定固定的本地端口，客户端才能找到它。
    if (bind(sockfd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        close(sockfd);
        Die("bind");
    }

    std::cout << "UDP server listening on 0.0.0.0:" << port << '\n';

    for (;;) {
        char buffer[kBufferSize];
        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);

        // UDP 一次 recvfrom 对应一个数据报；n 是本次数据报的实际字节数。
        const ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                   reinterpret_cast<sockaddr*>(&peer), &peer_len);
        if (n < 0) {
            if (errno == EINTR) continue; // 被信号打断，重新接收
            std::cerr << "recvfrom: " << std::strerror(errno) << '\n';
            continue;
        }

        const std::string message(buffer, static_cast<std::size_t>(n));
        std::cout << PeerToString(peer) << " says: " << message << '\n';

        // UDP 要把目标地址一起交给 sendto；这里把数据报回送给原发送者。
        const ssize_t sent = sendto(sockfd, message.data(), message.size(), 0,
                                    reinterpret_cast<sockaddr*>(&peer), peer_len);
        if (sent < 0) {
            std::cerr << "sendto: " << std::strerror(errno) << '\n';
        } else if (sent != n) {
            // UDP 要么发送完整数据报，要么失败；部分发送通常意味着异常。
            std::cerr << "sendto sent an unexpected length\n";
        }
    }
}
