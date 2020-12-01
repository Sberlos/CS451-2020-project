#ifndef urb_hpp
#define urb_hpp

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include "perfectLink.h"

class Urb {
    private:

        std::unordered_map<long unsigned, std::unordered_set<long unsigned>> forwardMap;
        std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_set<unsigned long>>> ackMap;
        std::unordered_map<unsigned long, std::set<unsigned long>> delivered;

        mutable std::shared_mutex forwardLock;
        mutable std::shared_mutex ackLock;
        mutable std::shared_mutex deliveredLock;

        // duplicate as already in pl but maybe better like that
        std::atomic_bool run;
        perfectLink * pl;
    public:

        // constructor
        Urb();

        void extractFromDelivering();
        void urbBroadcast(const unsigned long m);
        // move this to his own class?
        void bebBroadcast(const unsigned long m) const;
        void urbDeliver(const unsigned long fromId, const unsigned long m);

};
#endif
