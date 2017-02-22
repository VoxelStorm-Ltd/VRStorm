#include "vrstorm.h"
#include <iostream>
#include <thread>
#include "dynamic_load.h"
#include "vectorstorm/vector/vector3.h"

namespace vrstorm {

manager::manager()
  : input_controller(*this) {
  /// Default constructor
}

manager::~manager() {
  /// Default destructor
  shutdown();
}

void manager::init() {
  /// Initialise the virtual reality system
  #ifndef VRSTORM_DISABLED
    try {
      // dynamic library load
      #ifdef PLATFORM_WINDOWS
        lib = load_dynamic({"./openvr_api.dll", "openvr_api.dll"});
      #else
        #ifdef NDEBUG
          lib = load_dynamic({"./libopenvr_api.so", "libopenvr_api.so"});
        #else
          lib = load_dynamic({"./libopenvr_api.so.dbg", "libopenvr_api.so.dbg", "./libopenvr_api.so", "libopenvr_api.so"});
        #endif // NDEBUG
      #endif // PLATFORM_WINDOWS
      if(!lib) {
        std::cout << "VRStorm: No OpenVR dynamic library found, not initialising." << std::endl;
        shutdown();
        return;
      }

      //bool (*VR_IsRuntimeInstalled)() = (bool(*)())dlsym(lib, "VR_IsRuntimeInstalled");
      //bool (*VR_IsRuntimeInstalled)() = (decltype(&vr::VR_IsRuntimeInstalled))dlsym(lib, "VR_IsRuntimeInstalled");
      //auto VR_IsRuntimeInstalled = reinterpret_cast<decltype(&vr::VR_IsRuntimeInstalled)>(dlsym(lib, "VR_IsRuntimeInstalled"));
      auto VR_IsRuntimeInstalled                 = load_symbol<decltype(&vr::VR_IsRuntimeInstalled                )>(lib, "VR_IsRuntimeInstalled");
      auto VR_IsHmdPresent                       = load_symbol<decltype(&vr::VR_IsHmdPresent                      )>(lib, "VR_IsHmdPresent");
      auto VR_RuntimePath                        = load_symbol<decltype(&vr::VR_RuntimePath                       )>(lib, "VR_RuntimePath");
      auto VR_InitInternal                       = load_symbol<decltype(&vr::VR_InitInternal                      )>(lib, "VR_InitInternal");
      auto VR_IsInterfaceVersionValid            = load_symbol<decltype(&vr::VR_IsInterfaceVersionValid           )>(lib, "VR_IsInterfaceVersionValid");
      auto VR_GetVRInitErrorAsEnglishDescription = load_symbol<decltype(&vr::VR_GetVRInitErrorAsEnglishDescription)>(lib, "VR_GetVRInitErrorAsEnglishDescription");
      auto VR_GetGenericInterface                = load_symbol<decltype(&vr::VR_GetGenericInterface               )>(lib, "VR_GetGenericInterface");
      auto VR_GetInitToken                       = load_symbol<decltype(&vr::VR_GetInitToken                      )>(lib, "VR_GetInitToken");

      // preliminary checks
      if(!VR_IsRuntimeInstalled()) {
        std::cout << "VRStorm: No VR runtime installed." << std::endl;
        shutdown();
        return;
      }
      std::cout << "VRStorm: VR runtime installed in " << VR_RuntimePath() << std::endl;
      if(!VR_IsHmdPresent()) {
        #ifdef PLATFORM_WINDOWS
          std::cout << "VRStorm: OpenVR says there's no head mounted display present, but we're on Windows and it often lies, so we'll try to initialise anyway." << std::endl;
        #else
          std::cout << "VRStorm: No head mounted display present." << std::endl;
          shutdown();
          return;
        #endif // PLATFORM_WINDOWS
      }
      std::cout << "VRStorm: Head mounted display may be present, initialising..." << std::endl;

      // initialise the vr system
      vr::EVRInitError vr_error = vr::VRInitError_None;

      // the following is a replacement for hmd_handle = vr::VR_Init(&vr_error, vr::VRApplication_Scene);
      vr::VRToken() = VR_InitInternal(&vr_error, vr::VRApplication_Scene);
      vr::COpenVRContext &vr_ctx = vr::OpenVRInternal_ModuleContext();
      vr_ctx.Clear();
      if(vr_error != vr::VRInitError_None) {
        std::cout << "VRStorm: Unable to init VR runtime: " << VR_GetVRInitErrorAsEnglishDescription(vr_error) << std::endl;
        //std::cout << "VRStorm: VR init error: " << VR_GetVRInitErrorAsSymbol(vr_error) << std::endl;
        return;
      }
      if(!VR_IsInterfaceVersionValid(vr::IVRSystem_Version)) {
        vr_error = vr::VRInitError_Init_InterfaceNotFound;
        std::cout << "VRStorm: Unable to init VR runtime, interface not found: " << VR_GetVRInitErrorAsEnglishDescription(vr_error) << std::endl;
        shutdown();
        return;
      }
      // the following replaces CheckClear();
      if(vr::VRToken() != VR_GetInitToken()) {
        vr_ctx.Clear();
        vr::VRToken() = VR_GetInitToken();
      }
      // the following replaces hmd_handle = vr::VRSystem();
      hmd_handle = static_cast<vr::IVRSystem*>(VR_GetGenericInterface(vr::IVRSystem_Version, &vr_error));

      if(!hmd_handle) {
        std::cout << "VRStorm: Unable to init VR runtime, although no error was returned!" << std::endl;
        shutdown();
        return;
      }
      std::string vr_driver  = get_tracked_device_string(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
      std::string vr_display = get_tracked_device_string(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
      std::cout << "VRStorm: Initialised, driver: " << vr_driver << ", display: " << vr_display << std::endl;

      {
        vec2<uint32_t> this_render_target_size;
        hmd_handle->GetRecommendedRenderTargetSize(&this_render_target_size.x, &this_render_target_size.y);
        render_target_size = this_render_target_size;
      }
      std::cout << "VRStorm: Render target size: " << render_target_size << std::endl;

      // initialise the compositor, the following replaces compositor = vr::VRCompositor();
      compositor = static_cast<vr::IVRCompositor*>(VR_GetGenericInterface(vr::IVRCompositor_Version, &vr_error));
      if(!compositor) {
        std::cout << "VRStorm: Unable to initialise VR compositor: " << VR_GetVRInitErrorAsEnglishDescription(vr_error) << std::endl;
        shutdown();
        return;
      }
      /*
      if(compositor->GetVSync()) {
        std::cout << "VRStorm: Compositor vsync enabled" << std::endl;
      } else {
        std::cout << "VRStorm: Compositor vsync disabled" << std::endl;
      }
      */
      if(compositor->IsFullscreen()) {
        std::cout << "VRStorm: Compositor is fullscreen" << std::endl;
      } else {
        std::cout << "VRStorm: Compositor is not fullscreen" << std::endl;
      }
      //std::cout << "VRStorm: Compositor gamma: " << compositor->GetGamma() << std::endl;
      float const display_freq = hmd_handle->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
      float const frame_duration = 1.0f / display_freq;
      std::cout << "VRStorm: Display frequency: " << display_freq << "Hz, frame duration " << frame_duration << "s" << std::endl;
      float const vsync_to_photon_time = hmd_handle->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);
      std::cout << "VRStorm: Time from vsync to photons: " << vsync_to_photon_time << "s" << std::endl;
      // in the loop, predicted pose time for GetDeviceToAbsoluteTrackingPose as per https://github.com/ValveSoftware/openvr/wiki/IVRSystem::GetDeviceToAbsoluteTrackingPose:
      //   float time_since_vsync;
      //   hmd_handle->GetTimeSinceLastVsync(&time_since_vsync, nullptr);
      //   float time_to_photons = frame_duration - time_since_vsync + vsync_to_photon_time;
      switch(compositor->GetTrackingSpace()) {
      case vr::TrackingUniverseSeated:                                          // Poses are provided relative to the seated zero pose
        std::cout << "VRStorm: Tracking relative to seated position." << std::endl;
        break;
      case vr::TrackingUniverseStanding:                                        // Poses are provided relative to the safe bounds configured by the user
        std::cout << "VRStorm: Tracking relative to standing space." << std::endl;
        break;
      case vr::TrackingUniverseRawAndUncalibrated:                              // Poses are provided in the coordinate system defined by the driver. You probably don't want this one.
        std::cout << "VRStorm: Tracking relative to raw uncalibrated coordinates." << std::endl;
        break;
      default:
        std::cout << "VRStorm: Tracking relative to unknown space: " << compositor->GetTrackingSpace() << std::endl;
        break;
      }

      // initialise the chaperone, the following replaces vr::IVRChaperone *chaperone = vr::VRChaperone();
      vr::IVRChaperone *chaperone = static_cast<vr::IVRChaperone*>(VR_GetGenericInterface(vr::IVRChaperone_Version, &vr_error));
      if(chaperone) {
        // we've just requested startup, so base stations may not be tracking yet - this is normal
        switch(chaperone->GetCalibrationState()) {
        case vr::ChaperoneCalibrationState_OK:
          std::cout << "VRStorm: Chaperone is fully calibrated and working correctly" << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Warning:
          break;
        case vr::ChaperoneCalibrationState_Warning_BaseStationMayHaveMoved:
          std::cout << "VRStorm: Chaperone WARNING: A base station thinks that it might have moved." << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Warning_BaseStationRemoved:
          std::cout << "VRStorm: Chaperone WARNING: There are fewer base stations than when calibrated." << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Warning_SeatedBoundsInvalid:
          std::cout << "VRStorm: Chaperone WARNING: Seated bounds haven't been calibrated for the current tracking centre." << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Error:
          std::cout << "VRStorm: Chaperone ERROR: The UniverseID is invalid." << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Error_BaseStationUninitalized:
          std::cout << "VRStorm: Chaperone ERROR: Tracking centre hasn't been calibrated for at least one of the base stations" << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Error_BaseStationConflict:
          std::cout << "VRStorm: Chaperone ERROR: Tracking centre is calibrated, but base stations disagree on the tracking space." << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Error_PlayAreaInvalid:
          std::cout << "VRStorm: Chaperone ERROR: Play area hasn't been calibrated for the current tracking centre." << std::endl;
          break;
        case vr::ChaperoneCalibrationState_Error_CollisionBoundsInvalid:
          std::cout << "VRStorm: Chaperone ERROR: Collision bounds haven't been calibrated for the current tracking centre." << std::endl;
          break;
        default:
          std::cout << "VRStorm: Unknown chaperone calibration state " << chaperone->GetCalibrationState() << std::endl;
          break;
        }
        vec2f play_area_size;
        if(chaperone->GetPlayAreaSize(&play_area_size.x, &play_area_size.y)) {
          std::cout << "VRStorm: Play area size: " << render_target_size << std::endl;
        } else {
          std::cout << "VRStorm: Could not get play area size";                 // this is not an error in the case of seating or standing configurations
          if(vr_error == 0) {
            std::cout << "." << std::endl;
          } else {
            std::cout << ": " << VR_GetVRInitErrorAsEnglishDescription(vr_error) << std::endl;
          }
        }
        std::array<vec3f, 4> play_area_rect;
        {
          vr::HmdQuad_t play_area_quad;
          if(chaperone->GetPlayAreaRect(&play_area_quad)) {
            for(unsigned int corner = 0; corner != 4; ++corner) {
              play_area_rect[corner].assign(play_area_quad.vCorners[corner].v[0],
                                            play_area_quad.vCorners[corner].v[1],
                                            play_area_quad.vCorners[corner].v[2]);
            }
            std::cout << "VRStorm: Play area rectangle: " << play_area_rect[0] << ", " << play_area_rect[1] << ", " << play_area_rect[2] << ", " << play_area_rect[3] << std::endl;
          } else {
            std::cout << "VRStorm: Could not get play area rectangle";          // this is not an error in the case of seating or standing configurations
            if(vr_error == 0) {
              std::cout << "." << std::endl;
            } else {
              std::cout << ": " << VR_GetVRInitErrorAsEnglishDescription(vr_error) << std::endl;
            }
          }
        }
      } else {
        std::cout << "VRStorm: Unable to initialise chaperone: " << VR_GetVRInitErrorAsEnglishDescription(vr_error) << std::endl;
        //shutdown();
        //return;
      }

      // identify tracked devices
      std::vector<unsigned int> controller_ids;
      for(unsigned int i = 0; i != vr::k_unMaxTrackedDeviceCount; ++i) {
        if(!hmd_handle->IsTrackedDeviceConnected(i)) {
          continue;
        }
        if(i == vr::k_unTrackedDeviceIndex_Hmd) {
          std::cout << "VRStorm: Primary HMD(" << i << "): ";
        } else {
          std::cout << "VRStorm: Device " << i << ": ";
        }
        switch(hmd_handle->GetTrackedDeviceClass(i)) {
        case vr::TrackedDeviceClass_Invalid:
          std::cout << "Invalid device:" << std::endl;
          break;
        case vr::TrackedDeviceClass_HMD:                                        // Head-Mounted Displays
          if(i != vr::k_unTrackedDeviceIndex_Hmd) {
            std::cout << "HMD: ";
          }
          break;
        case vr::TrackedDeviceClass_Controller:                                 // Tracked controllers
          std::cout << "Controller: ";
          controller_ids.emplace_back(i);
          break;
        case vr::TrackedDeviceClass_TrackingReference:                          // Camera and base stations that serve as tracking reference points
          std::cout << "Tracking reference: ";
          break;
        case vr::TrackedDeviceClass_Other:
          std::cout << "Unknown device: " << std::endl;
          break;
        default:
          std::cout << "Unrecognised (class " << hmd_handle->GetTrackedDeviceClass(i) << ") device: " << std::endl;
          break;
        }
        //std::cout << get_tracked_device_string(i, vr::Prop_ManufacturerName_String) << " " << get_tracked_device_string(i, vr::Prop_ModelNumber_String) << " " << get_tracked_device_string(i, vr::Prop_HardwareRevision_String);
        std::cout << get_tracked_device_string(i, vr::Prop_ManufacturerName_String) << " " << get_tracked_device_string(i, vr::Prop_ModelNumber_String) << " tracked by " << get_tracked_device_string(i, vr::Prop_TrackingSystemName_String);
        switch(hmd_handle->GetTrackedDeviceActivityLevel(i)) {
        case vr::k_EDeviceActivityLevel_Unknown:
          std::cout << ", status unknown";
          break;
        case vr::k_EDeviceActivityLevel_Idle:
          std::cout << ", idle";
          break;
        case vr::k_EDeviceActivityLevel_UserInteraction:
          std::cout << ", interactive";
          break;
        case vr::k_EDeviceActivityLevel_UserInteraction_Timeout:
          std::cout << ", timeout";
          break;
        case vr::k_EDeviceActivityLevel_Standby:
          std::cout << ", standby";
          break;
        default:
          std::cout << ", status unrecognised";
          break;
        }
        std::cout << std::endl;
        #ifdef DEBUG_VRSTORM
          std::cout << std::boolalpha;
          std::cout << "VRStorm: DEBUG: TrackingSystemName_String            " << get_tracked_device_string(                   i, vr::Prop_TrackingSystemName_String           ) << std::endl; // lighthouse
          std::cout << "VRStorm: DEBUG: ModelNumber_String                   " << get_tracked_device_string(                   i, vr::Prop_ModelNumber_String                  ) << std::endl; // Vive MV
          std::cout << "VRStorm: DEBUG: SerialNumber_String                  " << get_tracked_device_string(                   i, vr::Prop_SerialNumber_String                 ) << std::endl; // LHR-F3A6281E
          std::cout << "VRStorm: DEBUG: RenderModelName_String               " << get_tracked_device_string(                   i, vr::Prop_RenderModelName_String              ) << std::endl; // generic_hmd | lh_basestation_vive
          std::cout << "VRStorm: DEBUG: WillDriftInYaw_Bool                  " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_WillDriftInYaw_Bool                 ) << std::endl; // 0
          std::cout << "VRStorm: DEBUG: ManufacturerName_String              " << get_tracked_device_string(                   i, vr::Prop_ManufacturerName_String             ) << std::endl; // HTC
          std::cout << "VRStorm: DEBUG: TrackingFirmwareVersion_String       " << get_tracked_device_string(                   i, vr::Prop_TrackingFirmwareVersion_String      ) << std::endl; // 1462663157 steamservices@firmware-win32 2016-05-08 FPGA 1.6
          std::cout << "VRStorm: DEBUG: HardwareRevision_String              " << get_tracked_device_string(                   i, vr::Prop_HardwareRevision_String             ) << std::endl; // product 128 rev 2.1.0 lot 2000/0/0 0
          std::cout << "VRStorm: DEBUG: AllWirelessDongleDescriptions_String " << get_tracked_device_string(                   i, vr::Prop_AllWirelessDongleDescriptions_String) << std::endl; // 3580A4F484=1461100729;ADCC7BC54A=1461100729
          std::cout << "VRStorm: DEBUG: ConnectedWirelessDongle_String       " << get_tracked_device_string(                   i, vr::Prop_ConnectedWirelessDongle_String      ) << std::endl; // ""
          std::cout << "VRStorm: DEBUG: DeviceIsWireless_Bool                " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_DeviceIsWireless_Bool               ) << std::endl; // false
          std::cout << "VRStorm: DEBUG: DeviceIsCharging_Bool                " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_DeviceIsCharging_Bool               ) << std::endl; // false
          std::cout << "VRStorm: DEBUG: DeviceBatteryPercentage_Float        " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DeviceBatteryPercentage_Float       ) << std::endl; // 0
          //std::cout << "VRStorm: DEBUG: StatusDisplayTransform_Matrix34      " << hmd_handle->GetMatrix34TrackedDeviceProperty(i, vr::Prop_StatusDisplayTransform_Matrix34     ) << std::endl;
          std::cout << "VRStorm: DEBUG: Firmware_UpdateAvailable_Bool        " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_Firmware_UpdateAvailable_Bool       ) << std::endl; // false
          std::cout << "VRStorm: DEBUG: Firmware_ManualUpdate_Bool           " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_Firmware_ManualUpdate_Bool          ) << std::endl; // false
          std::cout << "VRStorm: DEBUG: Firmware_ManualUpdateURL_String      " << get_tracked_device_string(                   i, vr::Prop_Firmware_ManualUpdateURL_String     ) << std::endl; // https://developer.valvesoftware.com/wiki/SteamVR/HowTo_Update_Firmware
          std::cout << "VRStorm: DEBUG: HardwareRevision_Uint64              " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_HardwareRevision_Uint64             ) << std::endl; // 2164327680
          std::cout << "VRStorm: DEBUG: FirmwareVersion_Uint64               " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_FirmwareVersion_Uint64              ) << std::endl; // 1462663157
          std::cout << "VRStorm: DEBUG: FPGAVersion_Uint64                   " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_FPGAVersion_Uint64                  ) << std::endl; // 262
          std::cout << "VRStorm: DEBUG: VRCVersion_Uint64                    " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_VRCVersion_Uint64                   ) << std::endl; // 1465809477
          std::cout << "VRStorm: DEBUG: RadioVersion_Uint64                  " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_RadioVersion_Uint64                 ) << std::endl; // 1466630404
          std::cout << "VRStorm: DEBUG: DongleVersion_Uint64                 " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_DongleVersion_Uint64                ) << std::endl; // 1461100729
          std::cout << "VRStorm: DEBUG: BlockServerShutdown_Bool             " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_BlockServerShutdown_Bool            ) << std::endl; // false
          std::cout << "VRStorm: DEBUG: CanUnifyCoordinateSystemWithHmd_Bool " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_CanUnifyCoordinateSystemWithHmd_Bool) << std::endl; // false
          std::cout << "VRStorm: DEBUG: ContainsProximitySensor_Bool         " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_ContainsProximitySensor_Bool        ) << std::endl; // true
          std::cout << "VRStorm: DEBUG: DeviceProvidesBatteryStatus_Bool     " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_DeviceProvidesBatteryStatus_Bool    ) << std::endl; // false
          std::cout << "VRStorm: DEBUG: DeviceCanPowerOff_Bool               " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_DeviceCanPowerOff_Bool              ) << std::endl; // false
          std::cout << "VRStorm: DEBUG: Firmware_ProgrammingTarget_String    " << get_tracked_device_string(                   i, vr::Prop_Firmware_ProgrammingTarget_String   ) << std::endl; // LHR-F3A6281E
          std::cout << "VRStorm: DEBUG: DeviceClass_Int32                    " << hmd_handle->GetInt32TrackedDeviceProperty(   i, vr::Prop_DeviceClass_Int32                   ) << std::endl; // 1
          std::cout << "VRStorm: DEBUG: HasCamera_Bool                       " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_HasCamera_Bool                      ) << std::endl; // true
          std::cout << "VRStorm: DEBUG: DriverVersion_String                 " << get_tracked_device_string(                   i, vr::Prop_DriverVersion_String                ) << std::endl; // ""
          std::cout << "VRStorm: DEBUG: Firmware_ForceUpdateRequired_Bool    " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_Firmware_ForceUpdateRequired_Bool   ) << std::endl; // false
          switch(hmd_handle->GetTrackedDeviceClass(i)) {
          case vr::TrackedDeviceClass_HMD:                                      // Head-Mounted Displays
            std::cout << "VRStorm: DEBUG: ReportsTimeSinceVSync_Bool                   " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_ReportsTimeSinceVSync_Bool                  ) << std::endl; // true
            std::cout << "VRStorm: DEBUG: SecondsFromVsyncToPhotons_Float              " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_SecondsFromVsyncToPhotons_Float             ) << std::endl; // 0.0111111
            std::cout << "VRStorm: DEBUG: DisplayFrequency_Float                       " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayFrequency_Float                      ) << std::endl; // 90
            std::cout << "VRStorm: DEBUG: UserIpdMeters_Float                          " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_UserIpdMeters_Float                         ) << std::endl; // 0.0647
            std::cout << "VRStorm: DEBUG: CurrentUniverseId_Uint64                     " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_CurrentUniverseId_Uint64                    ) << std::endl; // 1475689499
            std::cout << "VRStorm: DEBUG: PreviousUniverseId_Uint64                    " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_PreviousUniverseId_Uint64                   ) << std::endl; // 0
            std::cout << "VRStorm: DEBUG: DisplayFirmwareVersion_Uint64                " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_DisplayFirmwareVersion_Uint64               ) << std::endl; // 2097432
            std::cout << "VRStorm: DEBUG: IsOnDesktop_Bool                             " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_IsOnDesktop_Bool                            ) << std::endl; // false
            std::cout << "VRStorm: DEBUG: DisplayMCType_Int32                          " << hmd_handle->GetInt32TrackedDeviceProperty(   i, vr::Prop_DisplayMCType_Int32                         ) << std::endl; // 1
            std::cout << "VRStorm: DEBUG: DisplayMCOffset_Float                        " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayMCOffset_Float                       ) << std::endl; // -0.498039
            std::cout << "VRStorm: DEBUG: DisplayMCScale_Float                         " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayMCScale_Float                        ) << std::endl; // 0.125
            std::cout << "VRStorm: DEBUG: EdidVendorID_Int32                           " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_EdidVendorID_Int32                          ) << std::endl; // 0
            std::cout << "VRStorm: DEBUG: DisplayMCImageLeft_String                    " << get_tracked_device_string(                   i, vr::Prop_DisplayMCImageLeft_String                   ) << std::endl; // Green_46GA163P002719_mura_analyzes.mc
            std::cout << "VRStorm: DEBUG: DisplayMCImageRight_String                   " << get_tracked_device_string(                   i, vr::Prop_DisplayMCImageRight_String                  ) << std::endl; // Green_46HA163W001489_mura_analyzes.mc
            std::cout << "VRStorm: DEBUG: DisplayGCBlackClamp_Float                    " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayGCBlackClamp_Float                   ) << std::endl; // 0.0117647
            std::cout << "VRStorm: DEBUG: EdidProductID_Int32                          " << hmd_handle->GetInt32TrackedDeviceProperty(   i, vr::Prop_EdidProductID_Int32                         ) << std::endl; // 43521
            //std::cout << "VRStorm: DEBUG: CameraToHeadTransform_Matrix34               " << hmd_handle->GetMatrix34TrackedDeviceProperty(i, vr::Prop_CameraToHeadTransform_Matrix34              ) << std::endl;
            std::cout << "VRStorm: DEBUG: DisplayGCType_Int32                          " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayGCType_Int32                         ) << std::endl; // 0
            std::cout << "VRStorm: DEBUG: DisplayGCOffset_Float                        " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayGCOffset_Float                       ) << std::endl; // -0.203125
            std::cout << "VRStorm: DEBUG: DisplayGCScale_Float                         " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayGCScale_Float                        ) << std::endl; // 0.166667
            std::cout << "VRStorm: DEBUG: DisplayGCPrescale_Float                      " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayGCPrescale_Float                     ) << std::endl; // 0.95
            std::cout << "VRStorm: DEBUG: DisplayGCImage_String                        " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_DisplayGCImage_String                       ) << std::endl; // 0
            std::cout << "VRStorm: DEBUG: LensCenterLeftU_Float                        " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_LensCenterLeftU_Float                       ) << std::endl; // 0.545005
            std::cout << "VRStorm: DEBUG: LensCenterLeftV_Float                        " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_LensCenterLeftV_Float                       ) << std::endl; // 0.497215
            std::cout << "VRStorm: DEBUG: LensCenterRightU_Float                       " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_LensCenterRightU_Float                      ) << std::endl; // 0.454052
            std::cout << "VRStorm: DEBUG: LensCenterRightV_Float                       " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_LensCenterRightV_Float                      ) << std::endl; // 0.497139
            std::cout << "VRStorm: DEBUG: UserHeadToEyeDepthMeters_Float               " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_UserHeadToEyeDepthMeters_Float              ) << std::endl; // 0.015
            std::cout << "VRStorm: DEBUG: CameraFirmwareVersion_Uint64                 " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_CameraFirmwareVersion_Uint64                ) << std::endl; // 8590262285
            std::cout << "VRStorm: DEBUG: CameraFirmwareDescription_String             " << get_tracked_device_string(                   i, vr::Prop_CameraFirmwareDescription_String            ) << std::endl; // Version: 02.05.0D Date: 2016.Feb.26
            std::cout << "VRStorm: DEBUG: DisplayFPGAVersion_Uint64                    " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_DisplayFPGAVersion_Uint64                   ) << std::endl; // 57
            std::cout << "VRStorm: DEBUG: DisplayBootloaderVersion_Uint64              " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_DisplayBootloaderVersion_Uint64             ) << std::endl; // 1048584
            std::cout << "VRStorm: DEBUG: DisplayHardwareVersion_Uint64                " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_DisplayHardwareVersion_Uint64               ) << std::endl; // 19
            std::cout << "VRStorm: DEBUG: AudioFirmwareVersion_Uint64                  " << hmd_handle->GetUint64TrackedDeviceProperty(  i, vr::Prop_AudioFirmwareVersion_Uint64                 ) << std::endl; // 3
            std::cout << "VRStorm: DEBUG: CameraCompatibilityMode_Int32                " << hmd_handle->GetInt32TrackedDeviceProperty(   i, vr::Prop_CameraCompatibilityMode_Int32               ) << std::endl; // 0
            std::cout << "VRStorm: DEBUG: ScreenshotHorizontalFieldOfViewDegrees_Float " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_ScreenshotHorizontalFieldOfViewDegrees_Float) << std::endl; // 85
            std::cout << "VRStorm: DEBUG: ScreenshotVerticalFieldOfViewDegrees_Float   " << hmd_handle->GetFloatTrackedDeviceProperty(   i, vr::Prop_ScreenshotVerticalFieldOfViewDegrees_Float  ) << std::endl; // 85
            std::cout << "VRStorm: DEBUG: DisplaySuppressed_Bool                       " << hmd_handle->GetBoolTrackedDeviceProperty(    i, vr::Prop_DisplaySuppressed_Bool                      ) << std::endl; // false
            break;
          case vr::TrackedDeviceClass_Controller:                               // Tracked controllers
            std::cout << "VRStorm: DEBUG: AttachedDeviceId_String " << get_tracked_device_string(                 i, vr::Prop_AttachedDeviceId_String) << std::endl;
            std::cout << "VRStorm: DEBUG: SupportedButtons_Uint64 " << hmd_handle->GetUint64TrackedDeviceProperty(i, vr::Prop_SupportedButtons_Uint64) << std::endl;
            std::cout << "VRStorm: DEBUG: Axis0Type_Int32         " << hmd_handle->GetInt32TrackedDeviceProperty( i, vr::Prop_Axis0Type_Int32        ) << std::endl;
            std::cout << "VRStorm: DEBUG: Axis1Type_Int32         " << hmd_handle->GetInt32TrackedDeviceProperty( i, vr::Prop_Axis1Type_Int32        ) << std::endl;
            std::cout << "VRStorm: DEBUG: Axis2Type_Int32         " << hmd_handle->GetInt32TrackedDeviceProperty( i, vr::Prop_Axis2Type_Int32        ) << std::endl;
            std::cout << "VRStorm: DEBUG: Axis3Type_Int32         " << hmd_handle->GetInt32TrackedDeviceProperty( i, vr::Prop_Axis3Type_Int32        ) << std::endl;
            std::cout << "VRStorm: DEBUG: Axis4Type_Int32         " << hmd_handle->GetInt32TrackedDeviceProperty( i, vr::Prop_Axis4Type_Int32        ) << std::endl;
            // above return value is of type EVRControllerAxisType
            break;
          case vr::TrackedDeviceClass_TrackingReference:                        // Camera and base stations that serve as tracking reference points
            std::cout << "VRStorm: DEBUG: FieldOfViewLeftDegrees_Float     " << hmd_handle->GetFloatTrackedDeviceProperty(i, vr::Prop_FieldOfViewLeftDegrees_Float    ) << std::endl;
            std::cout << "VRStorm: DEBUG: FieldOfViewRightDegrees_Float    " << hmd_handle->GetFloatTrackedDeviceProperty(i, vr::Prop_FieldOfViewRightDegrees_Float   ) << std::endl;
            std::cout << "VRStorm: DEBUG: FieldOfViewTopDegrees_Float      " << hmd_handle->GetFloatTrackedDeviceProperty(i, vr::Prop_FieldOfViewTopDegrees_Float     ) << std::endl;
            std::cout << "VRStorm: DEBUG: FieldOfViewBottomDegrees_Float   " << hmd_handle->GetFloatTrackedDeviceProperty(i, vr::Prop_FieldOfViewBottomDegrees_Float  ) << std::endl;
            std::cout << "VRStorm: DEBUG: TrackingRangeMinimumMeters_Float " << hmd_handle->GetFloatTrackedDeviceProperty(i, vr::Prop_TrackingRangeMinimumMeters_Float) << std::endl;
            std::cout << "VRStorm: DEBUG: TrackingRangeMaximumMeters_Float " << hmd_handle->GetFloatTrackedDeviceProperty(i, vr::Prop_TrackingRangeMaximumMeters_Float) << std::endl;
            std::cout << "VRStorm: DEBUG: ModeLabel_String                 " << get_tracked_device_string(                i, vr::Prop_ModeLabel_String                ) << std::endl;
            break;
          default:
            break;
          }
        #endif // DEBUG_VRSTORM
      }

      for(auto const &i : controller_ids) {
        // identify the attached controllers
        controllers.emplace_back();
        controllers.back().id = i;
        std::cout << "VRStorm: Controller " << i;
        switch(hmd_handle->GetControllerRoleForTrackedDeviceIndex(i)) {
        case vr::TrackedControllerRole_LeftHand:
          std::cout << " (left): " << std::endl;
          controllers.back().hand = input::controller::hand_type::LEFT;
          break;
        case vr::TrackedControllerRole_RightHand:
          std::cout << " (right): " << std::endl;
          controllers.back().hand = input::controller::hand_type::RIGHT;
          break;
        case vr::TrackedControllerRole_Invalid:
        default:
          std::cout << ": " << std::endl;
          controllers.back().hand = input::controller::hand_type::UNKNOWN;
          break;
        }
        for(unsigned int axis = 0; axis != vr::Prop_Axis4Type_Int32 - vr::Prop_Axis0Type_Int32; ++axis) {
          vr::EVRControllerAxisType const controller_axis_type = static_cast<vr::EVRControllerAxisType>(hmd_handle->GetInt32TrackedDeviceProperty(i, static_cast<vr::ETrackedDeviceProperty>(vr::Prop_Axis0Type_Int32 + axis)));
          switch(controller_axis_type) {
          case vr::k_eControllerAxis_None:
            //std::cout << "VRStorm:   axis " << axis << ": " << hmd_handle->GetControllerAxisTypeNameFromEnum(controller_axis_type) << " is not there" << std::endl;
            break;
          case vr::k_eControllerAxis_TrackPad:
            std::cout << "VRStorm:   axis " << axis << " is a trackpad" << std::endl;
            break;
          case vr::k_eControllerAxis_Joystick:
            std::cout << "VRStorm:   axis " << axis << " is a joystick" << std::endl;
            break;
          case vr::k_eControllerAxis_Trigger:                                   // analogue trigger data is in the X axis
            std::cout << "VRStorm:   axis " << axis << " is a trigger" << std::endl;
            break;
          }
        }
        /*
        // the below is only useful for button events
        for(uint_fast64_t button = 0; button != std::min(static_cast<uint_fast64_t>(vr::k_EButton_Max), hmd_handle->GetUint64TrackedDeviceProperty(i, vr::Prop_SupportedButtons_Uint64)); ++button) {
          std::cout << "VRStorm:   button " << button << ": " << hmd_handle->GetButtonIdNameFromEnum(static_cast<vr::EVRButtonId>(button)) << std::endl;
        }
        */
      }
      if(hmd_handle->IsInputFocusCapturedByAnotherProcess()) {
        std::cout << "VRStorm: Input focus is currently held by another process." << std::endl;
      } else {
        std::cout << "VRStorm: Input focus is available for capture." << std::endl;
      }
      input_controller.init();
      input_controller.update_hands();
      input_controller.update_names();

      // get render models if available, the following replaces vr::IVRRenderModels *render_models = vr::VRRenderModels();
      vr::IVRRenderModels *render_models = static_cast<vr::IVRRenderModels*>(VR_GetGenericInterface(vr::IVRRenderModels_Version, &vr_error));
      if(render_models) {
        for(auto &it : controllers) {                                           // load models for controllers
          std::string const render_model_name(get_tracked_device_string(it.id, vr::Prop_RenderModelName_String));
          vr::RenderModel_t *&model = models[render_model_name];
          if(model) {
            std::cout << "VRStorm: Device " << it.id << " shares already loaded render model: " << render_model_name << std::endl;
          } else {
            std::cout << "VRStorm: Loading device " << it.id << "'s render model: " << render_model_name << "...";
            vr::EVRRenderModelError model_load_error = render_models->LoadRenderModel_Async(render_model_name.c_str(), &model);
            while(model_load_error == vr::VRRenderModelError_Loading) {
              std::this_thread::sleep_for(std::chrono::milliseconds(100));      // just sleep-wait for the models to load
              std::cout << ".";
              model_load_error = render_models->LoadRenderModel_Async(render_model_name.c_str(), &model);
            }
            // TODO: make this async to speed up loading
            if(model_load_error != vr::VRRenderModelError_None || !model) {
              std::cout << std::endl;
              std::cout << "VRStorm: Failed to load device " << it.id << "'s render model: " << render_model_name << std::endl;
              continue;
            }
            std::cout << " done, " << model->unVertexCount << " verts, " << model->unTriangleCount << " tris" << std::endl;
          }
          it.model = model;
        }
        // this works, but no need to implement it yet:
        /*
        for(unsigned int i = 0; i != render_models->GetRenderModelCount(); ++i) {
          size_t buffer_len = render_models->GetRenderModelName(i, nullptr, 0);
          std::string buffer(buffer_len, '\0');
          render_models->GetRenderModelName(i, &buffer[0], cast_if_required<uint32_t>(buffer.size()));
          std::cout << "VRStorm: Render model " << i << ": \"" << buffer << "\"" << std::endl;
        }
        */
      } else {
        std::cout << "VRStorm: Unable to get render model interface: " << VR_GetVRInitErrorAsEnglishDescription(vr_error) << std::endl; // this is not a fatal error - we just carry on
      }

      // grab the initial poses
      enabled = true;
      update();
      {
        vec3f const head_position(hmd_position.get_translation());
        if(head_position.y == 0.0f) {
          std::cout << "VRStorm: HMD initial position is invalid, defaulting to " << head_height << "m starting head height" << std::endl;
        } else {
          head_height = -head_position.y;
          std::cout << "VRStorm: Starting head height is " << head_height << "m" << std::endl;
        }
      }
      std::cout << "VRStorm: Successfully initialised." << std::endl;
    } catch(std::exception &e) {
      std::cout << "VRStorm: Exception in startup: " << e.what() << std::endl;
      shutdown();
    }
  #else
    std::cout << "VRStorm: VR is disabled in this build." << std::endl;
  #endif // VRSTORM_DISABLED
}

