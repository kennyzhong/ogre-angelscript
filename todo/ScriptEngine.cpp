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

#include "ScriptEngine.h"
#include "Ogre.h"
#include "RoRFrameListener.h"
#include "BeamFactory.h"

// AS addons start
#include "scriptstdstring/scriptstdstring.h"
#include "scriptmath/scriptmath.h"
#include "contextmgr/contextmgr.h"
#include "scriptany/scriptany.h"
#include "scriptarray/scriptarray.h"
#include "scripthelper/scripthelper.h"
#include "scriptstring/scriptstring.h"
// AS addons end

#ifdef USE_CURL
#define CURL_STATICLIB
#include <stdio.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#endif //USE_CURL

#include "rornet.h"
#include "water.h"
#include "Beam.h"
#include "Settings.h"
#include "Console.h"
#include "OverlayWrapper.h"
#include "SkyManager.h"
#include "CacheSystem.h"
#include "as_ogre.h"
#include "LocalStorage.h"
#include "SelectorWindow.h"
#include "sha1.h"
#include "collisions.h"

#include "GameScript.h"
#include "OgreScriptBuilder.h"
#include "CBytecodeStream.h"
#include "ScriptEvents.h"

//using namespace Ogre;
//using namespace std;
//using namespace AngelScript;

template<> ScriptEngine *Ogre::Singleton<ScriptEngine>::ms_Singleton=0;

char *ScriptEngine::moduleName = "RoRScript";


// some hacky functions

void logString(const std::string &str)
{
	SLOG(str);
}

// the class implementation

ScriptEngine::ScriptEngine(RoRFrameListener *efl, Collisions *_coll) : mefl(efl), coll(_coll), engine(0), context(0), frameStepFunctionPtr(-1), wheelEventFunctionPtr(-1), eventCallbackFunctionPtr(-1), defaultEventCallbackFunctionPtr(-1), eventMask(0), terrainScriptName(), terrainScriptHash(), scriptLog(0)
{
	callbacks["on_terrain_loading"] = std::vector<int>();
	callbacks["frameStep"] = std::vector<int>();
	callbacks["wheelEvents"] = std::vector<int>();
	callbacks["eventCallback"] = std::vector<int>();

	enable_ingame_console = BSETTING("Enable Ingame Console");

	// create our own log
	scriptLog = LogManager::getSingleton().createLog(SSETTING("Log Path")+"/Angelscript.log", false);
	
	if(enable_ingame_console) 
		scriptLog->addListener(this);
	
	scriptLog->logMessage("ScriptEngine initialized");

	// init not earlier, otherwise crash
	init();
}

ScriptEngine::~ScriptEngine()
{
	// Clean up
	if(engine)  engine->Release();
	if(context) context->Release();
}

void ScriptEngine::messageLogged( const Ogre::String& message, Ogre::LogMessageLevel lml, bool maskDebug, const Ogre::String &logName )
{
	Console *c = Console::getInstancePtrNoCreation();
	if(c) c->printUTF(message, "Angelscript");
}

void ScriptEngine::ExceptionCallback(AngelScript::asIScriptContext *ctx, void *param)
{
	AngelScript::asIScriptEngine *engine = ctx->GetEngine();
	int funcID = ctx->GetExceptionFunction();
	const AngelScript::asIScriptFunction *function = engine->GetFunctionDescriptorById(funcID);
	SLOG("--- exception ---");
	SLOG("desc: " + String(ctx->GetExceptionString()));
	SLOG("func: " + String(function->GetDeclaration()));
	SLOG("modl: " + String(function->GetModuleName()));
	SLOG("sect: " + String(function->GetScriptSectionName()));
	int col, line = ctx->GetExceptionLineNumber(&col);
	SLOG("line: "+TOSTRING(line)+","+TOSTRING(col));

	// Print the variables in the current function
	//PrintVariables(ctx, -1);

	// Show the call stack with the variables
	SLOG("--- call stack ---");
	char tmp[2048]="";
    for( AngelScript::asUINT n = 1; n < ctx->GetCallstackSize(); n++ )
    {
    	function = ctx->GetFunction(n);
		sprintf(tmp, "%s (%d): %s\n", function->GetScriptSectionName(), ctx->GetLineNumber(n), function->GetDeclaration());
		SLOG(String(tmp));
		//PrintVariables(ctx, n);
    }
}

void ScriptEngine::exploreScripts()
{
	// this shouldnt be used atm
	return;
#if USE_ANGELSCRIPT
	FileInfoListPtr files= ResourceGroupManager::getSingleton().findResourceFileInfo("Scripts", "*.rs", false);
	for (FileInfoList::iterator iterFiles = files->begin(); iterFiles!= files->end(); ++iterFiles)
	{
		loadScript(iterFiles->filename);
	}
#endif //USE_ANGELSCRIPT
}

