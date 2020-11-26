#pragma once

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <deque>
#include <string>
#include <fstream>
#include <iostream>
#include <shared_mutex>
#include <mutex>

/* PERFORMANCE considerations: 
 * map -> vectors (as we use numbers without gaps as indices, stupid to hash...), the only problem
 * is that we waste space with some cells when a process is considered failed (but this is not the
 * case when performance is at test, therefore it's a viable strategy).
 * if we keep maps we have to correct their size at the beginning based on the number of elements in
 * addresses otherwise the structure will rehash to dinamically resize itself losing a lot of time.
 */

class HostC {
  unsigned long id;
  std::string ip; // maybe later we will use as another type
  unsigned short port;
  unsigned long toBroad; // number of messages to broadcast (coming from config)

  std::string outPath;
  std::deque<std::string> outBuffer;

  // map of id -> address of all the known hosts
  std::unordered_map<unsigned long, sockaddr_in> addresses;
  std::unordered_map<unsigned long, addrinfo *> addresses2;

  // map of id -> (message -> count) of expected messages (messages which 
  // I sent and I expect an ack back) for each host id.
  // Every id has a corresponding map of messages and count in order to consider
  // the receiver failed after a certain amount
  //std::unordered_map<long unsigned, std::unordered_map<long unsigned, unsigned>> expected;
  // message to toId, from fromId, message m and count c. Set the counter:
  // expected[toId][fromId][m] = c
  //std::vector<std::vector<std::vector<unsigned>>> expected;
  // with a map it would be
  std::unordered_map<long unsigned, std::unordered_map<long unsigned, std::unordered_map<long unsigned, unsigned>>> expected;

  // This is the maximum value of the number of retransmissions for a single message,
  // after that number we consider the process failed
  unsigned expectedTreshold = 5;

  //urb logic data (it uses the ackMap too for avoiding duplication)
  std::unordered_map<long unsigned, std::unordered_set<long unsigned>> forwardMap;

  // networking logic data -> urb in reality with the second version(message -> set of processes)
  std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_set<unsigned long>>> ackMap;

  //process -> messages delivered
  std::unordered_map<unsigned long, std::set<unsigned long>> delivered;

  std::unordered_map<unsigned long, std::unordered_set<unsigned long>> past;

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
  mutable std::shared_mutex deliveredLock;
  mutable std::shared_mutex pastLock;

