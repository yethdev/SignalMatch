#include "ir_db.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace fs = std::filesystem;


uint32_t flipperHexToU32(const std::string &hex) {
    // flipper stores addresses/commands as little-endian hex bytes
    // e.g. "07 00 00 00" -> 0x00000007
    uint32_t result = 0;
    int bytePos = 0;
    std::istringstream in(hex);
    std::string byte;
    while (in >> byte && bytePos < 4) {
        result |= (strtoul(byte.c_str(), NULL, 16) << (bytePos * 8));
        bytePos++;
    }
    return result;
}


// rip one .ir file into an IrRemote. most of the flipper IRDB files follow
// the same layout but some have weirdly placed comments or blank lines,
// hence all the defensive checks
static IrRemote readIrFile(const std::string &fpath,
                            const std::string &cat,
                            const std::string &brand)
{
    IrRemote rem;
    rem.category = cat;
    rem.brand    = brand;
    rem.path     = fpath;
    rem.model    = fs::path(fpath).stem().string();

    std::ifstream file(fpath);
    if (!file.is_open())
        return rem;

    IrSignal sig;
    bool haveSig = false;
    std::string ln;

    while (std::getline(file, ln)) {
        // strip whitespace on both ends
        while (!ln.empty() && (ln.back() == ' ' || ln.back() == '\r' || ln.back() == '\t'))
            ln.pop_back();
        size_t start = ln.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (start > 0) ln = ln.substr(start);

        if (ln[0] == '#') continue;
        if (ln.rfind("Filetype:", 0) == 0) continue;
        if (ln.rfind("Version:", 0) == 0) continue;

        size_t colon = ln.find(':');
        if (colon == std::string::npos) continue;

        std::string key = ln.substr(0, colon);
        std::string val = ln.substr(colon + 1);
        // trim leading space off val
        if (!val.empty() && val[0] == ' ') val = val.substr(1);

        if (key == "name") {
            if (haveSig && sig.name.length() > 0)
                rem.buttons.push_back(sig);
            sig = IrSignal();
            sig.name = val;
            haveSig = true;

        } else if (key == "type") {
            sig.raw = (val == "raw");

        } else if (key == "protocol") {
            sig.protocol = val;

        } else if (key == "address") {
            sig.address = flipperHexToU32(val);

        } else if (key == "command") {
            sig.command = flipperHexToU32(val);

        } else if (key == "frequency") {
            sig.freq = atoi(val.c_str());

        } else if (key == "duty_cycle") {
            sig.dutyCycle = atof(val.c_str());

        } else if (key == "data") {
            // raw timing data, space separated microsecond values
            std::istringstream ds(val);
            uint32_t t;
            while (ds >> t)
                sig.timings.push_back(t);
        }
    }

    if (haveSig && sig.name.length() > 0)
        rem.buttons.push_back(sig);

    return rem;
}


int loadFlipperIRDB(const std::string &rootDir, std::vector<IrRemote> &out)
{
    if (!fs::exists(rootDir)) {
        fprintf(stderr, "!! irdb root doesn't exist: %s\n", rootDir.c_str());
        return -1;
    }

    int cnt = 0;

    // walk every .ir file. the IRDB is organized Category/Brand/file.ir
    // but some entries are only Category/file.ir (no brand subfolder)
    for (auto &ent : fs::recursive_directory_iterator(rootDir))
    {
        if (!ent.is_regular_file()) continue;
        if (ent.path().extension() != ".ir") continue;

        fs::path rel = fs::relative(ent.path(), rootDir);
        std::string cat = "";
        std::string brand = "";

        // pull category and brand from the relative path
        auto parentPath = rel.parent_path();
        auto seg = parentPath.begin();
        if (seg != parentPath.end()) { cat = seg->string(); seg++; }
        if (seg != parentPath.end()) { brand = seg->string(); }

        // skip hidden/internal dirs like _Converted_ or .github
        if (cat.length() > 0 && (cat[0] == '.' || cat[0] == '_'))
            continue;

        IrRemote rem = readIrFile(ent.path().string(), cat, brand);
        if (rem.buttons.size() > 0) {
            out.push_back(rem);
            cnt++;
        }
    }

    return cnt;
}


std::vector<int> matchParsed(const std::vector<IrRemote> &db,
    const std::string &proto, uint32_t addr, uint32_t cmd)
{
    std::vector<int> results;

    for (int i = 0; i < (int)db.size(); i++) {
        bool found = false;
        for (int b = 0; b < (int)db[i].buttons.size() && !found; b++) {
            const IrSignal &s = db[i].buttons[b];
            if (s.raw) continue;
            if (s.protocol == proto && s.address == addr && s.command == cmd) {
                results.push_back(i);
                found = true;
            }
        }
    }
    return results;
}


std::vector<int> matchRaw(const std::vector<IrRemote> &db,
    const std::vector<uint32_t> &timings, float tolerance)
{
    std::vector<int> results;
    if (timings.empty()) return results;

    for (int i = 0; i < (int)db.size(); i++) {
        for (const IrSignal &s : db[i].buttons) {
            if (!s.raw || s.timings.size() != timings.size())
                continue;

            // check each pulse/space pair is within tolerance
            bool match = true;
            for (size_t j = 0; j < timings.size(); j++) {
                float a = (float)s.timings[j];
                float b = (float)timings[j];
                float bigger = std::max(a, b);
                if (bigger > 0 && fabsf(a - b) / bigger > tolerance) {
                    match = false;
                    break;
                }
            }

            if (match) {
                results.push_back(i);
                break; // one hit per remote is enough
            }
        }
    }
    return results;
}


const IrSignal *lookupBtn(const IrRemote &remote, const std::string &name) {
    for (size_t i = 0; i < remote.buttons.size(); i++) {
        if (remote.buttons[i].name == name)
            return &remote.buttons[i];
    }
    return NULL;
}
