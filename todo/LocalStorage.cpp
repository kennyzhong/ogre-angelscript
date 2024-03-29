/*
-----------------------------------------------------------------------------
This source file is part of OGRE-angelscript

For the latest info, see http://code.google.com/p/ogre-angelscript/

Copyright (c) 2006-2011 Thomas Fischer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "LocalStorage.h"
#include "Settings.h"

/* class that implements the localStorage interface for the scripts */
LocalStorage::LocalStorage(AngelScript::asIScriptEngine *engine_in, std::string fileName_in, const std::string &sectionName_in)
{
	this->engine = engine_in;
	engine->NotifyGarbageCollectorOfNewObject(this, engine->GetTypeIdByDecl("LocalStorage"));

	// inversed logic, better use a whiteliste instead of a blacklist, so you are on the safe side ;) - tdev
	std::string allowedChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
	for (std::string::iterator it = fileName_in.begin() ; it < fileName_in.end() ; ++it){
		if( allowedChars.find(*it) == std::string::npos )
			*it = '_';
	}

	sectionName = sectionName_in.substr(0, sectionName_in.find(".", 0));
	
	filename = SSETTING("Cache Path") + fileName_in + ".asdata";
	separators = "=";
	loadDict();
	
	saved = true;
}

LocalStorage::LocalStorage(AngelScript::asIScriptEngine *engine_in)
{
	this->engine = engine_in;
	engine->NotifyGarbageCollectorOfNewObject(this, engine->GetTypeIdByDecl("LocalStorage"));	
	saved = true;
}

LocalStorage::~LocalStorage()
{
	// save everything
	saveDict();
}

void LocalStorage::AddRef() const
{
	// We need to clear the GC flag
	refCount = (refCount & 0x7FFFFFFF) + 1;
}

void LocalStorage::Release() const
{
	// We need to clear the GC flag
	refCount = (refCount & 0x7FFFFFFF) - 1;
	if( refCount == 0 )
		delete this;
}

int LocalStorage::GetRefCount()
{
	return refCount & 0x7FFFFFFF;
}

void LocalStorage::SetGCFlag()
{
	refCount |= 0x80000000;
}

bool LocalStorage::GetGCFlag()
{
	return (refCount & 0x80000000) ? true : false;
}

void LocalStorage::EnumReferences(AngelScript::asIScriptEngine *engine){}
void LocalStorage::ReleaseAllReferences(AngelScript::asIScriptEngine * /*engine*/){}

LocalStorage &LocalStorage::operator =(LocalStorage &other)
{
	filename = other.getFilename();
	sectionName = other.getSection();
	SettingsBySection::iterator secIt;
	SettingsBySection osettings = other.getSettings();
	for(secIt = osettings.begin(); secIt!=osettings.end(); secIt++)
	{
		SettingsMultiMap::iterator setIt;
		for(setIt = secIt->second->begin(); setIt!=secIt->second->end(); setIt++)
		{
			setSetting(setIt->first, setIt->second, secIt->first);
		}
	}
	return *this;
}

void LocalStorage::changeSection(const std::string &section)
{
	sectionName = section.substr(0, section.find(".", 0));
}

// getters and setters
std::string LocalStorage::get(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return getSetting(key, sec);
}

void LocalStorage::set(std::string &key, const std::string &value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, value, sec);
	saved = false;
}

int LocalStorage::getInt(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return getSettingInt(key, sec);
}

void LocalStorage::set(std::string &key, const int value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, value, sec);
	saved = false;
}

float LocalStorage::getFloat(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return getSettingReal(key, sec);
}

void LocalStorage::set(std::string &key, const float value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, value, sec);
	saved = false;
}

bool LocalStorage::getBool(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return getSettingBool(key, sec);
}

void LocalStorage::set(std::string &key, const bool value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, value, sec);
	saved = false;
}

Ogre::Vector3 LocalStorage::getVector3(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return getSettingVector3(key, sec);
}

