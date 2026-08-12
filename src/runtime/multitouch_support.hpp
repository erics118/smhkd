#pragma once

#include <CoreFoundation/CoreFoundation.h>

// MultitouchSupport is a private framework with no public headers, so its API and
// frame struct are declared here. callbacks are delivered on the run loop of the
// thread that starts the devices.
extern "C" {
typedef struct {
    float x;
    float y;
} MTPoint;

typedef struct {
    MTPoint position;
    MTPoint velocity;
} MTReadout;

typedef struct {
    int frame;
    double timestamp;
    int identifier;
    int state;
    int fingerId;
    int handId;
    MTReadout normalized;
    float size;
    int pressure;
    float angle;
    float majorAxis;
    float minorAxis;
    MTReadout absolute;
    int unk1;
    int unk2;
    float zDensity;
} Finger;

typedef void* MTDeviceRef;
typedef int (*MTContactCallbackFunction)(int device, Finger* fingers, int nFingers, double timestamp, int frame);

CFMutableArrayRef MTDeviceCreateList(void);
void MTRegisterContactFrameCallback(MTDeviceRef, MTContactCallbackFunction);
void MTDeviceStart(MTDeviceRef, int);
void MTDeviceStop(MTDeviceRef);
}
