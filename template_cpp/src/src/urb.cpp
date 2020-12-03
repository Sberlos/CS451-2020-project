#include <urb.hpp>
#include <perfectLink.hpp>

Urb::Urb(perfect_link * pfl) : forwardMap(), ackMap(), delivered(), run(true) {
    pl = pfl;
}

void Urb::extractFromDelivering() {
    // This thing is wrong: I have to insert the sender and not always myself!
    // check slide 34: pi is not always myself!
    // TODO -> fixed at line 16 (I use senderId now) -> check if right
    while (run.load()) {
        deliverInfo * data = pl->getDelivered();
        if (data) {
            std::cout << "extracting from delivery:" << data->message << ", from:" << data->senderId << std::endl;
            //if (fromId != id) { //?
            // TODO ask TA if the sender can also be me
            std::unique_lock ackL(ackLock);
            // TODO get id
            //ackMap[data->fromId][data->message].insert(pl->getId());
            ackMap[data->fromId][data->message].insert(data->senderId);

            std::shared_lock forwardShL(forwardLock);
            if (forwardMap[data->fromId].count(data->message) < 1) {
                forwardShL.unlock();
                std::unique_lock forwardUL(forwardLock);
                forwardMap[data->fromId].insert(data->message);
                forwardUL.unlock();
                bebBroadcast(data->message, data->fromId);
            }
            delete data;
        } else {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void Urb::checkToDeliver() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    while (run.load()) {
        std::shared_lock forLock(forwardLock);
        for (auto id_mSet : forwardMap) {
            unsigned long pId = id_mSet.first;
            for (auto m : id_mSet.second) {
                std::shared_lock aLock(ackLock);
                unsigned long ackS = ackMap[pId][m].size();
                aLock.unlock();
                std::unique_lock dLock(deliveredLock);
                if (pl->getAddressesIds().size() <= ackS &&
                        delivered[pId].count(m) < 1) {
                    delivered[pId].insert(m);
                    dLock.unlock();
                    urbDeliver(pId, m);
                }
            }
        }
        forLock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Urb::bebBroadcast(const unsigned long message, const unsigned long fromId) const {
    std::vector<unsigned long> addressesIds = pl->getAddressesIds();
    const unsigned long id = pl->getId();
    for (auto peerId : addressesIds) {
        if (peerId != id) {
            pl->sendTrack(message, peerId, fromId);
        }
    }
}

void Urb::urbBroadcast(const unsigned long m) {
    std::unique_lock fLock(forwardLock);
    forwardMap[pl->getId()].insert(m);
    fLock.unlock();
    bebBroadcast(m, pl->getId());
}

void Urb::urbDeliver(const unsigned long fromId, const unsigned long m) {
    deliverInfo * data = new deliverInfo(fromId, 0, m, "");
    std::unique_lock dequeLock(deliveringLock);
    delivering.push_back(data);
    if (fromId == pl->getId()) {
        std::cout << "b " << m << std::endl;
    } else {
        std::cout << "d " << fromId << " " << m << std::endl;
    }
}

void Urb::stopThreads() {
    run = false;
    pl->stopThreads();
}