void ScriptEngine::LineCallback(AngelScript::asIScriptContext *ctx, unsigned long *timeOut)
{
	// If the time out is reached we abort the script
	if(OgreFramework::getSingleton().getTimeSinceStartup() > *timeOut)
		ctx->Abort();

	// It would also be possible to only suspend the script,
	// instead of aborting it. That would allow the application
	// to resume the execution where it left of at a later 
	// time, by simply calling Execute() again.
}

/*
void ScriptEngine::PrintVariables(asIScriptContext *ctx, int stackLevel)
{
	char tmp[1024]="";
	asIScriptEngine *engine = ctx->GetEngine();

	int typeId = ctx->GetThisTypeId(stackLevel);
	void *varPointer = ctx->GetThisPointer(stackLevel);
	if( typeId )
	{
		sprintf(tmp," this = %p", varPointer);
		SLOG(tmp);
	}

	int numVars = ctx->GetVarCount(stackLevel);
	for( int n = 0; n < numVars; n++ )
	{
		int typeId = ctx->GetVarTypeId(n, stackLevel);
		void *varPointer = ctx->GetAddressOfVar(n, stackLevel);
		if( typeId == engine->GetTypeIdByDecl("int") )
		{
			sprintf(tmp, " %s = %d", ctx->GetVarDeclaration(n, stackLevel), *(int*)varPointer);
			SLOG(tmp);
		}
		else if( typeId == engine->GetTypeIdByDecl("string") )
		{
			std::string *str = (std::string*)varPointer;
			if( str )
			{
				sprintf(tmp, " %s = '%s'", ctx->GetVarDeclaration(n, stackLevel), str->c_str());
				SLOG(tmp);
			} else
			{
				sprintf(tmp, " %s = <null>", ctx->GetVarDeclaration(n, stackLevel));
				SLOG(tmp);
			}
		SLOG(tmp);
		}
	}
};
*/

