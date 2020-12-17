#ifndef SETTINGS_H
#define SETTINGS_H

#include <map>
#include <list>

class Settings
{
private:
    struct SettingsData {
        SettingsData(const std::string arg, const std::string argShort, const std::string help)
            : argName(arg)
            , argNameShort(argShort)
            , helpText(help) {

        }
        std::string argName;
        std::string argNameShort;
        std::string helpText;
    };

public:
    static Settings& getInstance() {
        static Settings instance;
        return instance;
    }

private:
    Settings();
    Settings(Settings const&);
    void operator=(Settings const&);

public:
    void parseArgs(int argc, char **argv);
    std::string getHelp() const;

public:
    std::string getInputFilePath() const;
    std::string getTlePath() const;

    std::string getResourcesPath() const;
    std::string getOutputPath() const;

private:
    std::map<std::string, std::string> mArgs;
    std::list<SettingsData> mSettingsList;
};

#endif // SETTINGS_H
