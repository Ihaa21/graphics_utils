/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

inline camera CameraCreate_(input_state* InputState, v3 Pos, v3 View, v3 Up, v3 Right, f32 AspectRatio, f32 Near, f32 Far, f32 Fov)
{
    camera Result = {};

    // TODO: Use in shadow map in playstate?

    Result.AspectRatio = AspectRatio;
    Result.Near = Near;
    Result.Far = Far;
    Result.Fov = DegreeToRad(Fov);
    
    Result.Pos = Pos;
    Result.View = View;
    Result.Up = Up;
    Result.Right = Right;

    return Result;
}

inline camera CameraGameCreate(input_state* InputState, v3 Pos, v3 View, f32 AspectRatio, f32 Near, f32 Far, f32 Fov,
                               f32 ZoomSpeed, f32 MoveSpeed, f32 DragPercent)
{
    // NOTE: https://github.com/Erkaman/gl-movable-camera
    camera Result = CameraCreate_(InputState, Pos, View, V3(0, 1, 0), V3(-1, 0, 0), AspectRatio, Near, Far, Fov);

    Result.Type = CameraType_Game;
    Result.GameCamera.ZoomSpeed = ZoomSpeed;
    Result.GameCamera.MoveSpeed = MoveSpeed;
    Result.GameCamera.OneMinusDragPercent = 1.0f - DragPercent;

    return Result;
}

inline camera CameraFpsCreate(input_state* InputState, v3 Pos, v3 View, f32 AspectRatio, f32 Near, f32 Far, f32 Fov,
                              f32 TurningVelocity, f32 Velocity)
{
    // NOTE: https://github.com/Erkaman/gl-movable-camera
    camera Result = CameraCreate_(InputState, Pos, View, V3(0, 1, 0), Normalize(Cross(Result.Up, Result.View)),
                                  AspectRatio, Near, Far, Fov);

    Result.Type = CameraType_Fps;
    Result.FpsCamera.TurningVelocity = TurningVelocity;
    Result.FpsCamera.Velocity = Velocity;

    return Result;
}

inline m4 CameraGetVP(render_state* RenderState, camera* Camera)
{
    m4 Result = {};
    m4 VTransform = LookAtM4(Camera->View, Camera->Up, Camera->Pos);
    m4 PTransform = RenderPerspProjM4(Camera->AspectRatio, Camera->Fov, Camera->Near, Camera->Far);

    Result = PTransform*VTransform;

    return Result;
}

inline m4 CameraGetVP(camera* Camera)
{
    m4 Result = {};
    m4 VTransform = LookAtM4(Camera->View, Camera->Up, Camera->Pos);
    m4 PTransform = RenderPerspProjM4(Camera->AspectRatio, Camera->Fov, Camera->Near, Camera->Far);

    Result = PTransform*VTransform;

    return Result;
}

