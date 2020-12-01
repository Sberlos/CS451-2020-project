#ifndef perfectLink_hpp
#define perfectLink_hpp

class perfect_link {
    private:
        unsigned long id;
        unsigned long toBroad;

        std::unordered_map<unsigned long, addrinfo *> addresses;
        std::unordered_map<long unsigned, std::unordered_map<long unsigned, std::unordered_map<long unsigned, unsigned>>> expected;
        
        unsigned expectedTreshold;
        std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_set<unsigned long>>> ackMap;
        // Isn't this replicated in every layer?
        std::unordered_map<unsigned long, std::set<unsigned long>> delivered;

        std::unordered_map<unsigned long, std::unordered_set<unsigned long>> past;

        //TODO check if I will use it
        std::deque<deliverInfo*> delivering;

        int sockfd;

        mutable std::shared_mutex addessesLock;
        mutable std::shared_mutex ackLock;
        mutable std::shared_mutex pastLock;
        mutable std::shared_mutex deliveredLock;

        //TODO check if I will use it
        mutable std::shared_mutex deliveringLock;

        std::atomic_bool run;

    public:

        perfect_link(long unsigned int p_id) : id(p_id), addresses(), expected(), ackMap(), delivered(), past(), delivering(), run(true) {};

        // all methods
        void parseMessage(const char * buffer, const long bytesRcv, const struct sockaddr_storage * from);
        void handleMessages();
        void addHost(unsigned long hId, unsigned short hPort, std::string hIp);
        ssize_t sendAck(unsigned long m, unsigned long toId, unsigned long originalS);
        void checker();
        std::string appendPast(std::string mS, unsigned long pId);
        ssize_t sendTrack(const unsigned long m, const unsigned long toId, const unsigned long fromId);
        ssize_t sendTo2(const char * m, const struct addrinfo *to);
        void free_network();
        void initialize_network(unsigned short myPort);

        //TODO check if I will use it
        void deliver(const unsigned long fromId, const unsigned long senderId, const unsigned long m, const std::string buffer);

        // getters TODO think of a better way
        std::vector<unsigned long> getAddressesIds();
        unsigned long getId();
};

#endif
