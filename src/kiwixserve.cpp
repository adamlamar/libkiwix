#include "kiwixserve.h"
#include "subprocess.h"

#ifdef _WIN32
    # define KIWIXSERVE_CMD "kiwix-serve.exe"
#else
    # define KIWIXSERVE_CMD "kiwix-serve"
#endif

namespace kiwix {

KiwixServe::KiwixServe() : m_port(8181)
{
}

KiwixServe::~KiwixServe()
{
    shutDown();
}

void KiwixServe::run()
{
    #ifdef _WIN32
        int pid = GetCurrentProcessId();
    #else
        pid_t pid = getpid();

    #endif

    std::vector<const char*> callCmd;
    std::string kiwixServeCmd = appendToDirectory(
        removeLastPathElement(getExecutablePath(), true, true),
        KIWIXSERVE_CMD);
    if (fileExists(kiwixServeCmd)) {
        // A local kiwix-serve exe exists (packaged with kiwix-desktop), use it.
        callCmd.push_back(kiwixServeCmd.c_str());
    } else {
        // Try to use a potential installed kiwix-serve.
        callCmd.push_back(KIWIXSERVE_CMD);
    }
    std::string libraryPath = getDataDirectory() + "/library.xml";
    std::string attachProcessOpt = "-a" + to_string(pid);
    std::string portOpt = "-p" + to_string(m_port);
    callCmd.push_back(attachProcessOpt.c_str());
    callCmd.push_back(portOpt.c_str());
    callCmd.push_back("-l");
    callCmd.push_back(libraryPath.c_str());
    mp_kiwixServe = Subprocess::run(callCmd);
}

void KiwixServe::shutDown()
{
    if (mp_kiwixServe)
        mp_kiwixServe->kill();
}

}