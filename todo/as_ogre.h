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
#ifndef AS_OGRE_H_
#define AS_OGRE_H_

#include "RoRPrerequisites.h"

#include "angelscript.h"
#include "Ogre.h"

// This function will register the following objects with the scriptengine:
//    - Ogre::Vector3
//    - Ogre::Radian
//    - Ogre::Degree
//    - Ogre::Quaternion
void registerOgreObjects(AngelScript::asIScriptEngine *engine);

// The following functions shouldn't be called directly!
// Use the registerOgreObjects function above instead.
void registerOgreVector3(AngelScript::asIScriptEngine *engine);
void registerOgreRadian(AngelScript::asIScriptEngine *engine);
void registerOgreDegree(AngelScript::asIScriptEngine *engine);
void registerOgreQuaternion(AngelScript::asIScriptEngine *engine);

#endif //AS_OGRE_H_