void manager::shutdown() {
  /// Shut down the VR system
  #ifndef VRSTORM_DISABLED
    if(lib && enabled) {
      std::cout << "VRStorm: Shutting down." << std::endl;
      try {
        auto VR_ShutdownInternal = load_symbol<decltype(&vr::VR_ShutdownInternal)>(lib, "VR_ShutdownInternal");
        if(VR_ShutdownInternal) {
          VR_ShutdownInternal();
        }
      } catch(std::exception &e) {
        std::cout << "VRStorm: Exception at shutdown: " << e.what() << std::endl;
      }
      hmd_handle = nullptr;
    }
    models.clear();
  #endif // VRSTORM_DISABLED
  unload_dynamic(lib);
  enabled = false;
}

void manager::update() {
  #ifndef VRSTORM_DISABLED
    if(!enabled) {
      return;
    }
    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> tracked_device_poses;
    compositor->WaitGetPoses(tracked_device_poses.data(), vr::k_unMaxTrackedDeviceCount, nullptr, 0);
    if(tracked_device_poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {     // update HMD state
      hmd_position = mat4f::from_row_major_34_array(*tracked_device_poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m).inverse();
    }
    for(auto &it : controllers) {                                               // update controller states
      if(tracked_device_poses[it.id].bPoseIsValid) {
        it.position = mat4f::from_row_major_34_array(*tracked_device_poses[it.id].mDeviceToAbsoluteTracking.m);
      }
    }

    float const new_ipd = hmd_handle->GetFloatTrackedDeviceProperty(0, vr::Prop_UserIpdMeters_Float);
    if(ipd != new_ipd) {                                                        // cache the new eye to head transforms only when IPD changes
      if(ipd == 0.0f) {
        std::cout << "VRStorm: Inter-pupillary distance set to " << new_ipd * 1000.0f << "mm" << std::endl;
      } else {
        std::cout << "VRStorm: Inter-pupillary distance changed from " << ipd * 1000.0f << "mm to " << new_ipd * 1000.0f << "mm" << std::endl;
      }
      ipd = new_ipd;
      eye_to_head_transform[static_cast<unsigned int>(vr::EVREye::Eye_Left )] = mat4f::from_row_major_34_array(*hmd_handle->GetEyeToHeadTransform(vr::EVREye::Eye_Left ).m).inverse();
      eye_to_head_transform[static_cast<unsigned int>(vr::EVREye::Eye_Right)] = mat4f::from_row_major_34_array(*hmd_handle->GetEyeToHeadTransform(vr::EVREye::Eye_Right).m).inverse();
    }

    vr::VREvent_t event;
    while(hmd_handle->PollNextEvent(&event, sizeof(event))) {                   // poll for any new events in the queue
      #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
        std::cout << "VRStorm: DEBUG: polled an event: " << hmd_handle->GetEventTypeNameFromEnum(static_cast<vr::EVREventType>(event.eventType)) << " on device " << static_cast<int64_t>(event.trackedDeviceIndex) << std::endl;
      #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
      switch(event.eventType) {
      case vr::VREvent_None:
        break;
      case vr::VREvent_TrackedDeviceActivated:                                  // when a new device is activated / connected
        #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
          std::cout << "VRStorm: DEBUG: controller id " << event.trackedDeviceIndex << " has been activated." << std::endl;
        #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
        // TODO: update the tracked device listings
        input_controller.update_hands();
        input_controller.update_names();
        break;
      case vr::VREvent_TrackedDeviceDeactivated:                                // when a device is deactivated / disconnected
        #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
          std::cout << "VRStorm: DEBUG: controller id " << event.trackedDeviceIndex << " has been deactivated." << std::endl;
        #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
        // TODO: update the tracked device listings
        input_controller.update_hands();
        input_controller.update_names();
        break;
      case vr::VREvent_TrackedDeviceUpdated:
        break;
      case vr::VREvent_TrackedDeviceUserInteractionStarted:                     // when the headset wakes up
        break;
      case vr::VREvent_TrackedDeviceUserInteractionEnded:                       // when the headset goes to sleep
        break;
      case vr::VREvent_IpdChanged:
        break;
      case vr::VREvent_EnterStandbyMode:                                        // when the headset goes to sleep
        break;
      case vr::VREvent_LeaveStandbyMode:                                        // when the headset wakes up
        break;
      case vr::VREvent_TrackedDeviceRoleChanged:
        break;
      // input:
      case vr::VREvent_ButtonPress:                                             // data is controller
        {
          unsigned int const button = event.data.controller.button;
          switch(hmd_handle->GetControllerRoleForTrackedDeviceIndex(event.trackedDeviceIndex)) {
          case vr::TrackedControllerRole_LeftHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " press on left controller begin" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            if(input_controller.get_id(input::controller::hand_type::LEFT) != event.trackedDeviceIndex) {
              // the handedness of this report does not agree with our cached controller roles - need to recache them
              input_controller.update_hands();
            }
            input_controller.execute_button(input::controller::hand_type::LEFT, button, input::controller::actiontype::PRESS);
            break;
          case vr::TrackedControllerRole_RightHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " press on right controller begin" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            if(input_controller.get_id(input::controller::hand_type::RIGHT) != event.trackedDeviceIndex) {
              // the handedness of this report does not agree with our cached controller roles - need to recache them
              input_controller.update_hands();
            }
            input_controller.execute_button(input::controller::hand_type::RIGHT, button, input::controller::actiontype::PRESS);
            break;
          default:
            std::cout << "VRStorm: WARNING: button " << button << " press on unknown controller begin!" << std::endl;
            break;
          }
        }
        break;
      case vr::VREvent_ButtonUnpress:                                           // data is controller
        {
          unsigned int const button = event.data.controller.button;
          switch(hmd_handle->GetControllerRoleForTrackedDeviceIndex(event.trackedDeviceIndex)) {
          case vr::TrackedControllerRole_LeftHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " press on left controller end" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            input_controller.execute_button(input::controller::hand_type::LEFT, button, input::controller::actiontype::RELEASE);
            break;
          case vr::TrackedControllerRole_RightHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " press on right controller end" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            input_controller.execute_button(input::controller::hand_type::RIGHT, button, input::controller::actiontype::RELEASE);
            break;
          default:
            std::cout << "VRStorm: WARNING: button " << button << " press on unknown controller end!" << std::endl;
            break;
          }
        }
        break;
      case vr::VREvent_ButtonTouch:                                             // data is controller
        {
          unsigned int const button = event.data.controller.button;
          switch(hmd_handle->GetControllerRoleForTrackedDeviceIndex(event.trackedDeviceIndex)) {
          case vr::TrackedControllerRole_LeftHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " touch on left controller begin" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            input_controller.execute_button(input::controller::hand_type::LEFT, button, input::controller::actiontype::TOUCH);
            break;
          case vr::TrackedControllerRole_RightHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " touch on right controller begin" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            input_controller.execute_button(input::controller::hand_type::RIGHT, button, input::controller::actiontype::TOUCH);
            break;
          default:
            std::cout << "VRStorm: WARNING: button " << button << " touch on unknown controller begin!" << std::endl;
            break;
          }
        }
        break;
      case vr::VREvent_ButtonUntouch:                                           // data is controller
        {
          unsigned int const button = event.data.controller.button;
          switch(hmd_handle->GetControllerRoleForTrackedDeviceIndex(event.trackedDeviceIndex)) {
          case vr::TrackedControllerRole_LeftHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " touch on left controller end" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            input_controller.execute_button(input::controller::hand_type::LEFT, button, input::controller::actiontype::UNTOUCH);
            break;
          case vr::TrackedControllerRole_RightHand:
            #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
              std::cout << "VRStorm: DEBUG: button " << button << " touch on right controller end" << std::endl;
            #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            input_controller.execute_button(input::controller::hand_type::RIGHT, button, input::controller::actiontype::UNTOUCH);
            break;
          default:
            std::cout << "VRStorm: WARNING: button " << button << " touch on unknown controller end!" << std::endl;
            break;
          }
        }
        break;
      case vr::VREvent_MouseMove:                                               // data is mouse
        break;
      case vr::VREvent_MouseButtonDown:                                         // data is mouse
        break;
      case vr::VREvent_MouseButtonUp:                                           // data is mouse
        break;
      case vr::VREvent_FocusEnter:                                              // data is overlay
        break;
      case vr::VREvent_FocusLeave:                                              // data is overlay
        break;
      case vr::VREvent_Scroll:                                                  // data is mouse
        break;
      case vr::VREvent_TouchPadMove:                                            // data is mouse
        break;
      case vr::VREvent_InputFocusCaptured:                                      // data is process DEPRECATED
        break;
      case vr::VREvent_InputFocusReleased:                                      // data is process DEPRECATED
        break;
      case vr::VREvent_SceneFocusLost:                                          // data is process
        break;
      case vr::VREvent_SceneFocusGained:                                        // data is process
        break;
      case vr::VREvent_SceneApplicationChanged:                                 // data is process - The App actually drawing the scene changed (usually to or from the compositor)
        break;
      case vr::VREvent_SceneFocusChanged:                                       // data is process - New app got access to draw the scene
        break;
      case vr::VREvent_InputFocusChanged:                                       // data is process
        break;
      case vr::VREvent_SceneApplicationSecondaryRenderingStarted:               // data is process
        break;
      case vr::VREvent_HideRenderModels:                                        // Sent to the scene application to request hiding render models temporarily
        break;
      case vr::VREvent_ShowRenderModels:                                        // Sent to the scene application to request restoring render model visibility
        break;
      case vr::VREvent_OverlayShown:
        break;
      case vr::VREvent_OverlayHidden:
        break;
      case vr::VREvent_DashboardActivated:                                      // user opens the dashboard, i.e. with hmd button
        break;
      case vr::VREvent_DashboardDeactivated:                                    // user closes the dashboard, i.e. with hmd button
        break;
      case vr::VREvent_DashboardThumbSelected:                                  // Sent to the overlay manager - data is overlay
        break;
      case vr::VREvent_DashboardRequested:                                      // Sent to the overlay manager - data is overlay
        break;
      case vr::VREvent_ResetDashboard:                                          // Send to the overlay manager
        break;
      case vr::VREvent_RenderToast:                                             // Send to the dashboard to render a toast - data is the notification ID
        break;
      case vr::VREvent_ImageLoaded:                                             // Sent to overlays when a SetOverlayRaw or SetOverlayFromFile call finishes loading
        break;
      case vr::VREvent_ShowKeyboard:                                            // Sent to keyboard renderer in the dashboard to invoke it
        break;
      case vr::VREvent_HideKeyboard:                                            // Sent to keyboard renderer in the dashboard to hide it
        break;
      case vr::VREvent_OverlayGamepadFocusGained:                               // Sent to an overlay when IVROverlay::SetFocusOverlay is called on it
        break;
      case vr::VREvent_OverlayGamepadFocusLost:                                 // Send to an overlay when it previously had focus and IVROverlay::SetFocusOverlay is called on something else
        break;
      case vr::VREvent_OverlaySharedTextureChanged:
        break;
      case vr::VREvent_DashboardGuideButtonDown:
        break;
      case vr::VREvent_DashboardGuideButtonUp:
        break;
      case vr::VREvent_ScreenshotTriggered:                                     // Screenshot button combo was pressed, Dashboard should request a screenshot
        break;
      case vr::VREvent_ImageFailed:                                             // Sent to overlays when a SetOverlayRaw or SetOverlayfromFail fails to load
        break;
      // screenshot API:
      case vr::VREvent_RequestScreenshot:                                       // Sent by vrclient application to compositor to take a screenshot
        break;
      case vr::VREvent_ScreenshotTaken:                                         // Sent by compositor to the application that the screenshot has been taken
        break;
      case vr::VREvent_ScreenshotFailed:                                        // Sent by compositor to the application that the screenshot failed to be taken
        break;
      case vr::VREvent_SubmitScreenshotToDashboard:                             // Sent by compositor to the dashboard that a completed screenshot was submitted
        break;
      case vr::VREvent_Notification_Shown:
        break;
      case vr::VREvent_Notification_Hidden:
        break;
      case vr::VREvent_Notification_BeginInteraction:
        break;
      case vr::VREvent_Notification_Destroyed:
        break;
      case vr::VREvent_Quit:                                                    // data is process
        break;
      case vr::VREvent_ProcessQuit:                                             // data is process
        break;
      case vr::VREvent_QuitAborted_UserPrompt:                                  // data is process
        break;
      case vr::VREvent_QuitAcknowledged:                                        // data is process
        break;
      case vr::VREvent_DriverRequestedQuit:                                     // The driver has requested that SteamVR shut down
        break;
      case vr::VREvent_ChaperoneDataHasChanged:
        break;
      case vr::VREvent_ChaperoneUniverseHasChanged:
        break;
      case vr::VREvent_ChaperoneTempDataHasChanged:
        break;
      case vr::VREvent_ChaperoneSettingsHaveChanged:
        break;
      case vr::VREvent_SeatedZeroPoseReset:
        break;
      case vr::VREvent_AudioSettingsHaveChanged:
        break;
      case vr::VREvent_BackgroundSettingHasChanged:
        break;
      case vr::VREvent_CameraSettingsHaveChanged:
        break;
      case vr::VREvent_ReprojectionSettingHasChanged:
        break;
      case vr::VREvent_ModelSkinSettingsHaveChanged:
        break;
      case vr::VREvent_EnvironmentSettingsHaveChanged:
        break;
      case vr::VREvent_StatusUpdate:
        break;
      case vr::VREvent_MCImageUpdated:
        break;
      case vr::VREvent_FirmwareUpdateStarted:
        break;
      case vr::VREvent_FirmwareUpdateFinished:
        break;
      case vr::VREvent_KeyboardClosed:
        break;
      case vr::VREvent_KeyboardCharInput:
        break;
      case vr::VREvent_KeyboardDone:                                            // Sent when DONE button clicked on keyboard
        break;
      case vr::VREvent_ApplicationTransitionStarted:
        break;
      case vr::VREvent_ApplicationTransitionAborted:
        break;
      case vr::VREvent_ApplicationTransitionNewAppStarted:
        break;
      case vr::VREvent_ApplicationListUpdated:
        break;
      case vr::VREvent_Compositor_MirrorWindowShown:
        break;
      case vr::VREvent_Compositor_MirrorWindowHidden:
        break;
      case vr::VREvent_Compositor_ChaperoneBoundsShown:
        break;
      case vr::VREvent_Compositor_ChaperoneBoundsHidden:
        break;
      case vr::VREvent_TrackedCamera_StartVideoStream:
        break;
      case vr::VREvent_TrackedCamera_StopVideoStream:
        break;
      case vr::VREvent_TrackedCamera_PauseVideoStream:
        break;
      case vr::VREvent_TrackedCamera_ResumeVideoStream:
        break;
      case vr::VREvent_PerformanceTest_EnableCapture:
        break;
      case vr::VREvent_PerformanceTest_DisableCapture:
        break;
      case vr::VREvent_PerformanceTest_FidelityLevel:
        break;
      case vr::VREvent_VendorSpecific_Reserved_Start:                           // Vendors are free to expose private events in this reserved region
      case vr::VREvent_VendorSpecific_Reserved_End:
      default:
        if(event.eventType >= vr::VREvent_VendorSpecific_Reserved_Start &&
           event.eventType <= vr::VREvent_VendorSpecific_Reserved_End) {
          #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            std::cout << "VRStorm: Received a vendor specific event on device " << event.trackedDeviceIndex << ", type " << event.eventType << std::endl;
          #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
        } else {
          //#if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
            std::cout << "VRStorm: Received an unknown event on device " << event.trackedDeviceIndex << ", type " << event.eventType << std::endl;
          //#endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
        }
        break;
      }
    }

    // poll and update the analogue controller axes
    input_controller.poll();
  #endif // VRSTORM_DISABLED
}

vec2<GLsizei> const &manager::get_render_target_size() const {
  /// Return the size of the render target for this VR system
  return render_target_size;
}

#ifndef VRSTORM_DISABLED
  std::string manager::get_tracked_device_string(vr::TrackedDeviceIndex_t device_index,
                                                 vr::TrackedDeviceProperty prop,
                                                 vr::TrackedPropertyError *vr_error) const {
    /// Helper to get a string from a tracked device property and turn it into a std::string
    if(!hmd_handle) {
      return "";
    }
    uint32_t buffer_len = hmd_handle->GetStringTrackedDeviceProperty(device_index, prop, NULL, 0, vr_error);
    if(buffer_len == 0) {
      return "";
    }
    std::string buffer(buffer_len, '\0');
    buffer_len = hmd_handle->GetStringTrackedDeviceProperty(device_index, prop, &buffer[0], buffer_len, vr_error);
    return buffer;
  }

  void manager::setup_render_perspective_one_eye(vr::EVREye eye) {
    /// Set up the render perspective for the specified openvr eye
    // Order: Model * View * Eye^-1 * Projection.
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(mat4f::from_row_major_array(*hmd_handle->GetProjectionMatrix(eye, nearplane, farplane, vr::API_OpenGL).m)); // projection matrix

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMultMatrixf(eye_to_head_transform[static_cast<unsigned int>(eye)]);       // cached eye position matrix
    glMultMatrixf(hmd_position);                                                // head position matrix
  };
#endif // VRSTORM_DISABLED

void manager::setup_render_perspective_left() {
  /// Set up the render perspective for the left eye
  #ifndef VRSTORM_DISABLED
    setup_render_perspective_one_eye(vr::Eye_Left);
  #endif // VRSTORM_DISABLED
};
void manager::setup_render_perspective_right() {
  /// Set up the render perspective for the right eye
  #ifndef VRSTORM_DISABLED
    setup_render_perspective_one_eye(vr::Eye_Right);
  #endif // VRSTORM_DISABLED
};

void manager::submit_frame_left(GLuint buffer
                                  #ifdef VRSTORM_DISABLED
                                    __attribute__((__unused__))
                                  #endif // VRSTORM_DISABLED
                                ) {
  /// Send a frame to the compositor for the left eye
  #ifndef VRSTORM_DISABLED
    vr::Texture_t texture_container{reinterpret_cast<void*>(buffer), vr::API_OpenGL, vr::ColorSpace_Gamma};
    compositor->Submit(vr::Eye_Left, &texture_container, nullptr, vr::Submit_Default);
  #endif // VRSTORM_DISABLED
}
void manager::submit_frame_right(GLuint buffer
                                   #ifdef VRSTORM_DISABLED
                                     __attribute__((__unused__))
                                   #endif // VRSTORM_DISABLED
                                 ) {
  /// Send a frame to the compositor for the right eye
  #ifndef VRSTORM_DISABLED
    vr::Texture_t texture_container{reinterpret_cast<void*>(buffer), vr::API_OpenGL, vr::ColorSpace_Gamma};
    compositor->Submit(vr::Eye_Right, &texture_container, nullptr, vr::Submit_Default);
  #endif // VRSTORM_DISABLED
}

}
