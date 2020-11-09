#pragma once

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <string>
#include <fstream>
#include <iostream>

class HostC {
  //buffer
  //networking window (map as for each?)
  //queue for packets later of the one(s) missing
  //queue for messages to be delivered (?maybe needed for concurrency)
  long unsigned int id;
  std::string ip; // maybe later we will use as another type
  //const char * port;
  short unsigned int port;
  int toBroad; // number of messages to broadcast (coming from config)

  std::string outPath;
  std::deque<const char *> outBuffer;

  // map of id -> address of all the known hosts
  std::unordered_map<long unsigned int, sockaddr_in> addresses;
  std::unordered_map<long unsigned int, addrinfo *> addresses2;

  // map of id -> set of expected messages (messages which I send and I expect
  // an ack back)
  std::unordered_map<long unsigned int, std::unordered_set<long unsigned int>> expected;

  // map of id -> (message -> count) of expected messages (messages which 
  // I sent and I expect an ack back) for each host id.
  // Every id has a corresponding map of messages and count in order to consider
  // the receiver failed after a certain amount
  //std::unordered_map<long unsigned int, std::unordered_map<long unsigned int, int>> expected;

  //urb logic data (it uses the ackMap too for avoiding duplication)
  std::unordered_map<long unsigned int, std::unordered_set<long unsigned int>> forwardMap;

  // networking logic data
  std::unordered_map<long unsigned int, long unsigned int> ackMap; //for the moment I store only the last

  // TODO change
  //std::unordered_map<int, int> lastDelivered;

  // networking internals data
  struct addrinfo *servinfo;
  int sockfd;

  std::string errorFile;

  public:
    HostC(long unsigned int p_id, std::string configPath, std::string outputPath) 
      : id(p_id), port(), outPath(outputPath), outBuffer(), addresses(), addresses2(), expected(), forwardMap(), ackMap() {

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

      std::string idS = std::to_string(id);
      std::string errorCore = "./errors";
      errorFile = errorCore.append(idS);
    }

