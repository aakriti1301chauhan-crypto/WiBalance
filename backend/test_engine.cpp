
#include <iostream>
#include "WiBalanceEngine.h"

int main() {
    WiBalanceEngine engine;

    std::cout << "Starting tick: "
              << engine.getCurrentTick() << '\n';

    engine.advanceTick();

    std::cout << "After one step: "
              << engine.getCurrentTick() << '\n';

    engine.reset();

    std::cout << "After reset: "
              << engine.getCurrentTick() << '\n';

    return 0;
}

