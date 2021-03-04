 // DRCB:Data Ref CallBack
 // FLCB:Flight Loop CallBack

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

#ifndef XPLM300
    #error This is made to be compiled against the XPLM300 SDK
#endif

#define MSG_ADD_DATAREF 0x01000000

static XPLMWindowID	g_window;

// Callbacks we will register when we create our window
void				draw_hello_world(XPLMWindowID in_window_id, void* in_refcon);
int					dummy_mouse_handler(XPLMWindowID in_window_id, int x, int y, int is_down, void* in_refcon) { return 0; }
XPLMCursorStatus	dummy_cursor_status_handler(XPLMWindowID in_window_id, int x, int y, void* in_refcon) { return xplm_CursorDefault; }
int					dummy_wheel_handler(XPLMWindowID in_window_id, int x, int y, int wheel, int clicks, void* in_refcon) { return 0; }
void				dummy_key_handler(XPLMWindowID in_window_id, char key, XPLMKeyFlags flags, char virtual_key, void* in_refcon, int losing_focus) { }

// dataref vars
XPLMCommandRef showwindow;
XPLMDataRef AltitudeAGLDataRef = NULL;               // altitude agl dataref
XPLMDataRef PitchDataRef = NULL;                     // pitch dataref
XPLMDataRef AoADataRef = NULL;                       // aoa dataref
XPLMDataRef ElevatorTrimDataRef = NULL;              // elevator trim dataref
int g_menu_container_idx; // The index of our menu item in the Plugins menu
XPLMMenuID g_menu_id; // The menu container we'll append all our menu items to
void menu_handler(void*, void*);

float   GetAoA(void* inRefcon);
float   GetAltitudeAGL(void* inRefcon);

float   GetAoADRCB(void* inRefcon);
float   GetAltitudeAGLDRCB(void* inRefcon);

float   GetElevatorTrimPosition(void* inRefcon);
void    SetElevatorTrimPosition(void* inRefcon, float outValue);

float   GetElevatorTrimPositionDRCB(void* inRefcon);
void    SetElevatorTrimPositionDRCB(void* inRefcon, float outValue);

float	MainFLCB(float elapsedMe, float elapsedSim, int counter, void* refcon);

float ElevatorTrimPos = 0;
float AoA;

//

PLUGIN_API int XPluginStart(
    char* outName,
    char* outSig,
    char* outDesc)
{
    strcpy(outName, "Simple Stall Prevention");
    strcpy(outSig, "vutter.alpha.p");
    strcpy(outDesc, "simple stall prevention");

    showwindow = XPLMCreateCommand("ShowWindow", "Shows window.");
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

    AltitudeAGLDataRef = XPLMFindDataRef("sim/flightmodel/position/y_agl");
    AoADataRef = XPLMFindDataRef("sim/flightmodel2/misc/AoA_angle_degrees");
    ElevatorTrimDataRef = XPLMFindDataRef("sim/cockpit2/controls/elevator_trim");

    g_menu_container_idx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Stall Helper", 0, 0);
    g_menu_id = XPLMCreateMenu("Main", XPLMFindPluginsMenu(), g_menu_container_idx, menu_handler, NULL);
    XPLMAppendMenuItemWithCommand(g_menu_id, "Show Window", showwindow);



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
    XPLMUnregisterFlightLoopCallback(MainFLCB, NULL);
    XPLMDestroyWindow(g_window);
    g_window = NULL;
}


PLUGIN_API int XPluginEnable(void)
{
    return 1;
}


PLUGIN_API void XPluginDisable(void)
{

}



PLUGIN_API void XPluginReceiveMessage(
    XPLMPluginID	inFromWho,
    long			inMessage,
    void* inParam)
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
    return AltitudeAGL;
}

float	GetElevatorTrimPositionDRCB(void* inRefcon)
{
    return ElevatorTrimPos;
}


void	SetElevatorTrimPositionDRCB(void* inRefcon, float inValue)
{
    ElevatorTrimPos = inValue;
}

int active = 3;
float response = 0.1;
float AltitudeAGL = 0;
bool stalled = 0;
int activations = 0;
float settrim = XPLMGetDataf(ElevatorTrimDataRef);
float InitialTrim = XPLMGetDataf(ElevatorTrimDataRef);


void	draw_hello_world(XPLMWindowID in_window_id, void* in_refcon)
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

    sprintf(otherinfo, "Altitude AGL (meters): %i, Activations: %i",(int)AltitudeAGL, activations);
    sprintf(otherinfo2, "Trim: %f, Initial Trim: %f", settrim, InitialTrim);

    XPLMDrawString(col_white, l + 10, t - 40, otherinfo, NULL, xplmFont_Proportional);
    XPLMDrawString(col_white, l + 10, t - 60, otherinfo2, NULL, xplmFont_Proportional);
}

void menu_handler(void*, void*)
{

}




float  MainFLCB(
    float  inElapsedSinceLastCall,
    float  inElapsedTimeSinceLastFlightLoop,
    int    inCounter,
    void* inRefcon)
{
    float response = 0.25;
    AltitudeAGL = XPLMGetDataf(AltitudeAGLDataRef);
    AoA = XPLMGetDataf(AoADataRef);
    if (AoA > 15 && AltitudeAGL > 60)
    {
        active = 1;
        stalled = 1;
        response = 0.01;
        if (settrim > -1)
        {
            settrim -= 0.01;
            XPLMSetDataf(ElevatorTrimDataRef, settrim);
        }
        activations++;
    }
    else if (AoA > 12 && AltitudeAGL > 152)
    {
        active = 2;
        stalled = 1;
        response = 0.05;
        if (settrim > -1)
        {
            settrim -= 0.01;
            XPLMSetDataf(ElevatorTrimDataRef, settrim);
        }
        activations++;
    }
    else if (AoA < 10 || AltitudeAGL < 60)
    {
        if (stalled == 1)
        {
            if (activations > 100)
            {
                activations = 0;
            }
            if (settrim > -1.2 && settrim < 1.1)
            {
                if (settrim > InitialTrim + 0.02)
                {
                    response = 0.01;
                    settrim -= 0.01;
                    XPLMSetDataf(ElevatorTrimDataRef, settrim);
                }
                else if (settrim < InitialTrim - 0.02)
                {
                    response = 0.01;
                    settrim += 0.01;
                    XPLMSetDataf(ElevatorTrimDataRef, settrim);
                }
                else 
                {
                    stalled = 0; 
                }
            }
        }
        else
        {
            response = 0.5;
            active = 3;
            InitialTrim = XPLMGetDataf(ElevatorTrimDataRef);
            settrim = XPLMGetDataf(ElevatorTrimDataRef);
        }
    }
    else
    {
        settrim = XPLMGetDataf(ElevatorTrimDataRef);
    }
    return response;
}
