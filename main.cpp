 // DRCB:Data Ref CallBack
 // FLCB:Flight Loop CallBack

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include "XPLMDataAccess.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMMenus.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#if IBM
    #include <windows.h>
#endif

#if LIN
    #include <GL/gl.h>
#elif __GNUC__
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif
#include "pid.h"
#define MSG_ADD_DATAREF 0x01000000
PID* pid;
static XPLMWindowID	g_window;
void draw_hello_world(XPLMWindowID in_window_id, void* in_refcon);
int dummy_mouse_handler(XPLMWindowID in_window_id, int x, int y, int is_down, void* in_refcon) { return 0; }
XPLMCursorStatus dummy_cursor_status_handler(XPLMWindowID in_window_id, int x, int y, void* in_refcon) { return xplm_CursorDefault; }
int dummy_wheel_handler(XPLMWindowID in_window_id, int x, int y, int wheel, int clicks, void* in_refcon) { return 0; }
void dummy_key_handler(XPLMWindowID in_window_id, char key, XPLMKeyFlags flags, char virtual_key, void* in_refcon, int losing_focus) { }

// dataref vars
XPLMCommandRef showwindow;
XPLMDataRef AltitudeAGLDataRef, ElevatorTrimDataRef, PitchOverrideDR, CommandedPitchDR, AoADataRef = NULL;
int g_menu_container_idx;
XPLMMenuID g_menu_id;

void menu_handler(void*, void*);
float   GetAoADRCB(void* inRefcon);
float   GetAltitudeAGLDRCB(void* inRefcon);
float   GetElevatorTrimPositionDRCB(void* inRefcon);
void    SetElevatorTrimPositionDRCB(void* inRefcon, float outValue);
void    SetPitchOverride(void* inRefcon, float inValue);
float   GetCommandedPitch(void* inRefcon);
void    SetCommandedPitch(void* inRefcon, float inValue);
float	MainFLCB(float elapsedMe, float elapsedSim, int counter, void* refcon);