void LocalStorage::set(std::string &key, const Ogre::Vector3 &value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, value, sec);
	saved = false;
}

Ogre::Quaternion LocalStorage::getQuaternion(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return getSettingQuaternion(key, sec);
}

void LocalStorage::set(std::string &key, const Ogre::Quaternion &value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, value, sec);
	saved = false;
}

Ogre::Radian LocalStorage::getRadian(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return getSettingRadian(key, sec);
}

void LocalStorage::set(std::string &key, const Ogre::Radian &value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, value, sec);
	saved = false;
}

Ogre::Degree LocalStorage::getDegree(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return Ogre::Degree(getSettingRadian(key, sec));
}

void LocalStorage::set(std::string &key, const Ogre::Degree &value)
{
	std::string sec;
	parseKey(key, sec);
	setSetting(key, Ogre::Radian(value), sec);
	saved = false;
}

void LocalStorage::saveDict()
{
	if(!saved && save())
		saved = true;
}

bool LocalStorage::loadDict()
{
	std::ifstream ifile(filename.c_str());
	if( !ifile )
		return false;

	load(filename);
	saved = true;
	return true;
}

void LocalStorage::eraseKey(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	if(mSettings.find(sec) != mSettings.end() && mSettings[sec]->find(key) != mSettings[sec]->end())
		if(mSettings[sec]->erase(key) > 0)
			saved = false;
}

void LocalStorage::deleteAll()
{
	// SLOG("This feature is not available yet");
}

void LocalStorage::parseKey(std::string &key, std::string &section)
{
	size_t dot = key.find(".", 0);
	if( dot != std::string::npos )
	{
		section = key.substr(0, dot);

		if( !section.length() )
			section = sectionName;

		key.erase(0, dot+1);
	}
	else
		section = sectionName;
}

bool LocalStorage::exists(std::string &key)
{
	std::string sec;
	parseKey(key, sec);
	return hasSetting(key, sec);
}

void scriptLocalStorageFactory_Generic(AngelScript::asIScriptGeneric *gen)
{
	std::string filename	= **(std::string**)gen->GetAddressOfArg(0);
	std::string sectionname = **(std::string**)gen->GetAddressOfArg(1);
	
	*(LocalStorage**)gen->GetAddressOfReturnLocation() = new LocalStorage(gen->GetEngine(), filename, sectionname);
}

void scriptLocalStorageFactory2_Generic(AngelScript::asIScriptGeneric *gen)
{
	std::string filename	= **(std::string**)gen->GetAddressOfArg(0);	
	*(LocalStorage**)gen->GetAddressOfReturnLocation() = new LocalStorage(gen->GetEngine(), filename, "common");
}

void scriptLocalStorageFactory3_Generic(AngelScript::asIScriptGeneric *gen)
{	
	*(LocalStorage**)gen->GetAddressOfReturnLocation() = new LocalStorage(gen->GetEngine());
}

