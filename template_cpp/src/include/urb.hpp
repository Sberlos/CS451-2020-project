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

        mutable std::shared_mutex forwardLock;
        mutable std::shared_mutex ackLock;
        mutable std::shared_mutex deliveredLock;
        mutable std::shared_mutex deliveringLock;

        // duplicate as already in pl but maybe better like that
        std::atomic_bool run;
        perfect_link * pl;
    public:

        // constructor
        Urb(perfect_link * pfl);

        void extractFromDelivering();
        void checkToDeliver();
        void urbBroadcast(const unsigned long m);
        // move this to his own class?
        void bebBroadcast(const unsigned long m, const unsigned long fromId) const;
        void urbDeliver(const unsigned long fromId, const unsigned long m);
        void stopThreads();

};
#endif
