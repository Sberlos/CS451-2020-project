#include <controller.hpp>
#include <fstream>

controller::controller(const unsigned long myId, const std::string configPath, const std::string outputPath, rcb * rP) : outPath(outputPath) {
    r = rP;

    // open config file for reading how many messages to broadcast
    std::ifstream configFile;
    std::string line;
    configFile.open(configPath);
    if (configFile.is_open()) {
        getline(configFile,line);
        toBroad = std::stoul(line); 
        while (getline(configFile,line)) {
            // from how I understand lcb I don't care about the others
            unsigned long spacePos = line.find(" ");
            if (std::stoul(line.substr(0, spacePos)) == myId) {
                elaborateDependecies(line.substr(spacePos + 1));
            }
        }
        configFile.close();
    }
}

void controller::broadcast() const {
    for (unsigned long i = 1; i <= toBroad; ++i) {
        r->rcoBroadcast(i);
    }
}

void controller::elaborateDependecies(const std::string & myLine) const {
    unsigned long start = 0;
    unsigned long end = myLine.find(" ");
    while (end != std::string::npos) {
        r->addDependency(std::stoul(myLine.substr(start, end - start)));
        start = end + 1;
        end = myLine.find(" ", start);
    }
    r->addDependency(std::stoul(myLine.substr(start, end - start)));
}

void controller::flushBuffer() const {
    std::ofstream outputFile(outPath);
    if (outputFile.is_open()) {
        std::deque<std::pair<const unsigned long, const unsigned long>> outQueue =
            r->getOutQueue();
        while (!outQueue.empty()) {
            std::pair<const unsigned long, const unsigned long> p = outQueue.front();
            if (p.first == 0) {
                outputFile << "b " << p.second << std::endl;
            } else {
                outputFile << "d " << p.first << " " << p.second << std::endl;
            }
            outQueue.pop_front();
        }
    outputFile.close();
    }
}

void controller::stopThreads() const {
    r->stopThreads();
}
