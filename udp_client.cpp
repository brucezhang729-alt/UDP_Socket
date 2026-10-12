#include<arpa/inet.h>
#include<cerrno>
#include<cstddef>
#include<cstdint>
#include<cstdlib>
#include<cstring>
#include<iostream>
#include<stdexcept>
#include<string>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>

namespace{
    constexpr std::size_t kBufferSize = 65535;

    void Die(const std::string& message)
    {
        std::cerr<<message<<":"<<std::strerror(errno)<<'\n';
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc,char*argv[])
{
    if(argc!=3)
    {
        std::cerr<<"Usage:"<<argv[0]<<"server_ip server_port\n";
        return EXIT_FAILURE;
    }
    uint16_t server_port;
    try{
        const unsigned long parsed = std::stoul(argv[2]);
        if(parsed>65535)  throw std::out_of_range("port");
        server_port  = static_cast<uint16_t>(parsed);
    }catch(const std::exception&)
    {
        std::cerr<<"invalid server port\n";
        return EXIT_FAILURE;
    }
    const int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd<0) Die("socket");

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    if(inet_pton(AF_INET,argv[1],&server.sin_addr)!=1)
    {
        std::cerr<<"invalid IPv4 address\n";
        close(sockfd);
        return EXIT_FAILURE;
    }

    // 没有bind():第一次sento时，内核会自动分配客户端临时端口
    std::string message;
    while(std::cout<<"Enter message(q to quit):"&&std::getline(std::cin,message))
    {
        if(message=="q")    break;
        const ssize_t sent = sendto(sockfd,message.data(),message.size(),0,
        reinterpret_cast<sockaddr*>(&server),sizeof(server));

        if(sent<0)
        {
            std::cerr<<"sendto:"<<std::strerror(errno)<<'\n';
            continue;
        }
        if(static_cast<std::size_t>(sent)!=message.size())
        {
            std::cerr<<"sendto sent an unexpected length\n";
            continue;
        }
        char buffer[kBufferSize];
        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);
        const ssize_t received = recvfrom(sockfd,buffer,sizeof(buffer),0,
                                            reinterpret_cast<sockaddr*>(&peer),&peer_len);
          if(received<0)
          {
            std::cerr<<"recvfrom"<<std::strerror(errno)<<'\n';
            continue;
          }
          std::cout<<"server echo"<<std::string(buffer,static_cast<std::size_t>(received))<<'\n';
    }
    close(sockfd);
    return 0; 
}

