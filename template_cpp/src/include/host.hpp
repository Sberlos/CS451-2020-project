#pragma once

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <string>
#include <fstream>
#include <iostream>
#include <shared_mutex>
#include <mutex>

/*
// from StackOverflow question [link] //TODO
typedef std::shared_mutex Lock;
typedef std::unique_lock<Lock> WriteLock;
typedef std::shared_lock<Lock> ReadLock;
*/

class HostC {
  //buffer
  //networking window (map as for each?)
  //queue for packets later of the one(s) missing
  //queue for messages to be delivered (?maybe needed for concurrency)
  unsigned long id;
  std::string ip; // maybe later we will use as another type
  //const char * port;
  short unsigned int port;
  unsigned long toBroad; // number of messages to broadcast (coming from config)

  std::string outPath;
  //std::deque<const char *> outBuffer;
  std::deque<std::string> outBuffer;

  // map of id -> address of all the known hosts
  std::unordered_map<unsigned long int, sockaddr_in> addresses;
  std::unordered_map<unsigned long int, addrinfo *> addresses2;

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

  //Lock myLock;
  mutable std::shared_mutex addessesLock;
  mutable std::shared_mutex outBufferLock;
  mutable std::shared_mutex expectedLock;
  mutable std::shared_mutex forwardLock;
  mutable std::shared_mutex ackLock;

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

    ssize_t sendTo2(const char * m, const struct addrinfo *to) {
      std::string mess = std::string(m);
      std::string phrase = "sendTo message: ";
      writeError(phrase.append(mess));
      //const void * convM = convertIntMessage(m);
      //const char * convM = m.c_str();

      //return sendto(sockfd, htonl(m), sizeof m, 0, to, sizeof *to) //htonl or htons?
      return sendto(sockfd, m, strlen(m), 0, to->ai_addr, to->ai_addrlen); //htonl or htons?
    }

    void bebBroadcast(const unsigned long message, const unsigned long fromId){
      for (auto peer : addresses2) {
          if (peer.first != id) {
              writeError(std::string("peer id:").append(std::to_string(peer.first)));
              sendTrack2(message, peer.first, fromId);
          }
      }
    }

    void startBroadcasting() {
      /*
      std::shared_lock lock(forwardLock);
      std::unordered_map<unsigned long, unsigned long> s = forwardMap[id];
      std::shared_lock unlock(forwardLock);
      s.insert(
      */
      for (unsigned long i = 1; i <= toBroad; i++) { // could cause problems the i++ with max int?
        bebBroadcast(i, id);
      }
    }
    // testSend2 before
    void testSend2(){
      writeError("S: bebBroadcast");
      ssize_t s;
      std::string ss;
      //for (auto peer : addresses) {
      for (int i = 0; i < 2; i++) {
          writeError("S: sending:"+std::to_string(i));
          for (auto peer : addresses2) {
              if (peer.first != id) {
                  s = sendTrack2(i, peer.first, id);
                  ss = std::to_string(s);
                  writeError("S: sent " + ss + "bytes");
              }
          }
      }
    }

    // Perfect link component
    // Send data and add the tracking to it if missing
    ssize_t sendTrack2(const unsigned long m, const unsigned long toId, const unsigned long fromId) {
      writeError("S: starting sendTrack");

      struct addrinfo * to = addresses2[toId];

      //unsigned long int mID = extractMessId(m);
      // write in two steps to allow only read unless missing
      std::unordered_set<unsigned long> s = expected[toId];
      if (s.count(m) == 0) { //can I do !s.find(m) ?
        s.insert(m);
        expected[toId] = s;
      }
      //const char * charM = convertIntMessage(m);
      std::string sM = std::to_string(fromId).append(",0,").append(std::to_string(m));
      const char * charM = sM.c_str();
      return sendTo2(charM, to);
    }

