#include "perfectLink.hpp"

void perfectLink::initialize_network(unsigned short myPort) {
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
    unsigned myPortU = myPort;
    const char * myPortC = std::to_string(myPortU).c_str();

    if ((status = getaddrinfo(NULL, myPortC, &hints, &servinfo)) != 0) {
          fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
          //writeError("Error while initializing network");
          exit(1); //exit or retry?
    }
    // make a socket:
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    // Set the timeout for the socket for allowing to stop it
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 100000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

    // bind it to the port we passed in to getaddrinfo():
    bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

    // free the allocated linked list
    freeaddrinfo(servinfo);
}

void perfectLink::free_network() {
    for (auto s : addresses) {
        freeaddrinfo(s.second);
    }

void perfectLink::addHost(unsigned long hId, unsigned short hPort, std::string hIp) {
    int status;
    struct addrinfo hints, *si;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_DGRAM; //use UDP

    unsigned hPortU = hPort;
    const char * hPortC = std::to_string(hPortU).c_str();

    // leave null as address for now -> localhost
    // if I want to specify the ip I should remove the AI_PASSIVE from hints
    // and substitute NULL with my ip
    //status = getaddrinfo(hIp.c_str(), convertIntMessageS(hPort), &hints, &si);
    status = getaddrinfo(hIp.c_str(), hPortC, &hints, &si);
    if (status != 0) {
        return;
    }

    addresses[hId] = si;
}

void perfectLink::handleMessages() {
    struct sockaddr_storage their_addr;
    socklen_t addr_size;

    // singlethreaded for now
    while (run.load()) {
        addr_size = sizeof their_addr;
        long int bytesRcv;
        char buffer[20];
        // not sure if needed
        memset(&buffer, '\0', 20);
        bytesRcv = recvfrom(sockfd, buffer, 20, 0, reinterpret_cast<struct sockaddr *>(&their_addr), &addr_size);
        if (bytesRcv == -1) {
            continue;
        }
        parseMessage(buffer, bytesRcv, &their_addr);
    }
}

void perfectLink::parseMessage(const char * buffer, const long bytesRcv, const struct sockaddr_storage * from) {

    unsigned long senderId;
    unsigned long fromId;
    unsigned long message;
    int ack;
    int parsedN;
    if (std::sscanf(buffer,"%lu,%lu,%d,%lu", &senderId, &fromId, &ack, &message) == 4) {
        std::cout << "received:" << buffer << std::endl;
        if (ack) {
            std::cout << "ack from:" << senderId << std::endl;
            std::unique_lock expLock(expectedLock);
            expected[senderId][fromId].erase(message); // does it work this way? test says yes
            expLock.unlock();

            // extract the size of the correct addresses
            std::shared_lock lockAddr(addessesLock);
            unsigned long addrSize = addresses2.size();

            std::unique_lock lockAck(ackLock); //remember to do this in the watcher
            ackMap[fromId][message].insert(senderId);
            std::cout << ackMap[fromId][message].size() << " : " << addrSize << std::endl;
            lockAck.unlock();

            // TODO change with the connection with the layer above
            deliver(fromId, senderId, message);
        } else {
            if (fromId == id) {
                sendAckMine(message, from);
            } else {
                sendAck(message, senderId, fromId);
            }
            std::shared_lock delShLock(deliveredLock);
            if (delivered[fromId].count(message) < 1) {
                delShLock.unlock();
                std::unique_lock delULock(deliveredLock);
                delivered[fromId].insert(message);
                delULock.unlock();
                // TODO change with the connection with the layer above
                deliver(fromId, senderId, message);
            }
        }
    }
}

ssize_t perfectLink::sendAck(unsigned long m, unsigned long toId, unsigned long originalS) {
    struct addrinfo * to = addresses[toId];

    std::string charMid = std::to_string(m);
    std::string from = std::to_string(id);
    std::string strOriginal = std::to_string(originalS);
    std::string Smess = from.append(",").append(strOriginal).append(",1,").append(charMid); //TODO implement new way
    const char * mess = Smess.c_str();

    //writeError(Smess);
    return sendTo2(mess, to);
}

void perfectLink::checker() {
    while (run.load()) {
        // TODO change the value afterwards
        std::this_thread::sleep_for(std::chrono::seconds(1));

        for (auto id_MapFrom : expected) {
            for (auto from_m : id_MapFrom.second) {
                for (auto m_count : from_m.second) {
                    sendTrack(m_count.first, id_MapFrom.first, from_m.first);
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

                std::cout << m.second.size() << " : " << addrSize << std::endl;
                // all the addresses have rebroadcasted my message
                //if (m.second.size() == addrSize) {
                if (m.second.size() == addrSize - 1) {
                    std::unique_lock lockP(pastLock);
                    past[i.first].erase(m.first);
                    lockP.unlock();
                    urbDeliver(m.first, i.first, std::string(""));
                }
            }
        }
    }
}

// Send data and add the tracking to it if missing
ssize_t perfectLink::sendTrack(const unsigned long m, const unsigned long toId, const unsigned long fromId) {
    //writeError("S: starting sendTrack");
    std::cout << "m:" << m << ",to:" << toId << ",from:" << fromId << std::endl;

    //std::unordered_map<long unsigned int, int> mE = expected[toId];
    std::unique_lock lock(expectedLock);
    unsigned vE = expected[toId][fromId][m];
    expected[toId][fromId][m] = ++vE;
    //expectedLock.unlock();
    if (vE > expectedTreshold) {
        // remove process from correct -> addresses list
        std::unique_lock lockA(addessesLock);
        // I should also free the addrinfo
        freeaddrinfo(addresses2[toId]);
        addresses2.erase(toId);
        return 0;
    }

    std::shared_lock lockA(addessesLock);
    struct addrinfo * to = addresses2[toId];

    const std::string fromIdS = std::to_string(fromId);
    std::string sM = std::to_string(id).append(",").append(fromIdS).append(",0,").append(std::to_string(m));
    // append past
    std::string newSM;
    newSM = appendPast(sM, fromId);
    const char * charM = newSM.c_str();
    return sendTo2(charM, to);
}

std::string perfectLink::appendPast(std::string mS, unsigned long pId) {
    std::shared_lock pSLock(pastLock);
    for (unsigned long m : past[pId]) {
        mS.append(";").append(std::to_string(m));
    }

    return mS;
}

ssize_t perfectLink::sendTo2(const char * m, const struct addrinfo *to) {
      std::cout << "sending:" << m << std::endl;
      return sendto(sockfd, m, strlen(m), 0, to->ai_addr, to->ai_addrlen);
}

void perfectLink::deliver(const unsigned long fromId, const unsigned long senderId, const unsigned long m, const std::string buffer) {
    struct deliverInfo * data = new deliverInfo(fromId, senderId, m, buffer);
    std::unique_lock dequeLock(deliveringLock);
    delivering.push_back(data);
}
