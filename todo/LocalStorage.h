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

#ifndef LOCALSTORAGE_H__
#define LOCALSTORAGE_H__

#include "RoRPrerequisites.h"
#include <angelscript.h>
#include "ImprovedConfigFile.h"

void registerLocalStorage(AngelScript::asIScriptEngine *engine);
void scriptLocalStorageFactory_Generic(AngelScript::asIScriptGeneric *gen);
void scriptLocalStorageFactory2_Generic(AngelScript::asIScriptGeneric *gen);
void scriptLocalStorageFactory3_Generic(AngelScript::asIScriptGeneric *gen);

/**
 *  @brief A class that allows scripts to store data persistently
 */
class LocalStorage : public Ogre::ImprovedConfigFile
{	
public:
	// Memory management
	void AddRef() const;
	void Release() const;

	LocalStorage(AngelScript::asIScriptEngine *engine, std::string fileName_in,  const std::string &sectionName_in);
	LocalStorage(AngelScript::asIScriptEngine *engine_in);
	~LocalStorage();

	LocalStorage &operator =(LocalStorage &other);
	void changeSection(const std::string &section);
		
	std::string get(std::string &key);
	void set(std::string &key, const std::string &value);

	int getInt(std::string &key);
	void set(std::string &key, const int value);

	float getFloat(std::string &key);
	void set(std::string &key, const float value);

	bool getBool(std::string &key);
	void set(std::string &key, const bool value);

	Ogre::Vector3 getVector3(std::string &key);
	void set(std::string &key, const Ogre::Vector3 &value);

	Ogre::Quaternion getQuaternion(std::string &key);
	void set(std::string &key, const Ogre::Quaternion &value);

	Ogre::Radian getRadian(std::string &key);
	void set(std::string &key, const Ogre::Radian &value);

	Ogre::Degree getDegree(std::string &key);
	void set(std::string &key, const Ogre::Degree &value);

	void saveDict();
	// int extendDict();
	bool loadDict();
	
	// removes a key and its associated value
	void eraseKey(std::string &key);
	
    // Returns true if the key is set
    bool exists(std::string &key);

    // Deletes all keys
    void deleteAll();
	
	// parses a key
	void parseKey(std::string &key, std::string &section);

	// Garbage collections behaviours
	int GetRefCount();
	void SetGCFlag();
	bool GetGCFlag();
	void EnumReferences(AngelScript::asIScriptEngine *engine);
	void ReleaseAllReferences(AngelScript::asIScriptEngine *engine);
	
	SettingsBySection getSettings() { return mSettings; }
	std::string getFilename() { return filename; }
	std::string getSection() { return sectionName; }

protected:
	bool saved;
	std::string sectionName;
	
	// Our properties
    AngelScript::asIScriptEngine *engine;
    mutable int refCount;
};

#endif // LOCALSTORAGE_H__
