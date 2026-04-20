// made by yeth | MIT license
#pragma once

#include <string>
#include <vector>
#include <cstdint>

// flipper .ir files have two signal types. "parsed" means a known protocol
// like NEC or Samsung32 got decoded into address+command. "raw" is just
// timing data in microseconds, for protocols the flipper didn't recognize.
struct IrSignal {
    std::string name;
    bool raw;

    std::string protocol;
    uint32_t address;
    uint32_t command;

    uint32_t freq;    // only for raw, usually 38000
    float dutyCycle;
    std::vector<uint32_t> timings;

    IrSignal() : raw(false), address(0), command(0), freq(0), dutyCycle(0) {}
};

struct IrRemote {
    std::string category; // TVs, ACs, Fans, etc (top-level folder)
    std::string brand;
    std::string model;    // stem of the .ir filename
    std::string path;
    std::vector<IrSignal> buttons;
};

int loadFlipperIRDB(const std::string &rootDir, std::vector<IrRemote> &out);

// search the db for any remote that has a button matching this decoded signal
std::vector<int> matchParsed(const std::vector<IrRemote> &db,
    const std::string &proto, uint32_t addr, uint32_t cmd);

// same but for raw timing data. tolerance is 0-1, like 0.25 = 25% wiggle
std::vector<int> matchRaw(const std::vector<IrRemote> &db,
    const std::vector<uint32_t> &timings, float tolerance);

const IrSignal *lookupBtn(const IrRemote &remote, const std::string &name);

uint32_t flipperHexToU32(const std::string &hex);