    // TODO
    // should also be responsible for writing to buffer when we have the guarantee that
    // all correct processes have delivered (check the exact condition)
    void checker() {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      while (true) {
        // TODO change the value afterwards
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }

    // TODO
    unsigned long int extractMessId(std::string m) {
      return 0;
    }

    ssize_t sendAck2(unsigned long int m, unsigned long int toId) {
      struct addrinfo * to = addresses2[toId];

      std::string charMid = std::to_string(m);
      std::string from = std::to_string(id);
      std::string Smess = from.append(",1,").append(charMid);
      const char * mess = Smess.c_str();

      writeError(Smess);

      return sendTo2(mess, to);
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
      while (true) {
          addr_size = sizeof their_addr;
          long int bytesRcv;
          char buffer[20];
          bytesRcv = recvfrom(sockfd, buffer, 20, 0, reinterpret_cast<struct sockaddr *>(&their_addr), &addr_size);
          parseMessage(buffer, bytesRcv, &their_addr);
          writeError("buff size after parse in handle:" + std::to_string(outBuffer.size()));
          writeError("front:" + std::string(outBuffer.front()));
      }
    }

    void parseMessage(const char * buffer, const long bytesRcv, const struct sockaddr_storage * from) {
        // TODO fix (now it pushes the buffer)
      outBuffer.push_back(std::string(buffer));
      writeError("S:parsed message = " + std::string(buffer));
      writeError("buff size after parse in parse:" + std::to_string(outBuffer.size()));
      //flushBuffer2();

      // activate response mechanism

      // if ack: remove from the expected map and send fin
      // if new message: send ack
      
      unsigned long fromId;
      unsigned long message;
      int ack;
      int parsedN;
      if (std::sscanf(buffer,"%lu,%d,%lu", &fromId, &ack, &message) == 3) {
          if (ack) {
            /*
            // lock for reading
            std::shared_lock lock(expectedLock);
            std::unordered_set<long unsigned int> s = expected[fromId];
            std::shared_lock unlock(expectedLock); //should I do it?
            //realease reading
            s.erase(message);
            //take write
            std::unique_lock lock(expectedLock);
            expected[fromId] = s;
            //release write
            std::unique_lock unlock(expectedLock);
            */
            std::unique_lock lock(expectedLock);
            long unsigned b = expected[fromId].count(message);
            expected[fromId].erase(message); // does it work this way? test says yes
            long unsigned a = expected[fromId].count(message);
            //expectedLock.unlock();
            if (a == b) {
                writeError("failed to erase");
            }
            //release write
          } else if (fromId == id) { // I was the original sender
            /*
            std::shared_lock lock(forwardLock);
            if 
            */
            sendAckMine(message, from);
              // do urb stuff
          } else { // not an ack and not one of my messages
            sendAck2(message, fromId);
              // this is related to urb
              // add to ack map
            std::shared_lock lock(forwardLock);
            std::unordered_set<long unsigned int> s = forwardMap[fromId];
            forwardLock.unlock();
            if (!(s.count(message) > 0)) {
                s.insert(message);
                //std::unique_lock lock(forwardLock);
                forwardLock.lock();
                forwardMap[fromId] = s;
                //forwardLock.unlock();
                bebBroadcast(message, fromId);
            }
          }
      }
    }

    void flushBuffer2() {
      std::ofstream outputFile(outPath.append("-internal"));
      if (outputFile.is_open()) {
        while (!outBuffer.empty()) {
          outputFile << "m:" << outBuffer.front() << std::endl;
          outBuffer.pop_front();
        }
        outputFile.close();
      }
    }

    void flushBuffer() {
      std::ofstream outputFile(outPath);
      //outputFile.open(outPath);
      if (outputFile.is_open()) {
        //outputFile << "TestN" << std::endl;
        outputFile << "buffer size: " << std::to_string(outBuffer.size()) << std::endl;
        //outputFile << "id: " << std::to_string(id) << std::endl;
        //outputFile << "size addresses: " << std::to_string(addresses2.size()) << std::endl;
        while (!outBuffer.empty()) {
          /*
          writeError("I'm flushing");
          outputFile << "m:" << outBuffer.front() << std::endl;
          outBuffer.pop_front();
          */
          outputFile << "iteration" << std::endl;
          for (unsigned i = 0; i < outBuffer.size(); i++) {
            outputFile << "at" << i << ":" << outBuffer.at(i) << std::endl;
          }
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

    ssize_t sendAckMine(const unsigned long m, const sockaddr_storage * toAddr) {
      const struct sockaddr * to = reinterpret_cast<const sockaddr *>(toAddr);
      std::string charMid = std::to_string(m);
      std::string from = std::to_string(id);
      std::string Smess = from.append(",1,").append(charMid);
      const char * mess = Smess.c_str();
      writeError(Smess);
      return sendTo(mess, to);
    }

    // perfect link component
    ssize_t sendTo(const char * m, const struct sockaddr *to) {
      std::string mess = std::string(m);
      std::string phrase = "sendTo message: ";
      writeError(phrase.append(mess));
      return sendto(sockfd, m, strlen(m), 0, to, sizeof *to); //htonl or htons?
    }

    /* code not used anymore - to delete
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

    // testSend is a dummy method for debugging purposes
    void testSend(){
      writeError("S: testSend");
      ssize_t s;
      std::string ss;
      //for (auto peer : addresses) {
      for (auto peer : addresses2) {
        if (peer.first != id) {
          s = sendTrack2(peer.first, 1, id); // this is the old one wrong, not used anymore
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

    ssize_t sendAck(unsigned long int m, unsigned long int toId) {
      struct sockaddr * to = reinterpret_cast<sockaddr *>(&(addresses[toId]));
      std::string charMid = std::to_string(m);
      std::string from = std::to_string(id);
      std::string Smess = from + ",1," + charMid;
      const char * mess = Smess.c_str();
      writeError(Smess);
      return sendTo(mess, to);
    }
    */

};
