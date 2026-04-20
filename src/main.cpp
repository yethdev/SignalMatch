// made by yeth | MIT license
#include "ir_db.h"
#include <cstdio>
#include <string>

// just a test harness for now. fakes a received IR signal, searches the
// flipper IRDB for matches, pretends the user picked one, and dumps
// the available buttons. none of the actual hardware stuff yet.

int main(int argc, char *argv[]) {
    const char *dbPath = "./Flipper-IRDB";
    if (argc > 1)
        dbPath = argv[1];

    printf("loading IRDB from %s ...\n", dbPath);

    std::vector<IrRemote> db;
    int n = loadFlipperIRDB(dbPath, db);
    if (n < 0) {
        printf("failed to load irdb, check path\n");
        return 1;
    }

    // count total buttons across all remotes (just curious)
    int totalBtns = 0;
    for (int i = 0; i < (int)db.size(); i++)
        totalBtns += db[i].buttons.size();
    printf("got %d remotes, %d buttons total\n\n", n, totalBtns);


    // simulate receiving a samsung TV power button
    // Samsung32 protocol, address 0x07, command 0x02 is Power on most samsung TVs
    std::string rxProto = "Samsung32";
    uint32_t rxAddr = 0x07;
    uint32_t rxCmd  = 0x02;

    printf("simulated RX: proto=%s addr=0x%02X cmd=0x%02X\n",
           rxProto.c_str(), rxAddr, rxCmd);

    auto hits = matchParsed(db, rxProto, rxAddr, rxCmd);
    printf("found %d matching remote(s):\n", (int)hits.size());

    // only show first 20, there could be tons of samsung models
    int cap = (int)hits.size();
    if (cap > 20) cap = 20;

    for (int i = 0; i < cap; i++) {
        IrRemote &r = db[hits[i]];
        printf("  %2d) [%s] %s / %s  (%d btns)\n",
               i + 1, r.category.c_str(), r.brand.c_str(),
               r.model.c_str(), (int)r.buttons.size());
    }
    if ((int)hits.size() > 20)
        printf("  ... +%d more\n", (int)hits.size() - 20);


    // === pretend user selected the first match ===
    if (hits.empty()) {
        printf("no matches, nothing to do\n");
        return 0;
    }

    IrRemote &picked = db[hits[0]];
    printf("\nuser picked: %s %s\n", picked.brand.c_str(), picked.model.c_str());
    printf("buttons on this remote:\n");

    for (size_t i = 0; i < picked.buttons.size(); i++) {
        IrSignal &btn = picked.buttons[i];
        if (btn.raw) {
            printf("  %-18s  [raw %uHz, %d samples]\n",
                   btn.name.c_str(), btn.freq, (int)btn.timings.size());
        } else {
            printf("  %-18s  %s addr=0x%02X cmd=0x%02X\n",
                   btn.name.c_str(), btn.protocol.c_str(),
                   btn.address, btn.command);
        }
    }

    // try to send Vol_up
    const IrSignal *volUp = lookupBtn(picked, "Vol_up");
    if (volUp) {
        printf("\n[TX] Vol_up -> %s addr=0x%02X cmd=0x%02X\n",
               volUp->protocol.c_str(), volUp->address, volUp->command);
    } else {
        // TODO: not all remotes use the same button names, the flipper
        // IRDB naming scheme has Vol_up but some older files use VOL+ etc
        printf("\nno Vol_up button on this remote\n");
    }

    return 0;
}
