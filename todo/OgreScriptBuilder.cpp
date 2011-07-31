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
#include "OgreScriptBuilder.h"

#include <string>
#include <Ogre.h>

using namespace std;
using namespace Ogre;

// OgreScriptBuilder
int OgreScriptBuilder::LoadScriptSection(const char *filename)
{
	// Open the script file
	string scriptFile = filename;

	DataStreamPtr ds;
	try
	{
		ds = ResourceGroupManager::getSingleton().openResource(scriptFile, ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);

	} catch(Ogre::Exception e)
	{
		LOG("exception upon loading script file: " + e.getFullDescription());
		return -1;
	}

	// Read the entire file
	string code;
	code.resize(ds->size());
	ds->read(&code[0], ds->size());

	// TODO: fix the script hashes
	/*
	// using SHA1 here is stupid, we need to replace it with something better
	// then hash it
	char hash_result[250];
	memset(hash_result, 0, 249);
	RoR::CSHA1 sha1;
	sha1.UpdateHash((uint8_t *)script.c_str(), script.size());
	sha1.Final();
	sha1.ReportHash(hash_result, RoR::CSHA1::REPORT_HEX_SHORT);
	hash = string(hash_result);
	*/

	return ProcessScriptSection(code.c_str(), filename);
}
