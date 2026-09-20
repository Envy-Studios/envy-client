#include "pch.h"
#include "Eventing.h"
#include "client/Envy.h"

Eventing& Eventing::get() {
    return Envy::getEventing();
}
