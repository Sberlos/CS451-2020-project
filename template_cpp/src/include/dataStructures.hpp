#ifndef dataStructures_hpp
#define dataStructures_hpp

#include <string>

struct deliverInfo {
    public:
        unsigned long fromId;
        unsigned long message;
        std::string buffer;

        deliverInfo() : fromId(), message(), buffer() {}
};
#endif
