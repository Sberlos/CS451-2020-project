#include "perfectLink.hpp"

void perfect_link::initialize_network(unsigned short myPort) {
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_DGRAM; //use UDP
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    //writeError(convertIntMessageS(myPort));
    int mP = myPort;
    //writeError(std::to_string(mP));
    unsigned myPortU = myPort;
    std::string myPortS = std::to_string(myPortU);

    if ((status = getaddrinfo(NULL, myPortS.c_str(), &hints, &servinfo)) != 0) {
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
    int res = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (res == -1) {
        freeaddrinfo(servinfo);
        fprintf(stderr, "bind error: %s\n", gai_strerror(res)); //not sure
        exit(-1);
    }
    // free the allocated linked list
    freeaddrinfo(servinfo);
}

void perfect_link::free_network() {
    for (auto s : addresses) {
        freeaddrinfo(s.second);
    }
}

void perfect_link::addHost(unsigned long hId, unsigned short hPort, std::string hIp) {
    int status;
    struct addrinfo hints, *si;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_DGRAM; //use UDP

    unsigned hPortU = hPort;
    std::string myPortS = std::to_string(hPortU);

    // leave null as address for now -> localhost
    // if I want to specify the ip I should remove the AI_PASSIVE from hints
    // and substitute NULL with my ip
    //status = getaddrinfo(hIp.c_str(), convertIntMessageS(hPort), &hints, &si);
    status = getaddrinfo(hIp.c_str(), myPortS.c_str(), &hints, &si);
    if (status != 0) {
        return;
    }

    addresses[hId] = si;
}

void perfect_link::handleMessages() {
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
        parseMessage(buffer);
    }
}

//void perfect_link::parseMessage(const char * buffer, const struct sockaddr_storage * from) {
void perfect_link::parseMessage(const char * buffer) {

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

            /*
            // extract the size of the correct addresses
            std::shared_lock lockAddr(addessesLock);
            unsigned long addrSize = addresses.size();
            */

            std::unique_lock lockAck(ackLock); //remember to do this in the watcher
            ackMap[fromId][message].insert(senderId);
            //std::cout << ackMap[fromId][message].size() << " : " << addrSize << std::endl;
            lockAck.unlock();

            std::shared_lock delShLock(deliveredLock);
            if (delivered[fromId][id].count(message) < 1) {
                delShLock.unlock();
                std::unique_lock delULock(deliveredLock);
                delivered[fromId][id].insert(message);
                delULock.unlock();
                deliver(fromId, id, message, buffer);
            }
        } else {
            sendAck(message, senderId, fromId);
            std::shared_lock delShLock(deliveredLock);
            if (delivered[fromId][senderId].count(message) < 1) {
                delShLock.unlock();
                std::unique_lock delULock(deliveredLock);
                delivered[fromId][senderId].insert(message);
                delULock.unlock();
                deliver(fromId, senderId, message, buffer);
            }
        }
    }
}

ssize_t perfect_link::sendAck(unsigned long m, unsigned long toId, unsigned long originalS) {
    struct addrinfo * to = addresses[toId];

    std::string charMid = std::to_string(m);
    std::string from = std::to_string(id);
    std::string strOriginal = std::to_string(originalS);
    std::string Smess = from.append(",").append(strOriginal).append(",1,").append(charMid); //TODO implement new way
    const char * mess = Smess.c_str();

    //writeError(Smess);
    return sendTo2(mess, to);
}

void perfect_link::checker() {
    while (run.load()) {
        // TODO change the value afterwards
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // TODO problem here: I check only expected and not if the node has been erased
        std::shared_lock expL(expectedLock);
        for (auto id_MapFrom : expected) {
            for (auto from_m : id_MapFrom.second) {
                for (auto m_pair : from_m.second) {
                    sendTrack(m_pair.first, id_MapFrom.first, from_m.first, m_pair.second.second);
                }
            }
        }

        // check if in the meantime some process failes therefore we have to check if we
        // previously completed the uniform messaging procedure, if yes deliver
        // TODO I have to remove the completed messages from the ackMap otherwise too much work
        /* This thing belongs to URB move there TODO
        std::shared_lock lockAddr(addessesLock);
        unsigned long addrSize = addresses.size();

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
        */
    }
}

// Send data and add the tracking to it if missing
ssize_t perfect_link::sendTrack(const unsigned long m, const unsigned long toId, const unsigned long fromId, const std::string past) {
    //writeError("S: starting sendTrack");
    std::cout << "m:" << m << ",to:" << toId << ",from:" << fromId << std::endl;

    //std::unordered_map<long unsigned int, int> mE = expected[toId];
    std::unique_lock lockE(expectedLock);
    unsigned vE = expected[toId][fromId][m].first;
    expected[toId][fromId][m].first = ++vE; // Can I do it? TODO check
    expected[toId][fromId][m].second = past;
    //expectedLock.unlock();
    if (vE > expectedTreshold) {
        // remove process from correct -> addresses list
        std::unique_lock lockA(addessesLock);
        // I should also free the addrinfo
        freeaddrinfo(addresses[toId]);
        addresses.erase(toId);
        return 0;
    }

    std::shared_lock lockA(addessesLock);
    struct addrinfo * to = addresses[toId];

    const std::string fromIdS = std::to_string(fromId);
    std::string sM = std::to_string(id).append(",").append(fromIdS).append(",0,").append(std::to_string(m));
    // append past
    sM.append(past);
    const char * charM = sM.c_str();
    return sendTo2(charM, to);
}

std::string perfect_link::appendPast(std::string mS, unsigned long pId) {
    std::shared_lock pSLock(pastLock);
    for (unsigned long m : past[pId]) {
        mS.append(";").append(std::to_string(m));
    }

    return mS;
}

ssize_t perfect_link::sendTo2(const char * m, const struct addrinfo *to) {
      std::cout << "sending:" << m << std::endl;
      return sendto(sockfd, m, strlen(m), 0, to->ai_addr, to->ai_addrlen);
}

void perfect_link::deliver(const unsigned long fromId, const unsigned long senderId, const unsigned long m, const std::string buffer) {
    deliverInfo * data = new deliverInfo(fromId, senderId, m, buffer);
    std::unique_lock dequeLock(deliveringLock);
    delivering.push_back(data);
}

deliverInfo * perfect_link::getDelivered() {
    std::unique_lock dequeLock(deliveringLock);
    if (!delivering.empty()) {
        deliverInfo * data = delivering.front();
        delivering.pop_front();
        return data;
    }
    return NULL;
}

std::vector<unsigned long> perfect_link::getAddressesIds() const {
    std::vector<unsigned long> addressesIds;
    std::shared_lock addrLock(addessesLock);
    for (auto id_addr : addresses) {
        addressesIds.push_back(id_addr.first);
    }
    return addressesIds;
}

unsigned long perfect_link::getId() const {
    return id;
}

void perfect_link::stopThreads() {
    run = false;
}

std::string perfect_link::extractPast(const std::string buffer) const {
    unsigned long start = buffer.find(";");
    if (start == std::string::npos) {
        return "";
    }
    return buffer.substr(start);
}
