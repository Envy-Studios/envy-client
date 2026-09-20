#include "pch.h"
#include "EjectCommand.h"
#include "client/Envy.h"
#include "client/misc/ClientMessageQueue.h"

EjectCommand::EjectCommand()
    : Command("eject", LocalizeString::get("client.commands.eject.desc"), "{0}") {
}

bool EjectCommand::execute(std::string const label, std::vector<std::string> args) {
    message(LocalizeString::get("client.commands.eject.ejectMsg.name"));
    Envy::get().queueEject();
    return true;
}
