#pragma once

// Standard base includes, defines that indicate our current platform, etc.

#include <stddef.h>


// Which platform we are on?
// UNITY_WIN - Windows (regular win32)
// UNITY_OSX - Mac OS X
// UNITY_LINUX - Linux
// UNITY_IOS - iOS
// UNITY_TVOS - tvOS
// UNITY_ANDROID - Android
// UNITY_METRO - WSA or UWP
// UNITY_WEBGL - WebGL

#define UNITY_ANDROID 1

// Which graphics device APIs we possibly support?
#ifndef SUPPORT_OPENGL_ES
	#define SUPPORT_OPENGL_ES 1
#endif
#define SUPPORT_OPENGL_UNIFIED SUPPORT_OPENGL_ES

// COM-like Release macro
#ifndef SAFE_RELEASE
	#define SAFE_RELEASE(a) if (a) { a->Release(); a = NULL; }
#endif