// continue with initializing everything
void ScriptEngine::init()
{
	SLOG("ScriptEngine (SE) initializing ...");
	int result;
	// Create the script engine
	engine = AngelScript::asCreateScriptEngine(ANGELSCRIPT_VERSION);

	// Set the message callback to receive information on errors in human readable form.
	// It's recommended to do this right after the creation of the engine, because if
	// some registration fails the engine may send valuable information to the message
	// stream.
	result = engine->SetMessageCallback(AngelScript::asMETHOD(ScriptEngine,msgCallback), this, AngelScript::asCALL_THISCALL);
	if(result < 0)
	{
		if(result == AngelScript::asINVALID_ARG)
		{
			SLOG("One of the arguments is incorrect, e.g. obj is null for a class method.");
			return;
		} else if(result == AngelScript::asNOT_SUPPORTED)
		{
			SLOG("	The arguments are not supported, e.g. asCALL_GENERIC.");
			return;
		}
		SLOG("Unkown error while setting up message callback");
		return;
	}

	// AngelScript doesn't have a built-in string type, as there is no definite standard
	// string type for C++ applications. Every developer is free to register it's own string type.
	// The SDK do however provide a standard add-on for registering a string type, so it's not
	// necessary to register your own string type if you don't want to.
	AngelScript::RegisterScriptArray(engine, true);
	AngelScript::RegisterStdString(engine);
	AngelScript::RegisterScriptMath(engine);
	AngelScript::RegisterScriptAny(engine);
	AngelScript::RegisterScriptDictionary(engine);
	//AngelScript::RegisterScriptString(engine);
	//AngelScript::RegisterScriptStringUtils(engine);

	// register some Ogre objects like the vector3 and the quaternion
	registerOgreObjects(engine);

	// Register the local storage object.
	// This needs to be done after the registration of the ogre objects!
	registerLocalStorage(engine);

	// some useful global functions
	result = engine->RegisterGlobalFunction("void log(const string &in)", AngelScript::asFUNCTION(logString), AngelScript::asCALL_CDECL); MYASSERT( result >= 0 );
	result = engine->RegisterGlobalFunction("void print(const string &in)", AngelScript::asFUNCTION(logString), AngelScript::asCALL_CDECL); MYASSERT( result >= 0 );

	// Register everything
	// class Beam
	result = engine->RegisterObjectType("BeamClass", sizeof(Beam), AngelScript::asOBJ_REF); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void scaleTruck(float)", AngelScript::asMETHOD(Beam,scaleTruck), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "string getTruckName()", AngelScript::asMETHOD(Beam,getTruckName), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void reset(bool)", AngelScript::asMETHOD(Beam,reset), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void setDetailLevel(int)", AngelScript::asMETHOD(Beam,setDetailLevel), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void showSkeleton(bool, bool)", AngelScript::asMETHOD(Beam,showSkeleton), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void hideSkeleton(bool)", AngelScript::asMETHOD(Beam,hideSkeleton), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void parkingbrakeToggle()", AngelScript::asMETHOD(Beam,parkingbrakeToggle), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void tractioncontrolToggle()", AngelScript::asMETHOD(Beam,tractioncontrolToggle), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void antilockbrakeToggle()", AngelScript::asMETHOD(Beam,antilockbrakeToggle), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void beaconsToggle()", AngelScript::asMETHOD(Beam,beaconsToggle), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void setReplayMode(bool)", AngelScript::asMETHOD(Beam,setReplayMode), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void resetAutopilot()", AngelScript::asMETHOD(Beam,resetAutopilot), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void toggleCustomParticles()", AngelScript::asMETHOD(Beam,toggleCustomParticles), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "float getDefaultDeformation()", AngelScript::asMETHOD(Beam,getDefaultDeformation), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "int getNodeCount()", AngelScript::asMETHOD(Beam,getNodeCount), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "float getTotalMass(bool)", AngelScript::asMETHOD(Beam,getTotalMass), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "int getWheelNodeCount()", AngelScript::asMETHOD(Beam,getWheelNodeCount), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void recalc_masses()", AngelScript::asMETHOD(Beam,recalc_masses), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void setMass(float)", AngelScript::asMETHOD(Beam,setMass), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "bool getBrakeLightVisible()", AngelScript::asMETHOD(Beam,getBrakeLightVisible), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "bool getCustomLightVisible(int)", AngelScript::asMETHOD(Beam,getCustomLightVisible), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void setCustomLightVisible(int, bool)", AngelScript::asMETHOD(Beam,setCustomLightVisible), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "bool getBeaconMode()", AngelScript::asMETHOD(Beam,getBeaconMode), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "void setBlinkType(int)", AngelScript::asMETHOD(Beam,setBlinkType), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "int getBlinkType()", AngelScript::asMETHOD(Beam,getBlinkType), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "bool getCustomParticleMode()", AngelScript::asMETHOD(Beam,getCustomParticleMode), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "int getLowestNode()", AngelScript::asMETHOD(Beam,getLowestNode), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "bool setMeshVisibility(bool)", AngelScript::asMETHOD(Beam,setMeshVisibility), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "bool getReverseLightVisible()", AngelScript::asMETHOD(Beam,getCustomParticleMode), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "float getHeadingDirectionAngle()", AngelScript::asMETHOD(Beam,getHeadingDirectionAngle), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "bool isLocked()", AngelScript::asMETHOD(Beam,isLocked), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("BeamClass", "float getWheelSpeed()", AngelScript::asMETHOD(Beam,getWheelSpeed), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);

	
	/*
	// impossible to use offsetof for derived classes
	// unusable, read http://www.angelcode.com/angelscript/sdk/docs/manual/doc_adv_class_hierarchy.html

	result = engine->RegisterObjectProperty("BeamClass", "float WheelSpeed", offsetof(Beam, WheelSpeed)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float brake", offsetof(Beam, brake)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float currentScale", offsetof(Beam, currentScale)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int nodedebugstate", offsetof(Beam, nodedebugstate)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int debugVisuals", offsetof(Beam, debugVisuals)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool networking", offsetof(Beam, networking)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int label", offsetof(Beam, label)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int trucknum", offsetof(Beam, trucknum)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int skeleton", offsetof(Beam, skeleton)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool replaymode", offsetof(Beam, replaymode)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int replaylen", offsetof(Beam, replaylen)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int replaypos", offsetof(Beam, replaypos)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool cparticle_enabled", offsetof(Beam, cparticle_enabled)); MYASSERT(result>=0);
	//result = engine->RegisterObjectProperty("BeamClass", "int hookId", offsetof(Beam, hookId)); MYASSERT(result>=0);
	//result = engine->RegisterObjectProperty("BeamClass", "BeamClass @lockTruck", offsetof(Beam, lockTruck)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int free_node", offsetof(Beam, free_node)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int dynamicMapMode", offsetof(Beam, dynamicMapMode)); MYASSERT(result>=0);
	//result = engine->RegisterObjectProperty("BeamClass", "int tied", offsetof(Beam, tied)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int canwork", offsetof(Beam, canwork)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int hashelp", offsetof(Beam, hashelp)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float minx", offsetof(Beam, minx)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float maxx", offsetof(Beam, maxx)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float miny", offsetof(Beam, miny)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float maxy", offsetof(Beam, maxy)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float minz", offsetof(Beam, minz)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float maxz", offsetof(Beam, maxz)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int state", offsetof(Beam, state)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int sleepcount", offsetof(Beam, sleepcount)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int driveable", offsetof(Beam, driveable)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int importcommands", offsetof(Beam, importcommands)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool requires_wheel_contact", offsetof(Beam, requires_wheel_contact)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool wheel_contact_requested", offsetof(Beam, wheel_contact_requested)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool rescuer", offsetof(Beam, rescuer)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int parkingbrake", offsetof(Beam, parkingbrake)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int antilockbrake", offsetof(Beam, antilockbrake)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int tractioncontrol", offsetof(Beam, tractioncontrol)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int lights", offsetof(Beam, lights)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int smokeId", offsetof(Beam, smokeId)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int editorId", offsetof(Beam, editorId)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float leftMirrorAngle", offsetof(Beam, leftMirrorAngle)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float refpressure", offsetof(Beam, refpressure)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int free_pressure_beam", offsetof(Beam, free_pressure_beam)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int done_count", offsetof(Beam, done_count)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int free_prop", offsetof(Beam, free_prop)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float default_beam_diameter", offsetof(Beam, default_beam_diameter)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float skeleton_beam_diameter", offsetof(Beam, skeleton_beam_diameter)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int free_aeroengine", offsetof(Beam, free_aeroengine)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float elevator", offsetof(Beam, elevator)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float rudder", offsetof(Beam, rudder)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float aileron", offsetof(Beam, aileron)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int flap", offsetof(Beam, flap)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int free_wing", offsetof(Beam, free_wing)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float fadeDist", offsetof(Beam, fadeDist)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool disableDrag", offsetof(Beam, disableDrag)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int currentcamera", offsetof(Beam, currentcamera)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int freecinecamera", offsetof(Beam, freecinecamera)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float brakeforce", offsetof(Beam, brakeforce)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool ispolice", offsetof(Beam, ispolice)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int loading_finished", offsetof(Beam, loading_finished)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int freecamera", offsetof(Beam, freecamera)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int first_wheel_node", offsetof(Beam, first_wheel_node)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int netbuffersize", offsetof(Beam, netbuffersize)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int nodebuffersize", offsetof(Beam, nodebuffersize)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "float speedoMax", offsetof(Beam, speedoMax)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool useMaxRPMforGUI", offsetof(Beam, useMaxRPMforGUI)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "string realtruckfilename", offsetof(Beam, realtruckfilename)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int free_wheel", offsetof(Beam, free_wheel)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int airbrakeval", offsetof(Beam, airbrakeval)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int cameranodecount", offsetof(Beam, cameranodecount)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "int free_cab", offsetof(Beam, free_cab)); MYASSERT(result>=0);
	// wont work: result = engine->RegisterObjectProperty("BeamClass", "int airbrakeval", offsetof(Beam, airbrakeval)); MYASSERT(result>=0);
	result = engine->RegisterObjectProperty("BeamClass", "bool meshesVisible", offsetof(Beam, meshesVisible)); MYASSERT(result>=0);
	*/

	result = engine->RegisterObjectBehaviour("BeamClass", AngelScript::asBEHAVE_ADDREF, "void f()", AngelScript::asMETHOD(Beam,addRef), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectBehaviour("BeamClass", AngelScript::asBEHAVE_RELEASE, "void f()", AngelScript::asMETHOD(Beam,release), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);

	// class Settings
	result = engine->RegisterObjectType("SettingsClass", sizeof(Settings), AngelScript::asOBJ_REF); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("SettingsClass", "string getSetting(const string &in)", AngelScript::asMETHOD(Settings,getSettingScriptSafe), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("SettingsClass", "void setSetting(const string &in, const string &in)", AngelScript::asMETHOD(Settings,setSettingScriptSafe), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectBehaviour("SettingsClass", AngelScript::asBEHAVE_ADDREF, "void f()", AngelScript::asMETHOD(Settings,addRef), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectBehaviour("SettingsClass", AngelScript::asBEHAVE_RELEASE, "void f()", AngelScript::asMETHOD(Settings,release), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);

	// TODO: add Vector3 classes and other utility classes!

	// class GameScript
	result = engine->RegisterObjectType("GameScriptClass", sizeof(GameScript), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void log(const string &in)", AngelScript::asMETHOD(GameScript,log), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "double getTime()", AngelScript::asMETHOD(GameScript,getTime), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setPersonPosition(vector3)", AngelScript::asMETHOD(GameScript,setPersonPosition), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void loadTerrain(const string &in)", AngelScript::asMETHOD(GameScript,loadTerrain), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "vector3 getPersonPosition()", AngelScript::asMETHOD(GameScript,getPersonPosition), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void movePerson(float, float, float)", AngelScript::asMETHOD(GameScript,movePerson), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "string getCaelumTime()", AngelScript::asMETHOD(GameScript,getCaelumTime), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setCaelumTime(float)", AngelScript::asMETHOD(GameScript,setCaelumTime), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setWaterHeight(float)", AngelScript::asMETHOD(GameScript,setWaterHeight), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "float getWaterHeight()", AngelScript::asMETHOD(GameScript,getWaterHeight), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "float getGroundHeight(vector3)", AngelScript::asMETHOD(GameScript,getGroundHeight), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int getCurrentTruckNumber()", AngelScript::asMETHOD(GameScript,getCurrentTruckNumber), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void boostCurrentTruck(float)", AngelScript::asMETHOD(GameScript, boostCurrentTruck), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int getNumTrucks()", AngelScript::asMETHOD(GameScript,getNumTrucks), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "float getGravity()", AngelScript::asMETHOD(GameScript,getGravity), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setGravity(float)", AngelScript::asMETHOD(GameScript,setGravity), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void flashMessage(const string &in, float, float)", AngelScript::asMETHOD(GameScript,flashMessage), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setDirectionArrow(const string &in, vector3)", AngelScript::asMETHOD(GameScript,setDirectionArrow), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void hideDirectionArrow()", AngelScript::asMETHOD(GameScript,hideDirectionArrow), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void registerForEvent(int)", AngelScript::asMETHOD(GameScript,registerForEvent), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "BeamClass @getCurrentTruck()", AngelScript::asMETHOD(GameScript,getCurrentTruck), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "BeamClass @getTruckByNum(int)", AngelScript::asMETHOD(GameScript,getTruckByNum), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int getChatFontSize()", AngelScript::asMETHOD(GameScript,getChatFontSize), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setChatFontSize(int)", AngelScript::asMETHOD(GameScript,setChatFontSize), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void showChooser(const string &in, const string &in, const string &in)", AngelScript::asMETHOD(GameScript,showChooser), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void repairVehicle(const string &in, const string &in, bool)", AngelScript::asMETHOD(GameScript,repairVehicle), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void removeVehicle(const string &in, const string &in)", AngelScript::asMETHOD(GameScript,removeVehicle), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void spawnObject(const string &in, const string &in, vector3, vector3, const string &in, bool)", AngelScript::asMETHOD(GameScript,spawnObject), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void destroyObject(const string &in)", AngelScript::asMETHOD(GameScript,destroyObject), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int setMaterialAmbient(const string &in, float, float, float)", AngelScript::asMETHOD(GameScript,setMaterialAmbient), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int setMaterialDiffuse(const string &in, float, float, float, float)", AngelScript::asMETHOD(GameScript,setMaterialDiffuse), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int setMaterialSpecular(const string &in, float, float, float, float)", AngelScript::asMETHOD(GameScript,setMaterialSpecular), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int setMaterialEmissive(const string &in, float, float, float)", AngelScript::asMETHOD(GameScript,setMaterialEmissive), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int getNumTrucksByFlag(int)", AngelScript::asMETHOD(GameScript,getNumTrucksByFlag), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "bool getCaelumAvailable()", AngelScript::asMETHOD(GameScript,getCaelumAvailable), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void startTimer()", AngelScript::asMETHOD(GameScript,startTimer), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "float stopTimer()", AngelScript::asMETHOD(GameScript,stopTimer), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "float rangeRandom(float, float)", AngelScript::asMETHOD(GameScript,rangeRandom), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int useOnlineAPI(const string &in, const dictionary &in, string &out)", AngelScript::asMETHOD(GameScript,useOnlineAPI), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "int getLoadedTerrain(string &out)", AngelScript::asMETHOD(GameScript,getLoadedTerrain), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void clearEventCache()", AngelScript::asMETHOD(GameScript,clearEventCache), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);

	result = engine->RegisterObjectMethod("GameScriptClass", "void setCameraPosition(vector3)",  AngelScript::asMETHOD(GameScript,setCameraPosition),  AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setCameraDirection(vector3)", AngelScript::asMETHOD(GameScript,setCameraDirection), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setCameraYaw(float)",         AngelScript::asMETHOD(GameScript,setCameraYaw),       AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setCameraPitch(float)",       AngelScript::asMETHOD(GameScript,setCameraPitch),     AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void setCameraRoll(float)",        AngelScript::asMETHOD(GameScript,setCameraRoll),      AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "vector3 getCameraPosition()",      AngelScript::asMETHOD(GameScript,getCameraPosition),  AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "vector3 getCameraDirection()",     AngelScript::asMETHOD(GameScript,getCameraDirection), AngelScript::asCALL_THISCALL); MYASSERT(result>=0);
	result = engine->RegisterObjectMethod("GameScriptClass", "void cameraLookAt(vector3)",       AngelScript::asMETHOD(GameScript,cameraLookAt),       AngelScript::asCALL_THISCALL); MYASSERT(result>=0);

	// enum scriptEvents
	result = engine->RegisterEnum("scriptEvents"); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_COLLISION_BOX_ENTER", SE_COLLISION_BOX_ENTER); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_COLLISION_BOX_LEAVE", SE_COLLISION_BOX_LEAVE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_ENTER", SE_TRUCK_ENTER); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_EXIT", SE_TRUCK_EXIT); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_ENGINE_DIED", SE_TRUCK_ENGINE_DIED); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_ENGINE_FIRE", SE_TRUCK_ENGINE_FIRE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_TOUCHED_WATER", SE_TRUCK_TOUCHED_WATER); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_BEAM_BROKE", SE_TRUCK_BEAM_BROKE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_LOCKED", SE_TRUCK_LOCKED); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_UNLOCKED", SE_TRUCK_UNLOCKED); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_LIGHT_TOGGLE", SE_TRUCK_LIGHT_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_SKELETON_TOGGLE", SE_TRUCK_SKELETON_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_TIE_TOGGLE", SE_TRUCK_TIE_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_PARKINGBREAK_TOGGLE", SE_TRUCK_PARKINGBREAK_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_TRACTIONCONTROL_TOGGLE", SE_TRUCK_TRACTIONCONTROL_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_ANTILOCKBRAKE_TOGGLE", SE_TRUCK_ANTILOCKBRAKE_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_BEACONS_TOGGLE", SE_TRUCK_BEACONS_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_CPARTICLES_TOGGLE", SE_TRUCK_CPARTICLES_TOGGLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_TRUCK_GROUND_CONTACT_CHANGED", SE_TRUCK_GROUND_CONTACT_CHANGED); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_GENERIC_NEW_TRUCK", SE_GENERIC_NEW_TRUCK); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_GENERIC_DELETED_TRUCK", SE_GENERIC_DELETED_TRUCK); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_GENERIC_INPUT_EVENT", SE_GENERIC_INPUT_EVENT); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_GENERIC_MOUSE_BEAM_INTERACTION", SE_GENERIC_MOUSE_BEAM_INTERACTION); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("scriptEvents", "SE_ALL_EVENTS", SE_ALL_EVENTS); MYASSERT(result>=0);
	

	result = engine->RegisterEnum("truckStates"); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_ACTIVATED", ACTIVATED); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_DESACTIVATED", DESACTIVATED); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_MAYSLEEP", MAYSLEEP); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_GOSLEEP", GOSLEEP); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_SLEEPING", SLEEPING); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_NETWORKED", NETWORKED); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_RECYCLE", RECYCLE); MYASSERT(result>=0);
	result = engine->RegisterEnumValue("truckStates", "TS_DELETED", DELETED); MYASSERT(result>=0);

	// now the global instances
	GameScript *gamescript = new GameScript(this, mefl);
	result = engine->RegisterGlobalProperty("GameScriptClass game", gamescript); MYASSERT(result>=0);
	//result = engine->RegisterGlobalProperty("CacheSystemClass cache", &CacheSystem::Instance()); MYASSERT(result>=0);
	result = engine->RegisterGlobalProperty("SettingsClass settings", &SETTINGS); MYASSERT(result>=0);

	SLOG("Type registrations done. If you see no error above everything should be working");
}

