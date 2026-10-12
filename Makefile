# UDP echo client/server build rules
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic
TARGETS := udp_server udp_client
.PHONY: all clean rebuild
all: $(TARGETS)
udp_server: udp_socket_server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
udp_client: udp_client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
clean:
	rm -f $(TARGETS)
rebuild: clean all