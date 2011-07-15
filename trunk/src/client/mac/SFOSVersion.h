/*
 *  SFOSVersion.h
 *  Kaleidoscope
 *
 *  Created by Pieter de Bie on 13-8-10.
 *  Copyright 2010 Sofa BV. All rights reserved.
 *
 */
#import <Foundation/Foundation.h>

#ifndef KS_EXPORT
#define KS_EXPORT extern
#endif // KS_EXPORT

// These return -1 if they fail

/** Major Version number, usually 10 (from 10.6.4) */
KS_EXPORT int SFGetMajorOSVersion(void);

/** Minor Version number, like 6 in 10.6.4 */
KS_EXPORT int SFGetMinorOSVersion(void);

/** BugFix Version number, like 4 in 10.6.4 */
KS_EXPORT int SFGetBugFixOSVersion(void);

/**
 * \return a CFString in the form of "10.6.4". Returns NULL if failed
 */
KS_EXPORT CFStringRef SFCreateOSVersionString(void);
