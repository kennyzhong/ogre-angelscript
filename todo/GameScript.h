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

#ifndef GAMESCRIPT_H__
#define GAMESCRIPT_H__

#include "RoRPrerequisites.h"

#include <string>
#include <angelscript.h>
#include <Ogre.h>
#include <OgreLogManager.h>

#include "scriptdictionary/scriptdictionary.h"
#include "scriptbuilder/scriptbuilder.h"

#include "collisions.h"

#include "ScriptEngine.h"

#include "ImprovedConfigFile.h"

struct curlMemoryStruct {
  char *memory;
  size_t size;
};

/**
 *  @brief Proxy class that can be called by script functions
 */
class GameScript
{
protected:
	ScriptEngine *mse;              //!< local script engine pointer, used as proxy mostly
	RoRFrameListener *mefl;     //!< local pointer to the main RoRFrameListener, used as proxy mostly

public:
	/**
	 * constructor
	 * @param se pointer to the ScriptEngine instance
	 * @param efl pointer to the RoRFrameListener instance
	 */
	GameScript(ScriptEngine *se, RoRFrameListener *efl);

	/**
	 * destructor
	 */
	~GameScript();

	/**
	 * writes a message to the games log (RoR.log)
	 * @param msg string to log
	 */
	void log(std::string &msg);

	/**
	 * returns the time in seconds since the game was started
	 * @return time in seconds
	 */
	double getTime();

	//anglescript test
	void boostCurrentTruck(float factor);

	/**
	 * sets the character position
	 * @param x X position on the terrain
	 * @param y Y position on the terrain
	 * @param z Z position on the terrain
	 */
	void setPersonPosition(Ogre::Vector3 vec);

	void loadTerrain(std::string &terrain);
	/**
	 * moves the person relative
	 * @param x X translation
	 * @param y Y translation
	 * @param z Z translation
	 */
	void movePerson(Ogre::Vector3);

	/**
	 * gets the time of the day in seconds
	 * @return string with HH::MM::SS format
	 */
	std::string getCaelumTime();
	
	/**
	 * sets the time of the day in seconds
	 * @param value day time in seconds
	 */
	void setCaelumTime(float value);
	
	/**
	 * returns the current base water level (without waves)
	 * @return water height in meters
	 */
	float getWaterHeight();

	float getGroundHeight(Ogre::Vector3 v);

	/**
	 * sets the base water height
	 * @param value base height in meters
	 */
	void setWaterHeight(float value);

	/**
	 * returns the current selected truck, 0 if in person mode
	 * @return reference to Beam object that is currently in use
	 */
	Beam *getCurrentTruck();

	/**
	 * returns a truck by index, get max index by calling getNumTrucks
	 * @return reference to Beam object that the selected slot
	 */
	Beam *getTruckByNum(int num);

	/**
	 * returns the current amount of loaded trucks
	 * @return integer value representing the amount of loaded trucks
	 */
	int getNumTrucks();

	/**
	 * returns the current truck number. >=0 when using a truck, -1 when in person mode
	 * @return integer truck number
	 */
	int getCurrentTruckNumber();
	
	/**
	 * returns the currently set upo gravity
	 * @return float number describing gravity terrain wide.
	 */
	float getGravity();
	
	/**
	 * sets the gravity terrain wide. This is an expensive call, since the masses of all trucks are recalculated.
	 * @param value new gravity terrain wide (default is -9.81)
	 */
	void setGravity(float value);

	/**
	 * registers for a new event to be received by the scripting system
	 * @param eventValue \see enum scriptEvents
	 */
	void registerForEvent(int eventValue);

	/**
	 * shows a message to the user
	 */
	void flashMessage(std::string &txt, float time, float charHeight);

	/**
	 * set direction arrow
	 * @param text text to be displayed. "" to hide the text
	 */
	void setDirectionArrow(std::string &text, Ogre::Vector3 vec);


	/**
	 * returns the size of the font used by the chat box
	 * @return pixel size of the chat text
	 */
	int getChatFontSize();

	/**
	 * changes the font size of the chat box
	 * @param size font size in pixels
	 */
	void setChatFontSize(int size);
	
	/**
	 * Sets the camera's position.
	 * @param pos The new position of the camera.
	 */
	void setCameraPosition(Ogre::Vector3 pos);
	
	/**
	 * Sets the camera's direction vector.
	 * @param vec A vector representing the direction of the vector.
	 */
	void setCameraDirection(Ogre::Vector3 vec);
	
	/**
	 * Rolls the camera anticlockwise, around its local z axis.
	 * @param angle The roll-angle
	 */
	void setCameraRoll(float angle);

	/**
	 * Rotates the camera anticlockwise around it's local y axis.
	 * @param angle The yaw-angle 
	 */
	void setCameraYaw(float angle);

	/**
	 * Pitches the camera up/down anticlockwise around it's local z axis.
	 * @param angle The pitch-angle
	 */
	void setCameraPitch(float angle);
	
	/**
	  * Retrieves the camera's position.
	  * @return The current position of the camera
	 */
	Ogre::Vector3 getCameraPosition();
	
	/**
	 * Gets the camera's direction.
	 * @return A vector representing the direction of the camera
	 */
	Ogre::Vector3 getCameraDirection();
	
	/** 
	 * Points the camera at a location in worldspace.
	 * @remarks
	 *      This is a helper method to automatically generate the
	 *      direction vector for the camera, based on it's current position
	 *      and the supplied look-at point.
	 * @param targetPoint A vector specifying the look at point.
	*/
	void cameraLookAt(Ogre::Vector3 targetPoint);

	// new things, not documented yet
	void showChooser(std::string &type, std::string &instance, std::string &box);
	void repairVehicle(std::string &instance, std::string &box, bool keepPosition);
	void removeVehicle(std::string &instance, std::string &box);

	void spawnObject(const std::string &objectName, const std::string &instanceName, Ogre::Vector3 pos, Ogre::Vector3 rot, const std::string &eventhandler, bool uniquifyMaterials);
	void destroyObject(const std::string &instanceName);
	int getNumTrucksByFlag(int flag);
	bool getCaelumAvailable();
	float stopTimer();
	void startTimer();
	std::string getSetting(std::string str);
	void hideDirectionArrow();
	int setMaterialAmbient(const std::string &materialName, float red, float green, float blue);
	int setMaterialDiffuse(const std::string &materialName, float red, float green, float blue, float alpha);
	int setMaterialSpecular(const std::string &materialName, float red, float green, float blue, float alpha);
	int setMaterialEmissive(const std::string &materialName, float red, float green, float blue);
	
	float rangeRandom(float from, float to);
	int useOnlineAPI(const std::string &apiquery, const AngelScript::CScriptDictionary &dict, std::string &result);
	int getLoadedTerrain(std::string &result);
	Ogre::Vector3 getPersonPosition();

	void clearEventCache();
};

#endif // GAMESCRIPT_H__