void registerLocalStorage(AngelScript::asIScriptEngine *engine)
{
	int r;

	r = engine->RegisterObjectType("LocalStorage", sizeof(LocalStorage), AngelScript::asOBJ_REF | AngelScript::asOBJ_GC); MYASSERT( r >= 0 );
	// Use the generic interface to construct the object since we need the engine pointer, we could also have retrieved the engine pointer from the active context
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_FACTORY, "LocalStorage@ f(const string &in, const string &in)", AngelScript::asFUNCTION(scriptLocalStorageFactory_Generic), AngelScript::asCALL_GENERIC); MYASSERT( r>= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_FACTORY, "LocalStorage@ f(const string &in)", AngelScript::asFUNCTION(scriptLocalStorageFactory2_Generic), AngelScript::asCALL_GENERIC); MYASSERT( r>= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_FACTORY, "LocalStorage@ f()", AngelScript::asFUNCTION(scriptLocalStorageFactory3_Generic), AngelScript::asCALL_GENERIC); MYASSERT( r>= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_ADDREF, "void f()",  AngelScript::asMETHOD(LocalStorage,AddRef), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_RELEASE, "void f()", AngelScript::asMETHOD(LocalStorage,Release), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "LocalStorage &opAssign(LocalStorage &in)", AngelScript::asMETHODPR(LocalStorage, operator=, (LocalStorage &), LocalStorage&), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void changeSection(const string &in)",     AngelScript::asMETHODPR(LocalStorage,changeSection,(const std::string&), void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "string get(string &in)",                 AngelScript::asMETHODPR(LocalStorage,get,(std::string&), std::string), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "string getString(string &in)",           AngelScript::asMETHODPR(LocalStorage,get,(std::string&), std::string), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, const string &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const std::string&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setString(string &in, const string &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const std::string&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "float getFloat(string &in)",  AngelScript::asMETHODPR(LocalStorage,getFloat,(std::string&), float), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, float)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const float),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setFloat(string &in, float)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const float),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "vector3 getVector3(string &in)",          AngelScript::asMETHODPR(LocalStorage,getVector3,(std::string&), Ogre::Vector3), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, const vector3 &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Vector3&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setVector3(string &in, const vector3 &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Vector3&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "radian getRadian(string &in)",           AngelScript::asMETHODPR(LocalStorage,getRadian,(std::string&), Ogre::Radian), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, const radian &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Radian&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setRadian(string &in, const radian &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Radian&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "degree getDegree(string &in)",           AngelScript::asMETHODPR(LocalStorage,getDegree,(std::string&), Ogre::Degree), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, const degree &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Degree&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setDegree(string &in, const degree &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Degree&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "quaternion getQuaternion(string &in)",       AngelScript::asMETHODPR(LocalStorage,getQuaternion,(std::string&), Ogre::Quaternion), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, const quaternion &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Quaternion&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setQuaternion(string &in, const quaternion &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const Ogre::Quaternion&),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "bool getBool(string &in)",             AngelScript::asMETHODPR(LocalStorage,getBool,(std::string&), bool), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, const bool &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const bool),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setBool(string &in, const bool &in)", AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const bool),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "int getInt(string &in)",     AngelScript::asMETHODPR(LocalStorage,getInt,(std::string&), int), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "int getInteger(string &in)", AngelScript::asMETHODPR(LocalStorage,getInt,(std::string&), int), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void set(string &in, int)",  AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const int),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setInt(string &in, int)",  AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const int),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void setInteger(string &in, int)",  AngelScript::asMETHODPR(LocalStorage,set,(std::string&, const int),void), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "void save()",   AngelScript::asMETHOD(LocalStorage,saveDict), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "bool reload()", AngelScript::asMETHOD(LocalStorage,loadDict), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	r = engine->RegisterObjectMethod("LocalStorage", "bool exists(string &in) const", AngelScript::asMETHOD(LocalStorage,exists), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectMethod("LocalStorage", "void delete(string &in)",       AngelScript::asMETHOD(LocalStorage,eraseKey), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

	// Register GC behaviours
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_GETREFCOUNT, "int f()", AngelScript::asMETHOD(LocalStorage,GetRefCount), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_SETGCFLAG, "void f()",  AngelScript::asMETHOD(LocalStorage,SetGCFlag), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_GETGCFLAG, "bool f()",  AngelScript::asMETHOD(LocalStorage,GetGCFlag), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_ENUMREFS, "void f(int&in)", AngelScript::asMETHOD(LocalStorage,EnumReferences), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );
	r = engine->RegisterObjectBehaviour("LocalStorage", AngelScript::asBEHAVE_RELEASEREFS, "void f(int&in)", AngelScript::asMETHOD(LocalStorage,ReleaseAllReferences), AngelScript::asCALL_THISCALL); MYASSERT( r >= 0 );

}
