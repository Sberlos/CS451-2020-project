#include "urb.hpp"

Urb() : forwardMap(), ackMap(), delivered(), run(false) {}

void Urb::extractFromDelivering() {
    struct deliverInfo * data = pl->getDelivered();
    if (data) {
        //if (fromId != id) { //?
        // TODO ask TA if the sender can also be me
        std::unique_lock ackL(ackLock);
        // TODO get id
        ackMap[data->fromId][m].insert(pl->getId());

        std::shared_lock forwardShL(forwardLock);
        if (forwardMap[data->fromId].count(data->message) < 1) {
            forwardShL.unlock();
            std::unique_lock forwardUL(forwardLock);
            forwardMap[data->fromId].insert(data->message);
            forwardUL.unlock();
            bebBroadcast(data->message, data->fromId);
        }
    }
}

void Urb::bebBroadcast(const unsigned long message, const unsigned long fromId) {
    std::vector<unsigned long> addressesIds = pl->getAddressesIds();
    const unsigned long id = pl->getId();
    for (auto peerId : addressesIds) {
        if (peerId != id) {
            pl->sendTrack(message, peer.first, fromId);
        }
    }
}

void Urb::urbBroadcast(const unsigned long m) {
    std::unique_lock fLock(forwardLock);
    forwardMap[pl->getId()].insert(m);
    fLock.unlock();
    bebBroadcast(m, pl->getId());
}

// TODO
void urbDeliver(const unsigned long fromId, const unsigned long m) {
}
