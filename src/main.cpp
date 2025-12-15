#include <stdio.h>
#include <aixlog/aixlog.hpp>
#include <iostream>
#include <string>
#include <cstdlib>
#include "CameraManager.h"
#include "UploadQueue.h"
#include "HttpTransport.h"
#include "UploadItem.h"
#include "Defaults.h"
#include "ApplicationConfig.h"
#include "UINotifier.h"
#include "USBDownload.h"

static json app_config;

struct Args {
	std::string config;
	int verbosity = 0;
	bool has_config = false;
	bool has_verbosity = false;
	bool show_help = false;
	bool show_gui = false;
};

Args parseArgs(int argc, char* argv[]) {
	Args args;
    
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
        
		if (arg == "-h" || arg == "--help") {
			args.show_help = true;
			return args;  // Return immediately when help is requested
		}
		else if (arg == "-c" || arg == "--config") {
			if (i + 1 >= argc) {
				throw std::runtime_error(arg + " requires a string argument");
			}
			args.config = argv[i + 1];
			args.has_config = true;
			i++;
		}
		else if (arg == "-v" || arg == "--verbosity") {
			if (i + 1 >= argc) {
				throw std::runtime_error(arg + " requires an integer argument");
			}
            
			char* endptr;
			long val = std::strtol(argv[i + 1], &endptr, 7);
            
			if (*endptr != '\0') {
				throw std::runtime_error(arg + " argument must be a valid integer");
			}

			if(val > 6 || val < 0) {
				args.has_verbosity = false;
				std::cout << "Invalid log level: " << static_cast<int>(val) << ". Defaulting to " << SC3_LOG_LEVEL << "." << std::endl;
			} else {
				args.verbosity = static_cast<int>(val);
				args.has_verbosity = true;
			}
            
			i++;
		} else if (arg == "-g" || arg == "--gui") {
			args.show_gui = true;
		}
		else {
			throw std::runtime_error("Unknown option: " + arg);
		}
	}
    
	return args;
}

void printHelp() {
	const char* program_name = "SC3 Camera Connector";
	std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
						<< "Options:\n"
						<< "  -c, --config <string>      Configuration file or string\n"
						<< "  -v, --verbosity <int>      Verbosity level (0-6)\n"
						<< "  -h, --help                 Show this help message\n"
						<< "\n"
						<< "Log Levels:\n"
						<< "0 = TRACE\n"
						<< "1 = DEBUG\n"
						<< "2 = INFO\n"
						<< "3 = NOTICE\n"
						<< "4 = WARNING\n"
						<< "5 = ERROR\n"
						<< "6 = FATAL\n"
						<< "Examples:\n"
						<< "  " << program_name << " -c config.txt -v 2\n"
						<< "  " << program_name << " --config myconfig --verbosity 5\n"
						<< std::endl;
}

int main(int argc, char** argv)
{
	Args args = parseArgs(argc, argv);
	if(args.show_help) {
		printHelp();
		return 0;
	}
	// Init logger
	AixLog::Log::init<AixLog::SinkCout>(static_cast<AixLog::Severity>(args.has_verbosity ? args.verbosity : SC3_LOG_LEVEL));

	// Get app config
	app_config = get_app_config(args.has_config ? args.config : SC3_CONFIG_PATH);

	std::string server_url = app_config["server_url"];
	bool enable_http = app_config["enable_http_upload"];
	bool enable_usb = app_config["enable_usb_download"];
	// Start upload queue
	UploadQueue upload_queue(new HttpTransport(server_url), 3, enable_http, enable_usb);
	upload_queue.Start();

	std::string socket_path = app_config["ui_socket_path"];
	UINotifier ui_notifier(socket_path);
	ui_notifier.Start();

	CameraManager camera_manager(0, true, args.show_gui);
	bool status = camera_manager.Start(true);

	camera_manager.Stop();
	// Stop upload queue & ui notifier if camera manager stops
	ui_notifier.Stop();
	upload_queue.Stop();
	return status ? 0 : -1;
}

//TODO: Graceful shutdown
