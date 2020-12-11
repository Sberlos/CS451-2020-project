#ifndef urb_hpp
#define urb_hpp

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <perfectLink.hpp>

class Urb {
    private:

        std::unordered_map<long unsigned, std::unordered_set<long unsigned>> forwardMap;
        std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_set<unsigned long>>> ackMap;
        std::unordered_map<unsigned long, std::set<unsigned long>> delivered;
        std::deque<deliverInfo*> delivering;

        // I would love to find a better way than having a map only for this
        std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::string>> pastBufferMap;

        mutable std::shared_mutex forwardLock;
        mutable std::shared_mutex ackLock;
        mutable std::shared_mutex deliveredLock;
        mutable std::shared_mutex deliveringLock;
        mutable std::shared_mutex pBuffLock;

        // duplicate as already in pl but maybe better like that
        std::atomic_bool run;
        perfect_link * pl;
    public:

        // constructor
        Urb(perfect_link * pfl);

        void extractFromDelivering();
        void checkToDeliver();
        void urbBroadcast(const unsigned long & m, const std::string & past);
        // move this to his own class?
        void bebBroadcast(const unsigned long & m, const unsigned long & fromId, const std::string & past) const;
        void urbDeliver(const unsigned long & fromId, const unsigned long & m);
        void stopThreads();

        // getters for the layer above
        unsigned long getId() const;
        deliverInfo * getDelivered();
};
#endif
