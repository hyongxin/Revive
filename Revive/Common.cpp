#include "Common.h"
#include "Extras/OVR_Math.h"

#include <stdio.h>

// Common structures

ovrTextureSwapChainData::ovrTextureSwapChainData(vr::ETextureType api, ovrTextureSwapChainDesc desc)
	: ApiType(api)
	, Length(REV_SWAPCHAIN_LENGTH)
	, CurrentIndex(0)
	, Desc(desc)
	, Overlay(vr::k_ulOverlayHandleInvalid)
{
	memset(Textures, 0, sizeof(Textures));
}

ovrMirrorTextureData::ovrMirrorTextureData(vr::ETextureType api, ovrMirrorTextureDesc desc)
	: ApiType(api)
	, Desc(desc)
{
}

ovrHmdStruct::ovrHmdStruct()
	: ShouldQuit(false)
	, IsVisible(false)
	, FrameIndex(0)
	, StatsIndex(0)
	, PixelsPerDisplayPixel(0.0f)
	, Compositor(nullptr)
	, Input(new InputManager())
	, Details(new SessionDetails())
{
	memset(StringBuffer, 0, sizeof(StringBuffer));
	memset(&ResetStats, 0, sizeof(ResetStats));
	memset(Stats, 0, sizeof(Stats));
	memset(TouchOffset, 0, sizeof(TouchOffset));

	LoadSettings();
}

// Common functions

OVR::Matrix4f rev_HmdMatrixToOVRMatrix(vr::HmdMatrix34_t m)
{
	OVR::Matrix4f r;
	memcpy(r.M, m.m, sizeof(vr::HmdMatrix34_t));
	return r;
}

OVR::Vector3f rev_HmdVectorToOVRVector(vr::HmdVector3_t v)
{
	OVR::Vector3f r;
	r.x = v.v[0];
	r.y = v.v[1];
	r.z = v.v[2];
	return r;
}

vr::HmdMatrix34_t rev_OvrPoseToHmdMatrix(ovrPosef pose)
{
	vr::HmdMatrix34_t result;
	OVR::Matrix4f matrix(pose);
	memcpy(result.m, matrix.M, sizeof(result.m));
	return result;
}

vr::VRTextureBounds_t rev_FovPortToTextureBounds(ovrEyeType eye, ovrFovPort fov)
{
	vr::VRTextureBounds_t result;

	// Get the headset field-of-view
	float left, right, top, bottom;
	vr::VRSystem()->GetProjectionRaw((vr::EVREye)eye, &left, &right, &top, &bottom);

	// Adjust the bounds based on the field-of-view in the game
	result.uMin = 0.5f + 0.5f * left / fov.LeftTan;
	result.uMax = 0.5f + 0.5f * right / fov.RightTan;
	result.vMin = 0.5f - 0.5f * bottom / fov.UpTan;
	result.vMax = 0.5f - 0.5f * top / fov.DownTan;

	// Sanitize the output
	if (result.uMin < 0.0)
		result.uMin = 0.0;
	if (result.uMax > 1.0)
		result.uMax = 1.0;

	if (result.vMin < 0.0)
		result.vMin = 0.0;
	if (result.vMax > 1.0)
		result.vMax = 1.0;

	return result;
}

void ovrHmdStruct::LoadSettings()
{
	Deadzone = ovr_GetFloat(nullptr, REV_KEY_THUMB_DEADZONE, REV_DEFAULT_THUMB_DEADZONE);
	Sensitivity = ovr_GetFloat(nullptr, REV_KEY_THUMB_SENSITIVITY, REV_DEFAULT_THUMB_SENSITIVITY);
	ToggleGrip = (revGripType)ovr_GetInt(nullptr, REV_KEY_TOGGLE_GRIP, REV_DEFAULT_TOGGLE_GRIP);
	ToggleDelay = ovr_GetFloat(nullptr, REV_KEY_TOGGLE_DELAY, REV_DEFAULT_TOGGLE_DELAY);

	OVR::Vector3f angles(
		OVR::DegreeToRad(ovr_GetFloat(this, REV_KEY_TOUCH_PITCH, REV_DEFAULT_TOUCH_PITCH)),
		OVR::DegreeToRad(ovr_GetFloat(this, REV_KEY_TOUCH_YAW, REV_DEFAULT_TOUCH_YAW)),
		OVR::DegreeToRad(ovr_GetFloat(this, REV_KEY_TOUCH_ROLL, REV_DEFAULT_TOUCH_ROLL))
	);
	OVR::Vector3f offset(
		ovr_GetFloat(this, REV_KEY_TOUCH_X, REV_DEFAULT_TOUCH_X),
		ovr_GetFloat(this, REV_KEY_TOUCH_Y, REV_DEFAULT_TOUCH_Y),
		ovr_GetFloat(this, REV_KEY_TOUCH_Z, REV_DEFAULT_TOUCH_Z)
	);

	// Check if the offset matrix needs to be updated
	if (angles != RotationOffset || offset != PositionOffset)
	{
		RotationOffset = angles;
		PositionOffset = offset;

		for (int i = 0; i < ovrHand_Count; i++)
		{
			OVR::Matrix4f yaw = OVR::Matrix4f::RotationY(angles.y);
			OVR::Matrix4f pitch = OVR::Matrix4f::RotationX(angles.x);
			OVR::Matrix4f roll = OVR::Matrix4f::RotationZ(angles.z);

			// Mirror the right touch controller offsets
			if (i == ovrHand_Right)
			{
				yaw.Invert();
				roll.Invert();
				offset.x *= -1.0f;
			}

			OVR::Matrix4f matrix(yaw * pitch * roll);
			matrix.SetTranslation(offset);
			memcpy(TouchOffset[i].m, matrix.M, sizeof(vr::HmdMatrix34_t));
		}
	}
}
