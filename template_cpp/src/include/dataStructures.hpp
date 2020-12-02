#ifndef dataStructures_hpp
#define dataStructures_hpp

#include <string>

struct deliverInfo {
    public:
        const unsigned long fromId;
        const unsigned long senderId;
        const unsigned long message;
        const std::string buffer;

        deliverInfo() : fromId(), senderId(), message(), buffer() {}
        deliverInfo(const unsigned long f, const unsigned long s, const unsigned long m, const std::string b) : fromId(f), senderId(s), message(m), buffer(b) {}
};
#endif
