#pragma once
#include "../Packet.h"

namespace SDK {
    enum class MobEffectEvent : int {
        Add = 1,
        Modify = 2,
        Remove = 3
    };

    // MobEffectPacket (id 0x1D).
    // The game's own class orders { effectId, eventType } or { eventType, effectId } after
    // the runtime id depending on the version; slotA/slotB are resolved at runtime by
    // PotionHUD, which validates which interpretation is plausible for the running game.
    class MobEffectPacket : public Packet {
    public:
        uint64_t runtimeID;
        int32_t slotA;
        int32_t slotB;
        int32_t amplifier;
        bool showParticles;
        int32_t durationTicks;
    };
}
