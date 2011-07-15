/*
 *  SFOSVersion.c
 *  Kaleidoscope
 *
 *  Created by Pieter de Bie on 13-8-10.
 *  Copyright 2010 Sofa BV. All rights reserved.
 *
 */

#include "SFOSVersion.h"

int SFGetMajorOSVersion(void)
{
	SInt32 aVersionNumber;
	OSErr anError = Gestalt(gestaltSystemVersionMajor, &aVersionNumber);
	if (anError != noErr)
		return -1;

	return aVersionNumber;
}

int SFGetMinorOSVersion(void)
{
	SInt32 aVersionNumber;
	OSErr anError = Gestalt(gestaltSystemVersionMinor, &aVersionNumber);
	if (anError != noErr)
		return -1;
	
	return aVersionNumber;
}

int SFGetBugFixOSVersion(void)
{
	SInt32 aVersionNumber;
	OSErr anError = Gestalt(gestaltSystemVersionBugFix, &aVersionNumber);
	if (anError != noErr)
		return -1;
	
	return aVersionNumber;
}

CFStringRef SFCreateOSVersionString(void)
{
	int aMajorVersion = SFGetMajorOSVersion();
	int aMinorVersion = SFGetMinorOSVersion();
	int aBugFixVersion = SFGetBugFixOSVersion();
	
	if (    aMajorVersion == -1
		 || aMinorVersion == -1
		 || aBugFixVersion == -1)
		return NULL;

	return CFStringCreateWithFormat(NULL, NULL, CFSTR("%i.%i.%i"), aMajorVersion, aMinorVersion, aBugFixVersion);
}
