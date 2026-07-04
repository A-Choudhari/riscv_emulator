#pragma once
#include <unordered_map>
#include <cstdint>

// TODO: 2-bit saturating counter branch predictor with a branch history
// table (BHT), indexed by PC.
//
// Background (classic Patterson & Hennessy states):
//   0 = Strongly Not Taken
//   1 = Weakly Not Taken
//   2 = Weakly Taken
//   3 = Strongly Taken
// Predict "taken" iff state >= 2. On resolution, nudge the counter one
// step toward the outcome that actually happened (saturating -- never go
// below 0 or above 3).
//
// Design questions to work out as you implement this:
//   - How many BHT entries? How do you hash/index a 32-bit PC into that
//     table (careful about collisions between different branches)?
//   - What should a not-yet-seen PC's initial state be?
//   - Where do accuracy stats get tallied -- before or after the counter
//     updates for this branch? (They need to reflect the prediction that
//     was actually *used*, not the post-update state.)
class BranchPredictor {
public:
    explicit BranchPredictor(size_t bhtEntries = 1024) : numEntries(bhtEntries) {}

    // Returns true if this PC's branch should be predicted taken.
    // Must NOT modify predictor state (this is just a lookup).
    bool predict(uint32_t pc) const {
        auto it = bht.find(pc);
        if (it != bht.end()) {
            return it->second >= 2;
        }
        return false;
    }

    // Called once the branch actually resolves (in EX). Updates the
    // counter for this PC's BHT entry, and should also update whatever
    // running accuracy/counts you want to expose via accuracy()/total().
    void update(uint32_t pc, bool actuallyTaken) {
        bool predicted = predict(pc);
        if(predicted == actuallyTaken) {
            correctPredictions++;
        }
        auto it = bht.find(pc);
        uint8_t state = (it != bht.end()) ? it->second : 0;
        if(actuallyTaken) {
            if(state < 3) {
                state++;
            } 
        } else {
            if(state > 0) {
                state--;
            }
        }
        bht[pc] = state;
        totalBranches++;
    }

    double accuracy() const {
        if(totalBranches == 0) {
            return 0.0;
        }
        return (double)correctPredictions / totalBranches;
    }

    uint64_t total() const { return totalBranches; }
    uint64_t correct() const { return correctPredictions; }

private:
    size_t numEntries;
    // TODO: pick a data structure for the BHT (e.g. unordered_map<uint32_t,
    // uint8_t> or a fixed-size std::vector<uint8_t> indexed by pc bits).
    std::unordered_map<uint32_t, uint8_t> bht;

    uint64_t totalBranches = 0;
    uint64_t correctPredictions = 0;
};