void ScriptEngine::msgCallback(const AngelScript::asSMessageInfo *msg)
{
	const char *type = "Error";
	if( msg->type == AngelScript::asMSGTYPE_INFORMATION )
		type = "Info";
	else if( msg->type == AngelScript::asMSGTYPE_WARNING )
		type = "Warning";

	char tmp[1024]="";
	sprintf(tmp, "%s (%d, %d): %s = %s", msg->section, msg->row, msg->col, type, msg->message);
	SLOG(tmp);
}

int ScriptEngine::framestep(Ogre::Real dt)
{

#if 0
	Beam **trucks = BeamFactory::getSingleton().getTrucks();
	int free_truck = BeamFactory::getSingleton().getTruckCount();
	// check for all truck wheels
	if(coll && wheelEventFunctionPtr > 0)
	{
		for(int t = 0; t < free_truck; t++)
		{
			Beam *b = trucks[t];
			if (!b || b->state == SLEEPING || b->state == NETWORKED || b->state == RECYCLE)
				continue;

			bool allwheels=true;
			int handlerid=-1;
			for(int w = 0; w < b->free_wheel; w++)
			{
				if(handlerid == -1 && b->wheels[w].lastEventHandler != -1)
				{
					handlerid = b->wheels[w].lastEventHandler;
					continue;
				}else if(b->wheels[w].lastEventHandler != handlerid)
				{
					allwheels=false;
					break;
				}
			}

			// if the truck is in there, call the functions
			if(allwheels && handlerid >= 0 && handlerid < MAX_EVENTSOURCE)
			{
				eventsource_t *source = coll->getEvent(handlerid);
				if(!engine) return 0;
				if(!context) context = engine->CreateContext();
				context->Prepare(wheelEventFunctionPtr);

				// Set the function arguments
				context->SetArgFloat (0, t);
				context->SetArgObject(1, &std::string("wheels"));
				context->SetArgObject(2, &std::string(source->instancename));
				context->SetArgObject(3, &std::string(source->boxname));

				//SLOG("Executing framestep()");
				int r = context->Execute();
				if( r == AngelScript::asEXECUTION_FINISHED )
				{
				  // The return value is only valid if the execution finished successfully
					AngelScript::asDWORD ret = context->GetReturnDWord();
				}
			}
		}
	}

	// check if current truck is in an event box
	Beam *truck = BeamFactory::getSingleton().getCurrentTruck();
	if(truck)
	{
		eventsource_t *source = coll->isTruckInEventBox(truck);
		if(source) envokeCallback(source->scripthandler, source, 0, 1);
	}

#endif // 0

	// framestep stuff below
	if(frameStepFunctionPtr<=0) return 1;
	if(!engine) return 0;
	if(!context) context = engine->CreateContext();
	context->Prepare(frameStepFunctionPtr);

	// Set the function arguments
	context->SetArgFloat(0, dt);

	//SLOG("Executing framestep()");
	int r = context->Execute();
	if( r == AngelScript::asEXECUTION_FINISHED )
	{
	  // The return value is only valid if the execution finished successfully
		AngelScript::asDWORD ret = context->GetReturnDWord();
	}
	return 0;
}


