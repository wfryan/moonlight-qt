#include <string>
#include <iostream>
#include <iomanip>
#include <memory>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>

enum class LogLevel {
    GRAPHING = 0,
    INFO = 1,
	  DEBUG = 2,
	  WARN = 3,
    myERROR = 4,
};


class Logger{

    public:
        void SetPrefs(std::string logFileName, LogLevel level);
        void Log(std::string input, LogLevel level);
        static std::shared_ptr<Logger> GetInstance(); //pointer of logger instance


    private:
        LogLevel logLevel;
        std::ofstream logFile;
        int64_t startTime;

        static std::shared_ptr<Logger> loggerInstance;
        void FileOutput(const std::string& message);
        std::string CurrentTime();
        std::string millisecondsToTimeFormat(int64_t milliseconds);
};
