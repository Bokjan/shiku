#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <unistd.h>
#include <dirent.h>
#include <stdexcept>
#include <sys/types.h>
#include "Utility.hpp"
#include "ShikuDB.hpp"
#include "Literals.hpp"
#include "ClientAPI.hpp"
using std::string;
extern shiku::ShikuDB dbmgr;
namespace shiku::API
{
	void CreateDatabase(Json &query, Json &ret)
	{
		if(query.find("database") == query.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = SHIKUDB_ERROR_MESSAGE_REQUIRED_FIELD_NOT_PROVIDED;
			return;
		}
		// Check if db name has a duplicate
		if(dbmgr.DBs.find(query["database"]) != dbmgr.DBs.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Duplicated database name";
			return;
		}
		string dbname = query["database"];
		char dbpath[512];
		sprintf(dbpath, "%s%s.db/", dbmgr.root, dbname.c_str());
		if(!Utility::MakeDirectory(dbpath))
		{
			Log.Warn("Fail to create directory `%s`", dbpath);
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Fail to create new database";
			return;
		}
		try
		{
			dbmgr.DBs.insert(std::make_pair(dbname, shiku::CreateDatabase(dbname.c_str(), dbpath)));
		}
		catch(std::runtime_error e)
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Failed to create new database";
			return;
		}
		Log.Info("New database %s created and loaded", dbname.c_str());
		ret["ok"] = true;
	}
	void DropDatabase(Json &query, Json &ret)
	{
		if(query.find("database") == query.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = SHIKUDB_ERROR_MESSAGE_REQUIRED_FIELD_NOT_PROVIDED;
			return;
		}
		// 1st: Destruct DbfsMgr
		string dbname = query["database"];
		auto itr = dbmgr.DBs.find(dbname);
		if(itr == dbmgr.DBs.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Database not found";
			return;
		}
		// itr->second.~DbfsManager();
		dbmgr.DBs.erase(itr);
		// 2nd: Delete files
		char dbpath[512];
		sprintf(dbpath, "%s%s.db/", dbmgr.root, dbname.c_str());
		// TODO: Cannot delete normally on windows
#ifdef __WIN32
		int len = strlen(dbpath);
		for(int i = 0; i < len; ++i)
			if(dbpath[i] == '/')
				dbpath[i] = '\\';
		char command[768];
		sprintf(command, "rmdir /s /q %s", dbpath);
		Log.Debug("Running command: %s", command);
		system(command);
#else
		DIR *root_dir;
		dirent *iterator;
		char filepath[512];
		if((root_dir = opendir(dbpath)) == nullptr)
		{
			Log.Warn("Cannot access %s when deleting %s", dbpath, dbname.c_str());
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Fail to delete files";
			return;
		}
		while((iterator = readdir(root_dir)) != nullptr)
		{
			char *fname = iterator->d_name;
			if(Utility::IsEndsWith(fname, ".")
				|| Utility::IsEndsWith(fname, ".."))
				continue;
			sprintf(filepath, "%s%s", dbpath, fname);
			if(remove(filepath) == 0)
				Log.Info("File %s deleted", filepath);
		}
		if(remove(dbpath) != 0)
		{
			Log.Warn("Database %s removed from dbmgr but files not deleted", dbname.c_str());
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Fail to delete files";
			return;
		}
#endif
		Log.Warn("Database %s deleted", dbname.c_str());
		ret["ok"] = true;
	}
	void CreateCollection(Json &query, Json &ret)
	{
		if(query.find("database") == query.end() || 
			query.find("collection") == query.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = SHIKUDB_ERROR_MESSAGE_REQUIRED_FIELD_NOT_PROVIDED;
			return;
		}
		string database = query["database"];
		string collection = query["collection"];
		if(dbmgr.DBs.find(database) == dbmgr.DBs.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Database not found";
			return;
		}
		if(dbmgr.DBs.find(collection) != dbmgr.DBs.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Duplicated collection name";
			return;
		}
		// Create new meta
		DbfsManager &mgr = dbmgr.DBs[database];
		// Open addressing
		uint32_t hashpos = Utility::BkdrHash(collection.c_str()) % DbfsManager::MAX_META_SIZE;
		while(mgr.metas[hashpos].Name[0] != '\0') // This slot is used
			++hashpos;
		strcpy(mgr.metas[hashpos].Name, collection.c_str());
		mgr.metas[hashpos].FirstRecord = DiskLoc();
		// Map it
		mgr.Metamap.insert(std::make_pair(collection, mgr.metas + hashpos));
		*mgr.MetadataCount += 1;
		ret["ok"] = true;
	}
	void ShowCollections(Json &query, Json &ret)
	{
		if(query.find("database") == query.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = SHIKUDB_ERROR_MESSAGE_REQUIRED_FIELD_NOT_PROVIDED;
			return;
		}
		string database = query["database"];
		if(dbmgr.DBs.find(database) == dbmgr.DBs.end())
		{
			ret[SHIKUDB_JSON_FIELD_MESSAGE] = "Database not found";
			return;
		}
		DbfsManager &mgr = dbmgr.DBs[database];
		ret["collections"] = Json::array();
		for(auto &i : mgr.Metamap)
			if(i.first[0] != '$')
				ret["collections"].push_back(i.first);
		ret["ok"] = true;
	}
}