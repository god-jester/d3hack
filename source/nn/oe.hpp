#pragma once

namespace nn::oe {

enum OperationMode {
  OperationMode_Handheld,
  OperationMode_Docked,
};

enum PerformanceMode {
  PerformanceMode_Normal = 0,
  PerformanceMode_Boost = 1,
  PerformanceMode_Invalid = -1,
};

enum FocusHandlingMode {
  FocusHandlingMode_SuspendHomeSleep,
  FocusHandlingMode_NoSuspend,
  FocusHandlingMode_SuspendHomeSleepNotify,
  FocusHandlingMode_AlwaysSuspend
};

enum FocusState {
  FocusState_InFocus = 1,
  FocusState_OutOfFocus = 2,
  FocusState_Background = 3,
};

struct DisplayVersion {
  char name[16];
};

typedef s32 PerformanceConfiguration;

void Initialize();
void FinishStartupLogo();
void EnableGamePlayRecording(void *, u64);
void SetExpectedVolumeBalance(f32, f32);
void SetPerformanceConfiguration(nn::oe::PerformanceMode,
                                 nn::oe::PerformanceConfiguration);
void SetResumeNotificationEnabled(bool);
void SetOperationModeChangedNotificationEnabled(bool);
void SetPerformanceModeChangedNotificationEnabled(bool);
void SetFocusHandlingMode(nn::oe::FocusHandlingMode);
void setScreenShotImageOrientation(int32 /* nn::album::ImageOrientation */);
void SetUserInactivityDetectionTimeExtended(bool);
bool IsUserInactivityDetectionTimeExtended();
bool TryPopNotificationMessage(u32 *);
bool TryPopLaunchParameter(size_t *, void *, size_t);
void GetExpectedVolumeBalance(f32 *, f32 *);
void GetDisplayVersion(DisplayVersion *);
FocusState GetCurrentFocusState();
OperationMode GetOperationMode();
PerformanceMode GetPerformanceMode();
// nn::settings::LanguageCode GetDesiredLanguage();

} // namespace nn::oe
