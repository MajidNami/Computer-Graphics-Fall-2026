// CityLab: the shared city of the Computer Graphics course project.
//
//     citylab                 walk through the whole city
//     citylab --team team07   start at your own plot
//     citylab --help          every option and key

#include <cstdio>
#include <stdexcept>

#include "app/Application.h"
#include "app/Checker.h"
#include "engine/Options.h"

#ifndef CITYLAB_VERSION
#define CITYLAB_VERSION "dev"
#endif

int main(int argc, char** argv) {
    using namespace citylab;
    try {
        const Options options = parseOptions(argc, argv);
        if (options.showHelp) {
            std::printf("%s", optionsHelp().c_str());
            return 0;
        }
        if (options.showVersion) {
            std::printf("CityLab %s\n", CITYLAB_VERSION);
            return 0;
        }
        const auto root = findRoot(options.root, argc > 0 ? argv[0] : nullptr);
        if (!options.checkTeam.empty()) return runChecker(options, root);
        Application app(options, root);
        return options.list ? app.list() : app.run();
    } catch (const std::invalid_argument& e) {
        std::fprintf(stderr, "citylab: %s\n", e.what());
        return 2;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "citylab: error: %s\n", e.what());
        return 1;
    }
}
