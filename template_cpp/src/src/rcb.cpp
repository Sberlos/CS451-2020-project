#include <rcb.hpp>

rcb::rcb(Urb * u) : delivered(), past(), outQueue(), dependencies(), run(true) {
    urb = u;
}

void rcb::extractFromDelivering() {
    while (run.load()) {
        deliverInfo * data = urb->getDelivered();
        if (data) {
            std::cout << "rcb delivery:" << data->message << data->buffer << std::endl;
            std::unique_lock delLock(deliveredLock);
            if (delivered[data->fromId].count(data->message) < 1) {
                // if there is a past to extract
                if (data->buffer.size() > 0) {
                    std::vector<std::pair<const unsigned long, const unsigned long>> pV = 
                        pastToV(data->buffer);

                    // loop through the past
                    for (auto pairP : pV) {
                        // If not delivered yet I deliver and add to the map
                        if (delivered[pairP.first].count(pairP.second) < 1) {
                            rcoDeliver(pairP.first, pairP.second);
                            delivered[pairP.first].insert(pairP.second);

                            // if somebody which I depend upon I add to my past
                            if (dependencies.count(pairP.first) > 0) {
                                std::pair<const unsigned long, const unsigned long> pastPair = 
                                    std::pair(pairP.first, pairP.second);
                                std::unique_lock pLock(pastLock);
                                past.push_back(pastPair);
                            }
                        }
                    }
                // for my messages I loop through my past, this avoids me to add the past
                // to ack messages (that are the ones delivered for my messages)
                } else if (data->fromId == urb->getId()) {
                    std::shared_lock pLock(pastLock);
                    const unsigned long myId = urb->getId();
                    for (auto p : past) {
                        /*
                        if (p.first == myId && p.second != data->message) {
                            if (delivered[myId].count(p.second) < 1) {
                                rcoDeliver(myId, p.second);
                                delivered[myId].insert(p.second);
                            }
                        }
                        */
                        if (p.first == myId) {
                            if (p.second == data->message) {
                                break;
                            }
                            if (delivered[myId].count(p.second) < 1) {
                                rcoDeliver(myId, p.second);
                                delivered[myId].insert(p.second);
                            }
                        }
                    }
                    pLock.unlock();
                }

                // Deliver the actual message
                rcoDeliver(data->fromId, data->message);
                delivered[data->fromId].insert(data->message);

                // if somebody which I depend upon I add to my past
                if (dependencies.count(data->fromId) > 0 && data->fromId != urb->getId()) {
                    std::pair<const unsigned long, const unsigned long> pastPair = 
                        std::pair(data->fromId, data->message);
                    std::unique_lock pLock(pastLock);
                    past.push_back(pastPair);
                }
            }

            delete data;
        } else {
            // This avoid a busy loop and lead to querying for data with higher success
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
}

void rcb::rcoBroadcast(const unsigned long & m) {
    urb->urbBroadcast(m, createPastString());
    std::unique_lock pULock(pastLock);
    //past[urb->getId()].push_back(m);
    past.push_back(std::pair<const unsigned long, const unsigned long>(urb->getId(), m));
    pULock.unlock();
    std::unique_lock bLock(outLock);
    outQueue.push_back(std::make_pair(0, m));
    bLock.unlock();
    std::cout << "b " <<  m << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void rcb::rcoDeliver(const unsigned long & fromId, const unsigned long & m) {
    std::unique_lock bLock(outLock);
    outQueue.push_back(std::make_pair(fromId, m));
    std::cout << "d " << fromId << " " << m << std::endl;
}

std::string rcb::createPastString() const {
    std::string pS;
    std::shared_lock pLock(pastLock);
    for (auto pair : past) {
        pS.append(";").append(std::to_string(pair.first)).append("-").append(std::to_string(pair.second));
    }
    return pS;
}

void rcb::addDependency(const unsigned long & process) {
    std::cout << "adding dependency " << process << std::endl;
    // no need to synchronize as it's called sequentially by a single thread
    dependencies.insert(process);
}

std::vector<std::pair<const unsigned long, const unsigned long>> rcb::pastToV(const std::string & past) const {
    unsigned long start = 0;
    unsigned long end = past.find(";", 1) - 1;
    std::vector<std::pair<const unsigned long, const unsigned long>> pastV;
    if (past.find(";", 1) != std::string::npos) {
        while (end != std::string::npos) {
            unsigned long split = past.find("-", start);
            //std::cout << "stoul-110:" << past.substr(start + 1, split - start - 1) << std::endl;
            const unsigned long id = std::stoul(past.substr(start + 1, split - start - 1));
            //std::cout << "stoul-112:" << past.substr(split + 1, end - split) << std::endl;
            const unsigned long m = std::stoul(past.substr(split + 1, end - split));
            std::pair<const unsigned long, const unsigned long> tmpP = std::pair(id, m);
            pastV.push_back(tmpP);

            // update values for looping
            start = end + 1;
            //end = past.find(";", start + 1) - 1;
            end = past.find(";", start + 1);
            if (end != std::string::npos) {
                --end;
            }

        }
    }
    unsigned long split = past.find("-", start + 1);
    //std::cout << "stoul-127:" << past.substr(start + 1, split - start - 1) << std::endl;
    const unsigned long id = std::stoul(past.substr(start + 1, split - start - 1));
    //std::cout << "stoul-129:" << past.substr(split + 1, end - split - 1) << std::endl;
    const unsigned long m = std::stoul(past.substr(split + 1, end - split - 1));
    std::pair<const unsigned long, const unsigned long> tmpP = std::pair(id, m);
    pastV.push_back(tmpP);

    return pastV;
}

std::deque<std::pair<const unsigned long, const unsigned long>> rcb::getOutQueue() const {
    return outQueue;
}

void rcb::stopThreads() {
    run = false;
    urb->stopThreads();
}