int ScriptEngine::envokeCallback(int functionPtr, eventsource_t *source, node_t *node, int type)
{
	if(!engine) return 0;
	if(functionPtr <= 0 && defaultEventCallbackFunctionPtr > 0)
	{
		// use the default event handler instead then
		functionPtr = defaultEventCallbackFunctionPtr;
	} else if (functionPtr <= 0)
	{
		// no default callback available, discard the event
		return 0;
	}
	if(!context) context = engine->CreateContext();
	context->Prepare(functionPtr);

	// Set the function arguments
	std::string *instance_name = new std::string(source->instancename);
	std::string *boxname = new std::string(source->boxname);
	context->SetArgDWord (0, type);
	context->SetArgObject(1, instance_name);
	context->SetArgObject(2, boxname);
	if(node)
		context->SetArgDWord (3, node->id);
	else
		context->SetArgDWord (3, -1);

	int r = context->Execute();
	if( r == AngelScript::asEXECUTION_FINISHED )
	{
	  // The return value is only valid if the execution finished successfully
		AngelScript::asDWORD ret = context->GetReturnDWord();
	}
	delete(instance_name);
	delete(boxname);

	return 0;
}

int ScriptEngine::executeString(Ogre::String command)
{
	if(!engine) return 1;
	if(!context) context = engine->CreateContext();
	AngelScript::asIScriptModule *mod = engine->GetModule(moduleName, AngelScript::asGM_CREATE_IF_NOT_EXISTS);
	int result = ExecuteString(engine, command.c_str(), mod, context);
	if(result < 0)
	{
		SLOG("error " + TOSTRING(result) + " while executing string: " + command + ".");
	}
	return result;
}

