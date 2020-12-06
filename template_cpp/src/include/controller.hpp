#ifndef controller_hpp
#define controller_hpp

#include "rcb.hpp"

class controller {
    private:
        // number of messages to broadcast (coming from config)
        unsigned long toBroad;
        std::string outPath;
        rcb * r;

    public:
        controller(const unsigned long myId, const std::string configPath, const std::string outputPath, rcb * rP);

        void broadcast() const;
        void elaborateDependecies(const std::string & myLine) const;
        void flushBuffer() const;
};
#endif
