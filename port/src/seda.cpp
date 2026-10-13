#include "seda.hpp"

namespace {

bool g_forced = false;

} // namespace

void SedaSetForced(bool forced) {
    g_forced = forced;
}

bool SedaActive() {
    return g_forced;
}
