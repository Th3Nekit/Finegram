#ifndef TGCALLS_V2WASM_SIGNALING_FRAMING_H
#define TGCALLS_V2WASM_SIGNALING_FRAMING_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tgcalls {
namespace v2wasm {

class SignalingFraming {
public:
    struct Delegate {
        std::function<void(std::vector<uint8_t> &&packet)> sendPacket;
        std::function<void(std::string &&message)> deliverMessage;
        std::function<void(std::string const &line)> log;
        std::function<int64_t()> nowMs;
    };

    explicit SignalingFraming(Delegate delegate);

    void sendMessage(std::string const &message);
    void receivePacket(std::vector<uint8_t> const &packet);

private:
    Delegate _delegate;
    uint32_t _counter = 0;
};

}
}

#endif
