#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <fstream>

class Host {
  std::unordered_map<int, int> lastDelivered;
  //buffer
  //networking window (map as for each?)
  //queue for packets later of the one(s) missing
  //queue for messages to be delivered (?maybe needed for concurrency)
  int id;
  std::string ip; // maybe later we will use as another type
  const char * port;
  int toBroad;
  std::unordered_map<int, sockaddr *> addresses;
  std::unordered_map<int, std::unordered_set<int>> expected;

  //urb logic data (it uses the ackMap too for avoiding duplication)
  std::unordered_map<int, std::unordered_set<int>> forwardMap;

  // networking logic data
  std::unordered_map<int, int> ackMap; //for the moment I store only the last

  // networking internals data
  struct addrinfo *servinfo;
  int sockfd;

  public:
    Host(int p_id, char * p_port, std::string ip, std::string configPath) 
      : id(p_id), port(p_port), forwardMap(), ackMap() {

      // open config file for reading how many messages to broadcast
      std::ifstream configFile;
      std::string line;
      configFile.open(configPath);
      if (configFile.is_open()) {
        while (getline(configFile,line)) {
          // In fifo it should be only one, therefore I directly assign it
          // For the next I will create a data structure
          toBroad = std::atoi(line.c_str()); 
        }
        configFile.close();
      }
    }

    void initialize_network() {
      int status;
      struct addrinfo hints;

      // first, load up address structs with getaddrinfo():

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
      hints.ai_socktype = SOCK_DGRAM; //use UDP
      hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

      // leave null as address for now -> localhost
      // if I want to specify the ip I should remove the AI_PASSIVE from hints
      // and substitute NULL with my ip
      if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
                exit(1); //exit or retry?
      }

      // make a socket:

      sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

      // bind it to the port we passed in to getaddrinfo():

      bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    }

    void free_network() {
      freeaddrinfo(servinfo);
    }

    const void * convertIntMessage(int m) {
      std::string sM = std::to_string(m);
      char const *cM = sM.c_str();
      return cM;
    }

    // perfect link component
    ssize_t sendTo(int m, const struct sockaddr *to) {
      const void * convM = convertIntMessage(m);
      //return sendto(sockfd, htonl(m), sizeof m, 0, to, sizeof *to) //htonl or htons?
      return sendto(sockfd, convM, sizeof m, 0, to, sizeof *to); //htonl or htons?
    }

    // Perfect link component
    // Send data and add the tracking to it if missing
    ssize_t sendTrack(int m, int toId) {
      struct sockaddr * to = addresses[toId];
      // write in two steps to allow only read unless missing
      std::unordered_set<int> s = expected[toId];
      if (s.count(m) == 0) { //can I do !s.find(m) ?
        s.insert(m);
        expected[toId] = s;
      }
      return sendTo(m, to);
    }

    ssize_t sendAck(int m, int toId) {
      struct sockaddr * to = addresses[toId];
      return sendTo(0 - m, to); //no problems as int_max is less than int_min
    }


    // broadcast
    // for each peer
    //   for each message (until count == configN)
};
