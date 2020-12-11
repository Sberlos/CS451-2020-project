#ifndef perfectLink_hpp
#define perfectLink_hpp

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <vector>
#include <deque>
#include <iostream>
#include <thread>
#include <cstring>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include <dataStructures.hpp>

class perfect_link {
    private:
        unsigned long id;
        unsigned long toBroad;

        std::unordered_map<unsigned long, addrinfo *> addresses;
        //std::unordered_map<long unsigned, std::unordered_map<long unsigned, std::unordered_map<long unsigned, unsigned>>> expected;
        // from who I expect the ack -> original sender of m -> m -> (count, past)
        std::unordered_map<long unsigned, std::unordered_map<long unsigned, std::unordered_map<long unsigned, std::pair<unsigned, std::string>>>> expected;
        
        unsigned expectedTreshold = 25;
        std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_set<unsigned long>>> ackMap;
        // Isn't this replicated in every layer?
        //std::unordered_map<unsigned long, std::set<unsigned long>> delivered;
        //This new version includes the sender: from -> sender -> message
        std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::set<unsigned long>>> delivered;

        std::unordered_map<unsigned long, std::unordered_set<unsigned long>> past;

        //TODO check if I will use it
        std::deque<deliverInfo*> delivering;

        int sockfd;

        mutable std::shared_mutex addessesLock;
        mutable std::shared_mutex ackLock;
        mutable std::shared_mutex pastLock;
        mutable std::shared_mutex deliveredLock;
        mutable std::shared_mutex expectedLock;

        //TODO check if I will use it
        mutable std::shared_mutex deliveringLock;

        std::atomic_bool run;

    public:

        perfect_link(long unsigned int p_id) : id(p_id), addresses(), expected(), ackMap(), delivered(), past(), delivering(), run(true) {};

        // all methods
        //void parseMessage(const char * buffer, const struct sockaddr_storage * from);
        void parseMessage(const char * buffer);
        void handleMessages();
        void addHost(unsigned long hId, unsigned short hPort, std::string hIp);
        ssize_t sendAck(const unsigned long & m, const unsigned long & toId, const unsigned long & originalS);
        void checker();
        std::string appendPast(std::string mS, const unsigned long & pId);
        ssize_t sendTrack(const unsigned long & m, const unsigned long & toId, const unsigned long & fromId, const std::string & past);
        ssize_t sendTo2(const char * m, const struct addrinfo *to);
        void free_network();
        void initialize_network(unsigned short myPort);

        //TODO check if I will use it
        void deliver(const unsigned long & fromId, const unsigned long & senderId, const unsigned long & m, const std::string & buffer);
        deliverInfo * getDelivered();

        // getters TODO think of a better way
        std::vector<unsigned long> getAddressesIds() const;
        unsigned long getId() const;

        void stopThreads();

        std::string extractPast(const std::string & buffer) const;
};

#endif
