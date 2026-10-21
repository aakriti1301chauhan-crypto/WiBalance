#ifndef WIBALANCE_CORE_H
#define WIBALANCE_CORE_H

#include "WiBalanceAPI.h"

class WiBalanceCore {
public:
    WiBalanceCore();
    ~WiBalanceCore();

    WiBalanceCore(const WiBalanceCore&) = delete;
    WiBalanceCore& operator=(const WiBalanceCore&) = delete;

    bool beginSimulation();
    bool advanceOneTick();

    WiBalanceSnapshot getSnapshot() const;

private:
    struct Impl;
    Impl* impl;
};

#endif
