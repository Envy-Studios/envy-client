#pragma once
class EnvyClientMessageEvent : public Event {
public:
    static const uint32_t hash = TOHASH(EnvyClientMessageEvent);

    EnvyClientMessageEvent(std::string const& msg)
        : message(msg) {}

    [[nodiscard]] std::string getMessage() { return message; }

private:
    std::string message;
};
