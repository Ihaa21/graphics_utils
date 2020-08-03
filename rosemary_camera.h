#pragma once

/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

struct camera;

enum camera_type
{
    CameraType_None,

    CameraType_Game,
    CameraType_Fps,
    CameraType_Radial, // TODO: Implement
};

//
// NOTE: Camera Interactions
//

struct drag_game_camera
{
    v2 PrevMouseP;
    f32 NumSecondsHeld;
};

struct drag_fps_camera
{
    v2 PrevMousePixelPos;
};

struct drag_camera_interaction
{
    camera* Camera;

    union
    {
        drag_game_camera GameCamera;
        drag_fps_camera FpsCamera;
    };
};

struct zoom_camera_interaction
{
    camera* Camera;
};

//
// NOTE: Camera Types
//

struct game_camera
{
    f32 ZoomSpeed;
    f32 MoveSpeed;
    f32 OneMinusDragPercent;
    v3 Vel;
};

struct fps_camera
{
    q4 Orientation;
    f32 TurningVelocity;
    f32 Velocity;
};

struct camera
{
    f32 AspectRatio;
    f32 Near;
    f32 Far;
    f32 Fov;
    
    v3 Pos;
    v3 View;
    v3 Up;
    v3 Right;

    camera_type Type;
    union
    {
        game_camera GameCamera;
        fps_camera FpsCamera;
    };
};

inline m4 CameraGetVP(camera* Camera);
inline v3 CameraGetMouseZeroPlanePos(camera* Camera, v2 MouseP);
