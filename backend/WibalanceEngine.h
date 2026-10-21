#ifndef WIBALANCE_ENGINE_H
#define WIBALANCE_ENGINE_H

class WiBalanceEngine {
private:
    int currentTick;
    int totalTicks;

public:
    WiBalanceEngine();

    void advanceTick();
    void reset();

    int getCurrentTick() const;
    bool isFinished() const;
};

#endif
