#include <string>
#include <iostream>
#include <iomanip>
#include <memory>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <vector>
#include <mutex>

enum class LogLevel {
    GRAPHING = 0,
    INFO = 1,
	DEBUG = 2,
	WARN = 3,
    myERROR = 4,
};


class Logger{

    public:
        void SetPrefs(std::string logFileName, LogLevel level, std::vector<std::string> input_columns);
        void Log(std::string input, LogLevel level);
        void LogGraph(std::string input, std::string column);
        static std::shared_ptr<Logger> GetInstance(); //pointer of logger instance

        //temp Counter variables
        int tempCounterFramesIn = 0;
        int tempCounterFramesOut = 0;


    private:
        LogLevel logLevel;
        std::ofstream logFile;
        int64_t startTime;
        std::vector<std::string> columns;

        static std::shared_ptr<Logger> loggerInstance;
        void FileOutput(const std::string& message);
        std::string CurrentTime();
        std::string millisecondsToTimeFormat(int64_t milliseconds);

        std::string queueGraph();
};
