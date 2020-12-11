#ifndef rcb_hpp
#define rcb_hpp

#include "urb.hpp"

class rcb {
    private:
        // Using a set and not a unordered one for making iterating on it faster?
        // I don't think it can happen that a value is inserted twice as rcoBroadcast is
        // called only once for message at the top level, make it a vector?
        std::unordered_map<unsigned long, std::set<unsigned long>> delivered;

        // past: vector of (id, m) pairs [will change to list if I clear the history]
        std::vector<std::pair<const unsigned long, const unsigned long>> past;

        //std::deque<std::pair<const unsigned long, const unsigned long>> outQueue;
        std::deque<std::pair<const unsigned long, const unsigned long>> outQueue;

        std::unordered_set<unsigned long> dependencies;

        std::atomic_bool run;
        Urb * urb;

        mutable std::shared_mutex pastLock;
        // this can be a single lock
        mutable std::shared_mutex deliveredLock;
        // this can be a single lock
        mutable std::shared_mutex outLock;
    public:
        rcb(Urb * u);

        void extractFromDelivering();
        void rcoBroadcast(const unsigned long & m);
        void rcoDeliver(const unsigned long & fromId, const unsigned long & m);
        // I have the past of everybody but in reality I am the only one calling
        // rcoBroadcast, therefore I can be the only one creating the past
        std::string createPastString() const;
        void addDependency(const unsigned long & process);
        std::vector<std::pair<const unsigned long, const unsigned long>> pastToV(const std::string & past) const;
        std::deque<std::pair<const unsigned long, const unsigned long>> getOutQueue() const;
        void stopThreads();
};
#endif
