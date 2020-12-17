#include "settings.h"
#include <sstream>

#if defined(_MSC_VER)
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

Settings::Settings()
{
    mSettingsList.push_back(SettingsData("--help",  "-h", "Print help"));
    mSettingsList.push_back(SettingsData("--tle",   "-t", "TLE file required for pass calculation"));
    mSettingsList.push_back(SettingsData("--input", "-i", "Input S file containing softbits"));
}

void Settings::parseArgs(int argc, char **argv)
{
    for(int i = 1; i < (argc-1); i+=2) {
        mArgs.insert(std::make_pair(argv[i], argv[i+1]));
    }
}

std::string Settings::getHelp() const
{
    std::list<SettingsData>::const_iterator it;
    std::stringstream ss;

    for(it = mSettingsList.begin(); it != mSettingsList.end(); ++it) {
        ss << (*it).argNameShort << "\t" << (*it).argName << "\t" << (*it).helpText << std::endl;
    }

    return ss.str();
}

std::string Settings::getInputFilePath() const
{
    if(mArgs.count("-i")) {
        return mArgs.at("-i");
    }
    if(mArgs.count("--input")) {
        return mArgs.at("--input");
    }
    return std::string();
}

std::string Settings::getTlePath() const
{
    if(mArgs.count("-t")) {
        return mArgs.at("-t");
    }
    if(mArgs.count("--tle")) {
        return mArgs.at("--tle");
    }
    return std::string();
}

std::string Settings::getResourcesPath() const
{
#if defined(_MSC_VER)
    CHAR path[MAX_PATH];

	GetModuleFileNameA(nullptr, path, MAX_PATH);
	PathRemoveFileSpecA(path);

    return std::string(path) + "\\resources\\";
#else
    struct passwd *pw = getpwuid(getuid());
    return std::string(pw->pw_dir) + "/meteordemod/";
#endif
}

std::string Settings::getOutputPath() const
{
    if(mArgs.count("-o")) {
        return mArgs.at("-o");
    }
    if(mArgs.count("--output")) {
        return mArgs.at("--output");
    }
    return std::string("./");
}
