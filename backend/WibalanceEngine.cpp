#include "WiBalanceEngine.h"

WiBalanceEngine::WiBalanceEngine()
    : currentTick(0), totalTicks(50) {
}

void WiBalanceEngine::advanceTick() {
    if (currentTick < totalTicks) {
        currentTick++;
    }
}

void WiBalanceEngine::reset() {
    currentTick = 0;
}

int WiBalanceEngine::getCurrentTick() const {
    return currentTick;
}

bool WiBalanceEngine::isFinished() const {
    return currentTick >= totalTicks;
}