inline void CameraUpdate(camera* Camera, input_frame* CurrInput, aabb2 WorldBounds)
{
    // NOTE: Camera http://in2gpu.com/2016/03/14/opengl-fps-camera-quaternion/
    switch (Camera->Type)
    {
        case CameraType_Game:
        {
            game_camera* GameCamera = &Camera->GameCamera;
            
            // NOTE: Apply drag to sliding
            GameCamera->Vel.xy *= GameCamera->OneMinusDragPercent;

            // NOTE: This is for PC debugging
            if (CurrInput->ZoomDelta != 0.0f)
            {
                GameCamera->Vel.z = CurrInput->ZoomDelta*GameCamera->ZoomSpeed;
            }

            f32 CameraOldZ = Camera->Pos.z;
            
            Camera->Pos.xy += CurrInput->FrameTime*GameCamera->Vel.xy;
            Camera->Pos += Camera->View*GameCamera->Vel.z; // NOTE: This is a instant change, not really a velocity
            GameCamera->Vel.z = 0.0f;

            // NOTE: Clamp camera to be inside our camera bounds
            v3 MinWorldPos = CameraGetMouseZeroPlanePos(Camera, V2(0, 0));
            v3 MaxWorldPos = CameraGetMouseZeroPlanePos(Camera, V2(1, 1));

            // NOTE: We first move our camera on each side to be in bounds if possible
#if !DEBUG_UNBLOCK_PLAYER_CAMERA
            v2 Translation = {};
            if (MinWorldPos.x < WorldBounds.Min.x)
            {
                Translation.x += WorldBounds.Min.x - MinWorldPos.x;
                GameCamera->Vel.x = 0.0f;
            }
            if (MaxWorldPos.x > WorldBounds.Max.x)
            {
                Translation.x += WorldBounds.Max.x - MaxWorldPos.x;
                GameCamera->Vel.x = 0.0f;
            }

            if (MinWorldPos.y < WorldBounds.Min.y)
            {
                Translation.y += WorldBounds.Min.y - MinWorldPos.y;
                GameCamera->Vel.y = 0.0f;
            }
            if (MaxWorldPos.y > WorldBounds.Max.y)
            {
                Translation.y += WorldBounds.Max.y - MaxWorldPos.y;
                GameCamera->Vel.y = 0.0f;
            }
            
            // TODO: There are some float errors here that make us shimmer between positions more. See if we can derive a better
            // more accurate way of doing this
            //DebugPrintLog("%f, %f, %f, %f, %f, %f, %f, %f\n", Camera->Pos.x, Camera->Pos.y, Translation.x, Translation.y,
            //              MinWorldPos.x, MinWorldPos.y, MaxWorldPos.x, MaxWorldPos.y);

            Camera->Pos.xy += Translation;
            
            // NOTE: Now that we moved the camera bounds, its possible its still bigger than world bounds, so we clamp
            v2 WorldDim = AabbGetDim(WorldBounds);
            if (WorldDim.x < (MaxWorldPos.x - MinWorldPos.x) ||
                WorldDim.y < (MaxWorldPos.y - MinWorldPos.y))
            {
                Camera->Pos.z = CameraOldZ;
            }
#endif

            // TODO: REMOVE
            //DebugPrintLog("%f, %f, %f\n", Camera->Pos.x, Camera->Pos.y, Camera->Pos.z);

            // NOTE: Clamp camera to a min zoom
            Camera->Pos.z = Max(Camera->Pos.z, 8.0f);            
        } break;

        case CameraType_Fps:
        {
            input_button MoveForward = InputGetButton(CurrInput, 'W');
            input_button MoveLeft = InputGetButton(CurrInput, 'A');
            input_button MoveBackward = InputGetButton(CurrInput, 'S');
            input_button MoveRight = InputGetButton(CurrInput, 'D');
            input_button SpeedUp = InputGetButton(CurrInput, 'M');
            input_button SlowDown = InputGetButton(CurrInput, 'N');
            
            if (SpeedUp.Down)
            {
                Camera->FpsCamera.Velocity += 0.1f;
            }
            if (SlowDown.Down)
            {
                Camera->FpsCamera.Velocity -= 0.1f;
                Camera->FpsCamera.Velocity = Max(Camera->FpsCamera.Velocity, 0.1f);
            }
    
            f32 Velocity = Camera->FpsCamera.Velocity;
            v3 MoveVel = {};
            if (MoveForward.Down)
            {
                MoveVel += Velocity*Camera->View;
            }
            if (MoveBackward.Down)
            {
                MoveVel -= Velocity*Camera->View;
            }

            if (MoveRight.Down)
            {
                MoveVel += Velocity*Camera->Right;
            }
            if (MoveLeft.Down)
            {
                MoveVel -= Velocity*Camera->Right;
            }
    
            Camera->Pos += MoveVel;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
}

#define DragCamera(InputState, UiState, Camera) DragCamera_(InputState, UiState, Camera, INTERACTION_FILE_LINE_ID())
inline void DragCamera_(input_state* InputState, ui_state* UiState, camera* Camera, u64 RefId)
{
    interaction_ref DragCameraRef = {};
    DragCameraRef.Id = RefId;

    input_frame* CurrInput = &InputState->CurrInput;
    input_pointer* CurrPointer = &CurrInput->MainPointer;

    // NOTE: Only do this interaction if we have 1 pointer and its pressed/held/released
    if ((CurrPointer->ButtonFlags & (MouseButtonFlag_PressedOrHeld | MouseButtonFlag_Released)) &&
        InputIsMainPointerOnly(CurrInput))
    {
        switch (Camera->Type)
        {
            case CameraType_Game:
            {
                if (InputInteractionsAreSame(DragCameraRef, InputState->PrevHot.Ref))
                {
                    // NOTE: Keep the interaction flowing through
                    drag_camera_interaction* DragCamera = &InputState->PrevHot.DragCamera;
                    InputAddInteraction(InputState, InputState->PrevHot);
                }
                else if (!UiState->MouseTouchingUi)
                {
                    interaction Interaction = {};
                    Interaction.Type = Interaction_DragCamera;
                    Interaction.Ref = DragCameraRef;
                    Interaction.DragCamera.Camera = Camera;
                    Interaction.DragCamera.GameCamera.NumSecondsHeld = 0.0f;
                    Interaction.DragCamera.GameCamera.PrevMouseP = CurrPointer->ScreenPos;
                
                    InputAddInteraction(InputState, Interaction);
                }
            } break;

            case CameraType_Fps:
            {
                input_pointer* PrevPointer = &InputState->PrevInput.MainPointer;

                if (InputInteractionsAreSame(DragCameraRef, InputState->PrevHot.Ref))
                {
                    // NOTE: Keep the interaction flowing through but update the input
                    interaction Interaction = InputState->PrevHot;
                    Interaction.DragCamera.FpsCamera.PrevMousePixelPos = PrevPointer->PixelPos;
                
                    InputAddInteraction(InputState, Interaction);
                }
                else if (!UiState->MouseTouchingUi)
                {
                    interaction Interaction = {};
                    Interaction.Type = Interaction_DragCamera;
                    Interaction.Ref = DragCameraRef;
                    Interaction.DragCamera.Camera = Camera;
                    Interaction.DragCamera.FpsCamera.PrevMousePixelPos = CurrPointer->PixelPos;
        
                    InputAddInteraction(InputState, Interaction);
                }
            } break;

            default:
            {
                InvalidCodePath;
            } break;
        }
    }
}

#define ZoomCamera(InputState, Camera) ZoomCamera_(InputState, Camera, INTERACTION_FILE_LINE_ID())
inline void ZoomCamera_(input_state* InputState, camera* Camera, u64 RefId)
{
    Assert(Camera->Type == CameraType_Game);

    interaction_ref ZoomCameraRef = {};
    ZoomCameraRef.Id = RefId;

    // NOTE: Because we only interact if we have held, we know that previous frame had 2 pointers and we can get prev inputs
    input_frame* PrevInput = &InputState->PrevInput;
    input_frame* CurrInput = &InputState->CurrInput;
    if (CurrInput->NumPointers == 2 && PrevInput->NumPointers == 2)
    {
        input_pointer* CurrPointers = CurrInput->Pointers;
        if (((CurrPointers[0].ButtonFlags & MouseButtonFlag_OnlyHeld) || (CurrPointers[0].ButtonFlags & MouseButtonFlag_Released)) &&
            ((CurrPointers[1].ButtonFlags & MouseButtonFlag_OnlyHeld) || (CurrPointers[1].ButtonFlags & MouseButtonFlag_Released)))
        {
            interaction Interaction = {};
            Interaction.Type = Interaction_ZoomCamera;
            Interaction.Ref = ZoomCameraRef;
            Interaction.ZoomCamera.Camera = Camera;
            InputAddInteraction(InputState, Interaction);
        }
    }
}

inline b32 CameraCanHandleInteraction(u32 Type)
{
    b32 Result = Type == Interaction_DragCamera;
    return Result;
}

INPUT_INTERACTION_HANDLER(CameraHandleInteraction)
{
    b32 Result = false;

    if (!(InputState->Hot.Type == Interaction_DragCamera ||
          InputState->Hot.Type == Interaction_ZoomCamera))
    {
        return Result;
    }

    input_frame* PrevInput = &InputState->PrevInput;
    input_frame* CurrInput = &InputState->CurrInput;
    f32 FrameTime = InputState->CurrInput.FrameTime;
    ui_state* UiState = &GameState->UiState;
    
    switch (InputState->Hot.Type)
    {
        case Interaction_DragCamera:
        {
            drag_camera_interaction* HotDragCameraInteraction = &InputState->Hot.DragCamera;
            camera* Camera = HotDragCameraInteraction->Camera;
            input_pointer* MainPointer = &CurrInput->MainPointer;

            switch (Camera->Type)
            {
                case CameraType_Game:
                {
                    drag_game_camera* DragGameCamera = &HotDragCameraInteraction->GameCamera;

                    // NOTE: We are swiping vertically or horizontally with one finger
                    DragGameCamera->NumSecondsHeld += FrameTime;
                    
                    v3 PrevZeroPlane = CameraGetMouseZeroPlanePos(Camera, DragGameCamera->PrevMouseP);
                    v2 Dir = ((PrevZeroPlane.xy - MainPointer->ZeroPlanePos.xy) / (2.0f*DragGameCamera->NumSecondsHeld));
                    Camera->GameCamera.Vel.xy = Camera->GameCamera.MoveSpeed*Dir;
                    Camera->GameCamera.Vel.z = 0.0f;
                        
                    InputState->PrevHot = InputState->Hot;
                } break;

                case CameraType_Fps:
                {
                    drag_fps_camera DragFpsCamera = HotDragCameraInteraction->FpsCamera;

                    f32 Head = -(f32)(MainPointer->PixelPos.x - DragFpsCamera.PrevMousePixelPos.x);
                    f32 Pitch = -(f32)(MainPointer->PixelPos.y - DragFpsCamera.PrevMousePixelPos.y);

                    // NOTE: Rotate about the up vector and right vector
                    Camera->View = RotateVectorAroundAxis(Camera->View, Camera->Up, Head*Camera->FpsCamera.TurningVelocity);
                    Camera->View = RotateVectorAroundAxis(Camera->View, Camera->Right, Pitch*Camera->FpsCamera.TurningVelocity);

                    // NOTE: Update right vector
                    Camera->Right = Normalize(Cross(Camera->Up, Camera->View));
                        
                    InputState->PrevHot = InputState->Hot;
                } break;
                    
                default:
                {
                    InvalidCodePath;
                } break;
            }
            
            Result = true;
        } break;

        case Interaction_ZoomCamera:
        {
            // NOTE: We are doing a pinch to zoom
            zoom_camera_interaction* ZoomCamera = &InputState->Hot.ZoomCamera;
            input_pointer* CurrPointers = CurrInput->Pointers;
            input_pointer* PrevPointers = PrevInput->Pointers;

            // NOTE: We project hte prev pos using new camera to avoid oscillating if we naively take prev zero plane pos
            v3 PrevZeroPlane1 = CameraGetMouseZeroPlanePos(ZoomCamera->Camera, PrevPointers[0].ScreenPos);
            v3 PrevZeroPlane2 = CameraGetMouseZeroPlanePos(ZoomCamera->Camera, PrevPointers[1].ScreenPos);
            
            f32 Distance1 = Length(PrevZeroPlane1.xy - CurrPointers[0].ZeroPlanePos.xy);
            f32 Distance2 = Length(PrevZeroPlane2.xy - CurrPointers[1].ZeroPlanePos.xy);
            f32 AvgDistance = 0.5f*(Distance1 + Distance2);
            
            f32 DistanceBetween1 = Length(PrevZeroPlane1.xy - PrevZeroPlane2.xy);
            f32 DistanceBetween2 = Length(CurrPointers[0].ZeroPlanePos.xy - CurrPointers[1].ZeroPlanePos.xy);
            f32 Direction = Sign(DistanceBetween2 - DistanceBetween1);

            DebugPrintLog("Zoom Interatc: %f, %f, %f, %f\n", Distance1, Distance2, AvgDistance, Direction);
            
            ZoomCamera->Camera->GameCamera.Vel.xy = {};
            ZoomCamera->Camera->GameCamera.Vel.z = Direction*AvgDistance;
                        
            InputState->PrevHot = InputState->Hot;

            Result = true;
        } break;
    }

    return Result;
}

inline v3 CameraGetMouseZeroPlanePos(camera* Camera, v2 MouseP)
{
    // NOTE: Finds the mouse world pos where they intersect the plane at zero height
    v3 MouseStart = {};
    v3 MouseDir = {};
    {
        v3 MouseWorld = RenderScreenToWorld(Camera, MouseP);
        MouseStart = Camera->Pos;
        MouseDir = Normalize(MouseWorld - MouseStart);
    }

    f32 TargetZ = 1.0f;
    f32 Multiplier = (TargetZ - MouseStart.z) / MouseDir.z;
    
    v3 MouseWorld = MouseStart + MouseDir*Multiplier;

    return MouseWorld;
}
