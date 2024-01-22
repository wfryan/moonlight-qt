#include "logger.h"

//shared pointer instance of logger
std::shared_ptr<Logger> Logger::loggerInstance;

std::mutex print_mutex;

//sets up ordinary log file
void Logger::SetPrefs(std::string logFileName, LogLevel level, std::vector<std::string> input_columns){

	

    logLevel = level;

	using namespace std::chrono;

	//assigns columns for graphing setup 
	columns = input_columns;
	

	milliseconds ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

	startTime = ms.count();

    std::string input = logFileName + "/" + std::to_string(startTime);

	if(logLevel == LogLevel::GRAPHING){
		input+=".csv";
	} else{
		input+=".txt";
	}
    

    logFile.open(input);
	if (!logFile.good()) {
		std::cerr << "Can't Open Log File at " << input << std::endl;
	} else {
        std::cout << "Log File Loaded successfully!" << std::endl;
		if(logLevel == LogLevel::GRAPHING){
            std::string columnNames = "time (ms)";
			for(int i = 0; i < input_columns.size(); i++){
				columnNames += ",";
				columnNames += input_columns[i];
			}
			columnNames += "\n";
			FileOutput(columnNames);
		}
		
		


	}
}

//necessary function to get singleton instance
std::shared_ptr<Logger> Logger::GetInstance(){
    if(loggerInstance == nullptr){
        loggerInstance = std::shared_ptr<Logger>(new Logger());

    }
    return loggerInstance;
}

//function that creates a proper log output line
void Logger::Log(std::string input, LogLevel messageLevel){

    if(messageLevel <= logLevel){
        std::string logType;
        switch (messageLevel) {
		case LogLevel::GRAPHING:
			logType = "GRAPHING: ";
			break;
		case LogLevel::DEBUG:
			logType = "DEBUG: ";
			break;
		case LogLevel::INFO:
			logType = "INFO: ";
			break;
		case LogLevel::WARN:
			logType = "WARN: ";
			break;
        case LogLevel::myERROR:
			logType = "ERROR: ";
			break;
		
		default:
			logType = "NONE: ";
			break;
		}

		FileOutput((CurrentTime() + " - " + logType + input));
    }
}

//sets up graphing inputs
void Logger::LogGraph(std::string input, std::string column){
	using namespace std::chrono;
	milliseconds ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch());
	std::string output = std::to_string(ms.count() - startTime);
	
	for(int i = 0; i < columns.size(); i++){
		output += ",";
		if(columns[i] == column){
			output+=input;
		}
	}

	FileOutput(output);
}

//enters proper log message into file
void Logger::FileOutput(const std::string& message){
	print_mutex.lock();
    logFile << message << std::endl;
	print_mutex.unlock();
}

//calculates current time in ms
std::string Logger::CurrentTime(){
	using namespace std::chrono;

	milliseconds ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

	std::string output = millisecondsToTimeFormat(ms.count() - startTime);
	return output;

}

//converts milliseconds into minutes, seconds, milliseconds
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

