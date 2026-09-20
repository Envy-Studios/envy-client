#include "pch.h"
#include "JsEvented.h"
#include "client/Envy.h"
#include "client/script/PluginManager.h"

JsValueRef JsEvented::dispatchEvent(Event& ev) {
    for (auto& evs : this->eventListeners[ev.name]) {
        Chakra::SetContext(evs.second);

        ev.arguments.insert(ev.arguments.begin(), evs.first);
        JsValueRef val;

        Envy::getPluginManager().handleErrors(Chakra::CallFunction(
            evs.first, ev.arguments.data(), static_cast<unsigned short>(ev.arguments.size()), &val));
        return val;
    }
    return JS_INVALID_REFERENCE;
}