    void initialize_network() {
      int status;
      struct addrinfo hints;

      // first, load up address structs with getaddrinfo():

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
      hints.ai_socktype = SOCK_DGRAM; //use UDP
      hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

      port = addresses[id].sin_port;

      writeError(convertIntMessage(port));
      // leave null as address for now -> localhost
      // if I want to specify the ip I should remove the AI_PASSIVE from hints
      // and substitute NULL with my ip
      if ((status = getaddrinfo(NULL, convertIntMessage(port), &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            writeError("Error while initializing network");
            exit(1); //exit or retry?
      }

      // make a socket:

      sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

      // bind it to the port we passed in to getaddrinfo():

      bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    }

    void initialize_network2(unsigned short int myPort) {
      int status;
      struct addrinfo hints;

      // first, load up address structs with getaddrinfo():

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
      hints.ai_socktype = SOCK_DGRAM; //use UDP
      hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

      writeError(convertIntMessageS(myPort));

      // leave null as address for now -> localhost
      // if I want to specify the ip I should remove the AI_PASSIVE from hints
      // and substitute NULL with my ip
      if ((status = getaddrinfo(NULL, convertIntMessageS(myPort), &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            writeError("Error while initializing network");
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

    const char * convertIntMessage(unsigned long int m) {
      std::string sM = std::to_string(m);
      char const *cM = sM.c_str();
      return cM;
    }

    const char * convertIntMessageS(unsigned short m) {
      unsigned long newM = m;
      std::string sM = std::to_string(newM);
      char const *cM = sM.c_str();
      return cM;
    }

    // perfect link component
    ssize_t sendTo(const char * m, const struct sockaddr *to) {
      std::string mess = std::string(m);
      std::string phrase = "sendTo message: ";
      writeError(phrase.append(mess));
      //const void * convM = convertIntMessage(m);
      //const char * convM = m.c_str();

      //return sendto(sockfd, htonl(m), sizeof m, 0, to, sizeof *to) //htonl or htons?
      return sendto(sockfd, m, strlen(m), 0, to, sizeof *to); //htonl or htons?
    }

    ssize_t sendTo2(const char * m, const struct addrinfo *to) {
      std::string mess = std::string(m);
      std::string phrase = "sendTo message: ";
      writeError(phrase.append(mess));
      //const void * convM = convertIntMessage(m);
      //const char * convM = m.c_str();

      //return sendto(sockfd, htonl(m), sizeof m, 0, to, sizeof *to) //htonl or htons?
      return sendto(sockfd, m, strlen(m), 0, to->ai_addr, to->ai_addrlen); //htonl or htons?
    }

    // testSend is a dummy method for debugging purposes
    void testSend(){
      writeError("S: testSend");
      ssize_t s;
      std::string ss;
      for (auto peer : addresses) {
        if (peer.first != id) {
          s = sendTrack2(peer.first, 1);
          ss = std::to_string(s);
          writeError("S: sent " + ss + "bytes");
        }
      }
    }

    // Perfect link component
    // Send data and add the tracking to it if missing
    ssize_t sendTrack(unsigned long int m, unsigned long int toId) {
      writeError("S: starting sendTrack");

      struct sockaddr * to = reinterpret_cast<sockaddr *>(&(addresses[toId]));
      //unsigned long int mID = extractMessId(m);
      // write in two steps to allow only read unless missing
      std::unordered_set<unsigned long int> s = expected[toId];
      if (s.count(m) == 0) { //can I do !s.find(m) ?
        s.insert(m);
        expected[toId] = s;
      }
      const char * charM = convertIntMessage(m);
      return sendTo(charM, to);
    }

    // Perfect link component
    // Send data and add the tracking to it if missing
    ssize_t sendTrack2(unsigned long int m, unsigned long int toId) {
      writeError("S: starting sendTrack");

      struct addrinfo * to = addresses2[toId];

      //unsigned long int mID = extractMessId(m);
      // write in two steps to allow only read unless missing
      std::unordered_set<unsigned long int> s = expected[toId];
      if (s.count(m) == 0) { //can I do !s.find(m) ?
        s.insert(m);
        expected[toId] = s;
      }
      const char * charM = convertIntMessage(m);
      return sendTo2(charM, to);
    }

    // TODO
    void checker() {
      while (true) {
        // TODO change the value afterwards
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }

    // TODO
    unsigned long int extractMessId(std::string m) {
      return 0;
    }


    ssize_t sendAck(unsigned long int m, unsigned long int toId) {
      struct sockaddr * to = reinterpret_cast<sockaddr *>(&(addresses[toId]));
      std::string charMid = std::to_string(m);
      std::string from = std::to_string(id);
      std::string Smess = from + ",1," + charMid;
      const char * mess = Smess.c_str();
      writeError(Smess);
      return sendTo(mess, to);
    }

    // addHost add an host information to the map addresses
    void addHost(long unsigned int hId, unsigned short int hPort, in_addr_t hIp) {
      struct in_addr hAddr;
      hAddr.s_addr = hIp;
      struct sockaddr_in hSocket;
      hSocket.sin_family = AF_INET; //only ipv4 for the moment
      hSocket.sin_port = hPort;
      hSocket.sin_addr = hAddr;

      addresses[hId] = hSocket; // is it correct?
      //addresses.insert({hId, hSocket});
    }

    void addHost2(long unsigned int hId, unsigned short int hPort, std::string hIp) {
      int status;
      //struct addrinfo hints, *si;
      struct addrinfo hints;
      struct addrinfo * si = new addrinfo;

      // first, load up address structs with getaddrinfo():

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
      hints.ai_socktype = SOCK_DGRAM; //use UDP

      // leave null as address for now -> localhost
      // if I want to specify the ip I should remove the AI_PASSIVE from hints
      // and substitute NULL with my ip
      if ((status = getaddrinfo(hIp.c_str(), convertIntMessageS(hPort), &hints, &si)) != 0) {
        writeError("Could not create addrinfo for host");
        return;
      }

      addresses2[hId] = si; // will it live?
    }

    // listen will listen for incoming connections
    void handleMessages() {
      writeError("S: Start handling");
      struct sockaddr_storage their_addr;
      socklen_t addr_size;

      /*
      listen(sockfd, 10); // set it to 10 for now
      addr_size = sizeof their_addr;
      accept_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
      */

      // singlethreaded for now
      addr_size = sizeof their_addr;
      long int bytesRcv;
      char buffer[20];
      bytesRcv = recvfrom(sockfd, buffer, 20, 0, reinterpret_cast<struct sockaddr *>(&their_addr), &addr_size);
      parseMessage(buffer, bytesRcv, &their_addr);
    }

    void parseMessage(const char * buffer, const long int bytesRcv, const struct sockaddr_storage * from) {
      outBuffer.push_back(buffer);
      writeError("S:parsed message");

      // activate response mechanism

      // if ack: remove from the expected map and send fin
      // if new message: send ack
    }

    void flushBuffer() {
      std::ofstream outputFile(outPath);
      //outputFile.open(outPath);
      if (outputFile.is_open()) {
        outputFile << "Test" << std::endl;
        while (!outBuffer.empty()) {
          outputFile << "m:" << outBuffer.front() << std::endl;
          outBuffer.pop_front();
        }
        outputFile.close();
      }
    }

    void writeError(std::string errorM) {
      std::ofstream outputFile;
      outputFile.open(errorFile, std::ios::out | std::ios::app);
      if (outputFile.is_open()) {
        outputFile << errorM << std::endl;
        outputFile.close();
      }
    }
};