float ElevatorTrimPos = 0;
float AoA;
float Cp;
//

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc)
{
    strcpy_s(outName, 40, "Test");
    strcpy_s(outSig, 40, "qjlu404.alpha.p");
    strcpy_s(outDesc, 40, "simple stall prevention");
    
    showwindow = XPLMCreateCommand("stallhelper/showwindow", "Shows window.");

    CommandedPitchDR = XPLMRegisterDataAccessor(
        "sim/joystick/yoke_pitch_ratio",
        xplmType_Int,            // The types we support 
        1,                       // Writable 
        (XPLMGetDatai_f)GetCommandedPitch, (XPLMSetDatai_f)SetCommandedPitch,  // Integer accessors  
        NULL, NULL,              // Float accessors zzplm
        NULL, NULL,              // Doubles accessors 
        NULL, NULL,              // Int array accessors 
        NULL, NULL,              // Float array accessors 
        NULL, NULL,              // Raw data accessors 
        NULL, NULL);

    PitchOverrideDR = XPLMRegisterDataAccessor(
        "sim/operation/override/override_joystick_pitch",
        xplmType_Int,            // The types we support 
        1,                       // Writable 
        NULL, (XPLMSetDatai_f)SetPitchOverride,  // Integer accessors  
        NULL, NULL,              // Float accessors zzplm
        NULL, NULL,              // Doubles accessors 
        NULL, NULL,              // Int array accessors 
        NULL, NULL,              // Float array accessors 
        NULL, NULL,              // Raw data accessors 
        NULL, NULL);

    AltitudeAGLDataRef = XPLMRegisterDataAccessor(
        "sim/flightmodel/position/y_agl",
        xplmType_Float,          // The types we support 
        1,                       // Writable 
        NULL, NULL,              // Integer accessors  
        GetAltitudeAGLDRCB, NULL,// Float accessors 
        NULL, NULL,              // Doubles accessors 
        NULL, NULL,              // Int array accessors 
        NULL, NULL,              // Float array accessors 
        NULL, NULL,              // Raw data accessors 
        NULL, NULL);

    AoADataRef = XPLMRegisterDataAccessor(
        "sim/flightmodel2/misc/AoA_angle_degrees",
        xplmType_Float,          // The types we support 
        1,                       // Writable 
        NULL, NULL,              // Integer accessors  
        GetAoADRCB, NULL,        // Float accessors 
        NULL, NULL,              // Doubles accessors 
        NULL, NULL,              // Int array accessors 
        NULL, NULL,              // Float array accessors 
        NULL, NULL,              // Raw data accessors 
        NULL, NULL);

    ElevatorTrimDataRef = XPLMRegisterDataAccessor(
        "sim/cockpit2/controls/elevator_trim",
        xplmType_Float,          // The types we support 
        1,                       // Writable 
        NULL, NULL,              // Integer accessors  
        GetElevatorTrimPositionDRCB, SetElevatorTrimPositionDRCB,// Float accessors 
        NULL, NULL,              // Doubles accessors 
        NULL, NULL,              // Int array accessors 
        NULL, NULL,              // Float array accessors 
        NULL, NULL,              // Raw data accessors 
        NULL, NULL);

    PitchOverrideDR = XPLMFindDataRef("sim/operation/override/override_joystick_pitch");
    AltitudeAGLDataRef = XPLMFindDataRef("sim/flightmodel/position/y_agl");
    AoADataRef = XPLMFindDataRef("sim/flightmodel2/misc/AoA_angle_degrees");
    ElevatorTrimDataRef = XPLMFindDataRef("sim/cockpit2/controls/elevator_trim");

    g_menu_container_idx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Stall Helper", 0, 0);
    g_menu_id = XPLMCreateMenu("Main", XPLMFindPluginsMenu(), g_menu_container_idx, menu_handler, NULL);
    XPLMAppendMenuItemWithCommand(g_menu_id, "(not implemented) Show Window", showwindow);



    XPLMRegisterFlightLoopCallback(MainFLCB, 0.0, NULL);
    XPLMSetFlightLoopCallbackInterval(MainFLCB, 0.1, 1, NULL);
    XPLMCreateWindow_t params;
    params.structSize = sizeof(params);
    params.visible = 1;
    params.drawWindowFunc = draw_hello_world;
    params.handleMouseClickFunc = dummy_mouse_handler;
    params.handleRightClickFunc = dummy_mouse_handler;
    params.handleMouseWheelFunc = dummy_wheel_handler;
    params.handleKeyFunc = dummy_key_handler;
    params.handleCursorFunc = dummy_cursor_status_handler;
    params.refcon = NULL;
    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    int left, bottom, right, top;
    XPLMGetScreenBoundsGlobal(&left, &top, &right, &bottom);
    params.left = left + 50;
    params.bottom = bottom + 150;
    params.right = params.left + 200;
    params.top = params.bottom + 200;

    g_window = XPLMCreateWindowEx(&params);

    XPLMSetWindowPositioningMode(g_window, xplm_WindowPositionFree, -1);
    XPLMSetWindowResizingLimits(g_window, 300, 80, 300, 80);
    XPLMSetWindowTitle(g_window, "Stall Helper Debug");

    return g_window != NULL;
}
PLUGIN_API void	XPluginStop(void)
{
    XPLMUnregisterDataAccessor(AoADataRef);
    XPLMUnregisterDataAccessor(ElevatorTrimDataRef);
    XPLMUnregisterDataAccessor(AltitudeAGLDataRef);
    XPLMUnregisterDataAccessor(AltitudeAGLDataRef);
    XPLMUnregisterFlightLoopCallback(MainFLCB, NULL);
    XPLMDestroyWindow(g_window);
    g_window = NULL;
    delete pid;
}
PLUGIN_API int XPluginEnable(void)
{
    return 1;
}
PLUGIN_API void XPluginDisable(void)
{

}
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho, long inMessage, void* inParam)
{
}
float   GetAoADRCB(void* inRefcon)
{
    float aoa = XPLMGetDataf(AoADataRef);
    return aoa;
}
float   GetAltitudeAGLDRCB(void* inRefcon)
{
    float AltitudeAGL = XPLMGetDataf(AltitudeAGLDataRef);
    return AltitudeAGL * 3.28084;
}
float	GetElevatorTrimPositionDRCB(void* inRefcon)
{
    return ElevatorTrimPos;
}
void	SetElevatorTrimPositionDRCB(void* inRefcon, float inValue)
{
    ElevatorTrimPos = inValue;
}
void    SetPitchOverride(void* inRefcon, float inValue)
{

}
float   GetCommandedPitch(void* inRefcon)
{
    return XPLMGetDataf(CommandedPitchDR);
}
void    SetCommandedPitch(void* inRefcon, float inValue)
{
    Cp = inValue;
}
int active = 3;
float response = 0.1;
float AltitudeAGL = 0;
bool stalled = 0;
int activations = 0;
float settrim;
float InitialTrim;
double res;
void draw_hello_world(XPLMWindowID in_window_id, void* in_refcon)
{
    XPLMSetGraphicsState(
        0 /* no fog */,
        0 /* 0 texture units */,
        0 /* no lighting */,
        0 /* no alpha testing */,
        1 /* do alpha blend */,
        1 /* do depth testing */,
        0 /* no depth writing */
    );
    char aoastr[100];
    char otherinfo[100];
    char otherinfo2[100];
    char otherinfo3[100];
    float col_red[] = { 1.0, 0, 0 };
    float col_og[] = { 1, 1, 0 };
    float col_white[] = { 1.0, 1.0, 1.0 };
    int l, t, r, b;
    XPLMGetWindowGeometry(in_window_id, &l, &t, &r, &b);
    if (active == 1) 
    {
        sprintf(aoastr, "Angle of Attack: %f Stall!", AoA);
        XPLMDrawString(col_red, l + 10, t - 20, aoastr, NULL, xplmFont_Proportional);
    }
    else if (active == 2) 
    {
        sprintf(aoastr, "Angle of Attack: %f Caution!", AoA);
        XPLMDrawString(col_og, l + 10, t - 20, aoastr, NULL, xplmFont_Proportional);
    }
    else 
    {
        sprintf(aoastr, "Angle of Attack: %f --", AoA);
        XPLMDrawString(col_white, l + 10, t - 20, aoastr, NULL, xplmFont_Proportional);
    }
    pid = new PID(0.2, 0.3, 0.01, 0.01f, 1, -1);
    sprintf(otherinfo, "Altitude AGL (meters): %i, Activations: %i",(int)AltitudeAGL, activations);
    sprintf(otherinfo2, "CommandedPitch: %f PID: %f", Cp, res);

    XPLMDrawString(col_white, l + 10, t - 40, otherinfo, NULL, xplmFont_Proportional);
    XPLMDrawString(col_white, l + 10, t - 60, otherinfo2, NULL, xplmFont_Proportional);
}

void menu_handler(void*, void*)
{

}

float  MainFLCB(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void* inRefcon)
{
    pid = new PID(0.03, 0.02, 0.01, 0.01f, 1, -1);
    float response = 0.01;
    AltitudeAGL = XPLMGetDataf(AltitudeAGLDataRef);
    AoA = XPLMGetDataf(AoADataRef);
    Cp = XPLMGetDataf(CommandedPitchDR);
    res = pid->calculate(15, AoA);
    if (AoA > 12)
    {
        XPLMSetDataf(ElevatorTrimDataRef, res);
    }
    return response;
}