void ScriptEngine::triggerEvent(int eventnum, int value)
{
	if(!engine) return;
	if(eventCallbackFunctionPtr<=0) return;
	if(eventMask & eventnum)
	{
		// script registered for that event, so sent it
		if(!context) context = engine->CreateContext();
		context->Prepare(eventCallbackFunctionPtr);

		// Set the function arguments
		context->SetArgDWord(0, eventnum);
		context->SetArgDWord(1, value);

		int r = context->Execute();
		if( r == AngelScript::asEXECUTION_FINISHED )
		{
		  // The return value is only valid if the execution finished successfully
			AngelScript::asDWORD ret = context->GetReturnDWord();
		}
		return;
	}
}

int ScriptEngine::loadScript(Ogre::String scriptname)
{
	// Load the entire script file into the buffer
	int result=0;

	// The builder is a helper class that will load the script file, 
	// search for #include directives, and load any included files as 
	// well.
	OgreScriptBuilder builder;

	AngelScript::asIScriptModule *mod = 0;
	// try to load bytecode
	bool cached = false;
	{
		// the code below should load a compilation result but it crashes for some reason atm ...
		//AngelScript::asIScriptModule *mod = engine->GetModule("RoRScript", AngelScript::asGM_ALWAYS_CREATE);
		/*
		String fn = SSETTING("Cache Path") + "script" + hash + "_" + scriptname + "c";
		CBytecodeStream bstream(fn);
		if(bstream.Existing())
		{
			// CRASHES here :(
			int res = mod->LoadByteCode(&bstream);
			cached = !res;
		}
		*/
	}
	if(!cached)
	{
		// not cached so dynamically load and compile it
		result = builder.StartNewModule(engine, moduleName);
		if( result < 0 )
		{
			SLOG("Failed to start new module");
			return result;
		}

		mod = engine->GetModule(moduleName, AngelScript::asGM_ONLY_IF_EXISTS);

		result = builder.AddSectionFromFile(scriptname.c_str());
		if( result < 0 )
		{
			SLOG("Unkown error while loading script file: "+scriptname);
			SLOG("Failed to add script file");
			return result;
		}
		result = builder.BuildModule();
		if( result < 0 )
		{
			SLOG("Failed to build the module");
			return result;
		}

		// save the bytecode
		string hash = "";
		// TODO: fix hash!
		{
			String fn = SSETTING("Cache Path") + "script" + hash + "_" + scriptname + "c";
			SLOG("saving script bytecode to file " + fn);
			CBytecodeStream bstream(fn);
			mod->SaveByteCode(&bstream);
		}
	}

	// get some other optional functions
	frameStepFunctionPtr = mod->GetFunctionIdByDecl("void frameStep(float)");
	if(frameStepFunctionPtr > 0) callbacks["frameStep"].push_back(frameStepFunctionPtr);
	
	wheelEventFunctionPtr = mod->GetFunctionIdByDecl("void wheelEvents(int, string, string, string)");
	if(wheelEventFunctionPtr > 0) callbacks["wheelEvents"].push_back(wheelEventFunctionPtr);

	eventCallbackFunctionPtr = mod->GetFunctionIdByDecl("void eventCallback(int, int)");
	if(eventCallbackFunctionPtr > 0) callbacks["eventCallback"].push_back(eventCallbackFunctionPtr);

	defaultEventCallbackFunctionPtr = mod->GetFunctionIdByDecl("void defaultEventCallback(int, string, string, int)");
	if(defaultEventCallbackFunctionPtr > 0) callbacks["defaultEventCallback"].push_back(defaultEventCallbackFunctionPtr);

	int cb = mod->GetFunctionIdByDecl("void on_terrain_loading(string lines)");
	if(cb > 0) callbacks["on_terrain_loading"].push_back(cb);

	// Find the function that is to be called.
	int funcId = mod->GetFunctionIdByDecl("void main()");
	if( funcId < 0 )
	{
		// The function couldn't be found. Instruct the script writer to include the
		// expected function in the script.
		SLOG("The script should have the function 'void main()'.");
		return 0;
	}

	// Create our context, prepare it, and then execute
	context = engine->CreateContext();


	unsigned long timeOut = 0;
	/*
	// TOFIX: AS crashes badly when using these :-\
	result = context->SetLineCallback(AngelScript::asMETHOD(ScriptEngine, LineCallback), &timeOut, AngelScript::asCALL_THISCALL);
	if(result < 0)
	{
		SLOG("Failed to set the line callback function.");
		context->Release();
		return -1;
	}

	result = context->SetExceptionCallback(AngelScript::asMETHOD(ScriptEngine,ExceptionCallback), this, AngelScript::asCALL_THISCALL);
	if(result < 0)
	{
		SLOG("Failed to set the exception callback function.");
		context->Release();
		return -1;
	}
	*/

	// Prepare the script context with the function we wish to execute. Prepare()
	// must be called on the context before each new script function that will be
	// executed. Note, that if you intend to execute the same function several 
	// times, it might be a good idea to store the function id returned by 
	// GetFunctionIDByDecl(), so that this relatively slow call can be skipped.
	result = context->Prepare(funcId);
	if(result < 0)
	{
		SLOG("Failed to prepare the context.");
		context->Release();
		return -1;
	}

	// Set the timeout before executing the function. Give the function 1 sec
	// to return before we'll abort it.
	timeOut = OgreFramework::getSingleton().getTimeSinceStartup() + 1000;

	SLOG("Executing main()");
	result = context->Execute();
	if( result != AngelScript::asEXECUTION_FINISHED )
	{
		// The execution didn't complete as expected. Determine what happened.
		if( result == AngelScript::asEXECUTION_ABORTED )
		{
			SLOG("The script was aborted before it could finish. Probably it timed out.");
		}
		else if( result == AngelScript::asEXECUTION_EXCEPTION )
		{
			// An exception occurred, let the script writer know what happened so it can be corrected.
			SLOG("An exception '" + String(context->GetExceptionString()) + "' occurred. Please correct the code in file '" + scriptname + "' and try again.");

			// Write some information about the script exception
			int funcID = context->GetExceptionFunction();
			AngelScript::asIScriptFunction *func = engine->GetFunctionDescriptorById(funcID);
			SLOG("func: " + String(func->GetDeclaration()));
			SLOG("modl: " + String(func->GetModuleName()));
			SLOG("sect: " + String(func->GetScriptSectionName()));
			SLOG("line: " + TOSTRING(context->GetExceptionLineNumber()));
			SLOG("desc: " + String(context->GetExceptionString()));
		} else
		{
			SLOG("The script ended for some unforeseen reason " + TOSTRING(result));
		}
	} else
	{
		SLOG("The script finished successfully.");
	}

	return 0;
}
