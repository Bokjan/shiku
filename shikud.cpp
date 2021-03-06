#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include "Json.hpp"
#include "ShikuDB.hpp"
#include "Utility.hpp"
#include "HttpServer.hpp"
#include "DbfsManager.hpp"
using Json = nlohmann::json;
using shiku::Log;
Json config;
shiku::ShikuDB dbmgr;
shiku::HttpServer *hs;
void ParseArgs(int argc, char *argv[]);
void InitDB(int, char *[]);
void InitHttpServer(int, char *[]);
int main(int argc, char *argv[])
{
	Log.SetLevel(shiku::LogClass::Level::All);
	std::function<void(int, char*[])> Delegates[] = 
	{
		ParseArgs, InitDB, InitHttpServer
	};
	for(auto fun : Delegates)
		fun(argc, argv);
	Log.Info("Shutting down server");
	delete hs;
	return 0;
}
void ParseArgs(int argc, char *argv[])
{
	for(int i = 0; i < argc; ++i)
	{
		// Parameter header
		if(argv[i][0] != '-')
			continue;
		// -c	Config File
		// -p	Port
		switch(argv[i][1])
		{
			case 'c': // Config File
			{
				if(!shiku::Utility::IsFileExists(argv[++i]))
				{
					Log.Fatal("Cannot open config file");
					throw std::runtime_error("Cannot open config file");
				}
				try
				{
					std::ifstream in(argv[i]);
					in >> config;
					in.close();
				}
				catch(std::invalid_argument e)
				{
					Log.Warn("Configuration JSON parse failed");
					// throw std::runtime_error("Configuration parse failed");
				}
				config["root"] += '/';
				break;
			}
			case 'p': // Port
			{
				int port;
				sscanf(argv[++i], "%d", &port);
				config["port"] = port;
				break;
			}
		}
	}
}
void InitDB(int argc, char *argv[])
{
	// Check if `config.root` exists
	if(config.find("root") == config.end())
	{
		Log.Fatal("Root path not set");
		throw std::runtime_error("Root path not set");
	}
	dbmgr.SetRoot(config["root"]);
	dbmgr.Initialize();
}
void InitHttpServer(int argc, char *argv[])
{
	if(config.find("port") != config.end())
		hs = new shiku::HttpServer((int)config["port"]);
	else
		hs = new shiku::HttpServer(6207);
	hs->Run();
}