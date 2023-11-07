#include "logger.h"

std::shared_ptr<Logger> Logger::loggerInstance;

void Logger::SetPrefs(std::string logFileName, LogLevel level){

    logLevel = level;

	using namespace std::chrono;

	milliseconds ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

	startTime = ms.count();

    std::string input = logFileName + "/" + millisecondsToTimeFormat(startTime) + ".txt";
    

    logFile.open(input);
		if (!logFile.good()) {
			std::cerr << "Can't Open Log File at " << input << std::endl;
		} else {
			std::cout << "Log File Loaded successfully!" << std::endl;
		}
}

std::shared_ptr<Logger> Logger::GetInstance(){
    if(loggerInstance == nullptr){
        loggerInstance = std::shared_ptr<Logger>(new Logger());

    }
    return loggerInstance;
}

void Logger::Log(std::string input, LogLevel messageLevel){

    if(messageLevel <= logLevel){
        std::string logType;
        switch (messageLevel) {
		case LogLevel::DEBUG:
			logType = "DEBUG: ";
			break;
		case LogLevel::INFO:
			logType = "INFO: ";
			break;
		case LogLevel::WARN:
			logType = "WARN: ";
			break;
		case LogLevel::ERROR:
			logType = "ERROR: ";
			break;
		default:
			logType = "NONE: ";
			break;
		}
        FileOutput((CurrentTime() + " - " + logType + input));
    }

    
}

void Logger::FileOutput(const std::string& message){
    logFile << message << std::endl;
}

// std::string Logger::CurrentTime(){
// 	const std::time_t now = std::time(nullptr);
//     const std::tm calendar_time = *std::localtime( std::addressof(now) );

// 	return std::to_string(calendar_time.tm_hour) + ":" + std::to_string(calendar_time.tm_min) + ":" + std::to_string(calendar_time.tm_sec);
// }

std::string Logger::CurrentTime(){
	using namespace std::chrono;

	milliseconds ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

	std::string output = millisecondsToTimeFormat(ms.count() - startTime);
	return output;

}

std::string Logger::millisecondsToTimeFormat(int64_t milliseconds) {
    // int hours = milliseconds / 3600000;
    int minutes = (milliseconds % 3600000) / 60000;
    int seconds = (milliseconds % 60000) / 1000;
    int ms = milliseconds % 1000;

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << seconds << "."
       << std::setfill('0') << std::setw(3) << ms;

    return ss.str();
}