  public:
    HostC(long unsigned int p_id, std::string configPath, std::string outputPath) 
      : id(p_id), port(), outPath(outputPath), outBuffer(), addresses(), addresses2(), expected(), forwardMap(), ackMap(), delivered(), past() {

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

    void initialize_network2(unsigned short myPort) {
      int status;
      struct addrinfo hints;

      // first, load up address structs with getaddrinfo():

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
      hints.ai_socktype = SOCK_DGRAM; //use UDP
      hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

      //writeError(convertIntMessageS(myPort));
      int mP = myPort;
      //writeError(std::to_string(mP));

      if ((status = getaddrinfo(NULL, convertIntMessageS(myPort), &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            //writeError("Error while initializing network");
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

    const char * convertIntMessage(unsigned long m) {
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
      //writeError(phrase.append(mess));
      //const void * convM = convertIntMessage(m);
      //const char * convM = m.c_str();

      //return sendto(sockfd, htonl(m), sizeof m, 0, to, sizeof *to) //htonl or htons?
      return sendto(sockfd, m, strlen(m), 0, to->ai_addr, to->ai_addrlen); //htonl or htons?
    }

    void bebBroadcast(const unsigned long message, const unsigned long fromId){
      for (auto peer : addresses2) {
          if (peer.first != id) {
              //writeError(std::string("peer id:").append(std::to_string(peer.first)));
              sendTrack2(message, peer.first, fromId);
          }
      }
      // add the message that I just broadcasted to the past of myself
      std::unique_lock LockP(pastLock);
      past[id].insert(message);
    }

    void startBroadcasting() {
      for (unsigned long i = 1; i <= toBroad; i++) { // could cause problems the i++ with max int?
        bebBroadcast(i, id);
      }
    }

    // Perfect link component
    // Send data and add the tracking to it if missing
    ssize_t sendTrack2(const unsigned long m, const unsigned long toId, const unsigned long fromId) {
      //writeError("S: starting sendTrack");

      //std::unordered_map<long unsigned int, int> mE = expected[toId];
      std::unique_lock lock(expectedLock);
      unsigned vE = expected[toId][fromId][m];
      expected[toId][fromId][m] = ++vE;
      //expectedLock.unlock();
      if (vE > expectedTreshold) {
          // remove process from correct -> addresses list
          std::unique_lock lockA(addessesLock);
          addresses2.erase(toId);
          return 0;
      }

      std::shared_lock lockA(addessesLock);
      struct addrinfo * to = addresses2[toId];
      //const char * charM = convertIntMessage(m);
      //std::string sM = std::to_string(fromId).append(",0,").append(std::to_string(m));
      const std::string fromIdS = std::to_string(fromId);
      std::string sM = std::to_string(id).append(",").append(fromIdS).append(",0,").append(std::to_string(m));
      // append past
      sM = appendPast(sM, fromId);
      const char * charM = sM.c_str();
      return sendTo2(charM, to);
    }

    std::string appendPast(std::string mS, unsigned long pId) {
      std::shared_lock pSLock(pastLock);
      for (unsigned long m : past[pId]) {
        mS.append(";").append(std::to_string(m));
      }

      return mS;
    }

    // TODO
    // should also be responsible for writing to buffer when we have the guarantee that
    // all correct processes have delivered (check the exact condition)
    void checker() {
      while (true) {
        // TODO change the value afterwards
        std::this_thread::sleep_for(std::chrono::seconds(1));

        for (auto id_MapFrom : expected) {
          for (auto from_m : id_MapFrom.second) {
            for (auto m_count : from_m.second) {
              // this is done in sendTrack2
              /*
              if (m_count.second > expectedTreshold) {
                  // remove from addresses
              } else {
                sendTrack2(m_count.first, id_MapFrom.first, from_m.first);
              }
              */
              sendTrack2(m_count.first, id_MapFrom.first, from_m.first);
            }
          }
        }

        // check if in the meantime some process failes therefore we have to check if we
        // previously completed the uniform messaging procedure, if yes deliver
        // TODO I have to remove the completed messages from the ackMap otherwise too much work 
        std::shared_lock lockAddr(addessesLock);
        unsigned long addrSize = addresses2.size();

        std::shared_lock lockAck(ackLock);
        for (auto i : ackMap) {
          for (auto m : i.second) { // m = pair (message -> set)
            //for (auto p : m.second) {

              // all the addresses have rebroadcasted my message
              if (m.second.size() == addrSize) {
                std::unique_lock lockP(pastLock);
                past[i.first].erase(m.first);
                urbDeliver(m.first, i.first, std::string(""));
              }
            //}
          }
        }
      }
    }

    ssize_t sendAck2(unsigned long m, unsigned long toId, unsigned long originalS) {
      struct addrinfo * to = addresses2[toId];

      std::string charMid = std::to_string(m);
      std::string from = std::to_string(id);
      std::string strOriginal = std::to_string(originalS);
      //std::string Smess = from.append(",1,").append(charMid);
      std::string Smess = from.append(",").append(strOriginal).append(",1,").append(charMid); //TODO implement new way
      const char * mess = Smess.c_str();

      //writeError(Smess);

      return sendTo2(mess, to);
    }

    void addHost2(unsigned long hId, unsigned short hPort, std::string hIp) {
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
      status = getaddrinfo(hIp.c_str(), convertIntMessageS(hPort), &hints, &si);
      if (status != 0) {
        //writeError("Could not create addrinfo for host");
        return;
      }

      addresses2[hId] = si;
    }

    // listen will listen for incoming connections
    void handleMessages() {
      //writeError("S: Start handling");
      struct sockaddr_storage their_addr;
      socklen_t addr_size;

      // singlethreaded for now
      while (true) {
          addr_size = sizeof their_addr;
          long int bytesRcv;
          char buffer[20];
          bytesRcv = recvfrom(sockfd, buffer, 20, 0, reinterpret_cast<struct sockaddr *>(&their_addr), &addr_size);
          parseMessage(buffer, bytesRcv, &their_addr);
          //writeError("buff size after parse in handle:" + std::to_string(outBuffer.size()));
          // I do explicitly to see if avoid some reports from valgrind
          /*
          unsigned long lenOut = outBuffer.size();
          std::string left = "buff size after parse in handle:";
          std::string right = std::to_string(lenOut);
          std::string sum = left.append(right);
          writeError(sum);

          if (outBuffer.size() > 0) {
            std::string left2 = "front:";
            std::string right2 = outBuffer.front();
            std::string sum2 = left2.append(right2);
            writeError(sum2);
          }
          //writeError("front:" + std::string(outBuffer.front()));
          */
      }
    }

    void parseMessage(const char * buffer, const long bytesRcv, const struct sockaddr_storage * from) {
      /*
      writeError("S:parsed message = " + std::string(buffer));
      writeError("buff size after parse in parse:" + std::to_string(outBuffer.size()));
      */
      
      unsigned long senderId;
      unsigned long fromId;
      unsigned long message;
      int ack;
      int parsedN;
      if (std::sscanf(buffer,"%lu,%lu,%d,%lu", &senderId, &fromId, &ack, &message) == 4) {
          if (ack) {
            std::unique_lock lock(expectedLock);
            //long unsigned b = expected[senderId][fromId].count(message);
            //expected[fromId].erase(message); // does it work this way? test says yes
            expected[senderId][fromId].erase(message); // does it work this way? test says yes
            //long unsigned a = expected[senderId].count(message);
            //expectedLock.unlock();
            /*
            if (a == b) { // debug code, remove
                //writeError("failed to erase");
            }
            */

            // extract the size of the correct addresses
            std::shared_lock lockAddr(addessesLock);
            unsigned long addrSize = addresses2.size();

            std::unique_lock lockAck(ackLock); //remember to do this in the watcher
            ackMap[fromId][message].insert(fromId);
            if (ackMap[fromId][message].size() == addrSize) { // all the addresses have rebroadcasted my message
                std::unique_lock lockP(pastLock);
                past[fromId].erase(message);
                std::unique_lock lockDel(deliveredLock);
                delivered[fromId].insert(message); //TODO check if correct
                urbDeliver(message, fromId, std::string(buffer));
            }

            // add to the set of ack
          } else if (fromId == id) { // I was the original sender
            sendAckMine(message, from);

            /* moved up
            // do urb stuff
            // extract the size of the correct addresses
            std::shared_lock lockAddr(addessesLock);
            unsigned long addrSize = addresses2.size();

            std::unique_lock lockAck(ackLock); //remember to do this in the watcher
            ackMap[message].insert(fromId);
            if (ackMap[message].size() == addrSize) { // all the addresses have rebroadcasted my message
                std::unique_lock lockP(pastLock);
                past[fromId].erase(message);
                urbDeliver(message, fromId);
            }
            */
          } else { // not an ack and not one of my messages
            sendAck2(message, senderId, fromId);

              // this is related to urb
              // add to ack map
            std::shared_lock lock(forwardLock);
            std::unordered_set<unsigned long> s = forwardMap[fromId];
            forwardLock.unlock(); // is this correct?
            if (!(s.count(message) > 0)) {
                s.insert(message);
                //std::unique_lock lock(forwardLock);
                forwardLock.lock();
                forwardMap[fromId] = s;
                //forwardLock.unlock();
                bebBroadcast(message, fromId);
            }
            /*
            } else {
                // add to delivered
                std::unique_lock lockDel(deliveredLock);
                delivered[fromId].insert(message);
                // trigger urb delivery
                urbDeliver(message, fromId);
            }
            */
          }
      }
    }

    void urbDeliver(const unsigned long m, const unsigned long sId, const std::string buffer) {
      // fifo thing and then add to buffer?

      std::shared_lock LockShD(deliveredLock);
      if (delivered[sId].count(m) == 0) {
        if (buffer.size() > 8) { // if there is a chance of a past in the buffer
          std::vector<unsigned long> mPast = retrievePast(buffer);
          for (auto v : mPast) {
            if (delivered.count(v) == 0) {
              std::unique_lock outL(outBufferLock);
              //outBuffer.push_back(v);
              outBuffer.push_back(std::string("d ").append(std::to_string(sId)).append(" ").append(std::to_string(v)));
              std::unique_lock LockUhD(deliveredLock);
              delivered[sId].insert(v);
              std::unique_lock LockPast(pastLock);
              past[sId].insert(v); // not sure at all TODO check
            }
          }
        }
      }
      std::unique_lock outL(outBufferLock);
      if (sId == id) {
        outBuffer.push_back(std::string("b ").append(std::to_string(m)));
      } else {
        outBuffer.push_back(std::string("d ").append(std::to_string(sId)).append(" ").append(std::to_string(m)));
      }
      std::unique_lock LockUhD(deliveredLock);
      delivered[sId].insert(m);
      std::unique_lock LockPast(pastLock);
      past[sId].insert(m); // not sure at all TODO check
    }
    
    std::vector<unsigned long> retrievePast(const std::string buffer) {
      unsigned long start = 0;
      unsigned long end = buffer.find(";");
      std::vector<unsigned long> pastV;
      while (end != std::string::npos) {
        pastV.push_back(std::stoul(buffer.substr(start, end - start)));
        start = end + 1;
        end = buffer.find(";", start);
      }
      pastV.push_back(std::stoul(buffer.substr(start, end)));
      return pastV;
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
      if (outputFile.is_open()) {
        while (!outBuffer.empty()) {
          outputFile << outBuffer.front() << std::endl;
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

    // This could be avoided using the new field of the message (senderId)
    ssize_t sendAckMine(const unsigned long m, const sockaddr_storage * toAddr) {
      const struct sockaddr * to = reinterpret_cast<const sockaddr *>(toAddr);
      std::string charMid = std::to_string(m);
      std::string from = std::to_string(id);
      std::string Smess = from.append(",").append(from).append("1,").append(charMid);
      const char * mess = Smess.c_str();
      //writeError(Smess);
      return sendTo(mess, to);
    }

    // perfect link component
    ssize_t sendTo(const char * m, const struct sockaddr *to) {
      /*
      std::string mess = std::string(m);
      std::string phrase = "sendTo message: ";
      writeError(phrase.append(mess));
      */
      return sendto(sockfd, m, strlen(m), 0, to, sizeof *to); //htonl or htons?
    }
};
