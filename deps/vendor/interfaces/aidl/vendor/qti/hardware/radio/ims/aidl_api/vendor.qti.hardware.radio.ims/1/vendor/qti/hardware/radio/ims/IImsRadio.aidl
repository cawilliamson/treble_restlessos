///////////////////////////////////////////////////////////////////////////////
// THIS FILE IS IMMUTABLE. DO NOT EDIT IN ANY CASE.                          //
///////////////////////////////////////////////////////////////////////////////

// This file is a snapshot of an AIDL file. Do not edit it manually. There are
// two cases:
// 1). this is a frozen version file - do not edit this in any case.
// 2). this is a 'current' file. If you make a backwards compatible change to
//     the interface (from the latest frozen version), the build system will
//     prompt you to update this file with `m <name>-update-api`.
//
// You must not make a backward incompatible change to any AIDL file built
// with the aidl_interface module type with versions property set. The module
// type is used to build AIDL files in a way that they can be used across
// independently updatable components of the system. If a device is shipped
// with such a backward incompatible change, it has a high risk of breaking
// later when a module using the interface is updated, e.g., Mainline modules.

package vendor.qti.hardware.radio.ims;
@VintfStability
interface IImsRadio {
  oneway void setCallback();
  oneway void dial();
  oneway void addParticipant();
  oneway void getImsRegistrationState();
  oneway void answer();
  oneway void hangup();
  oneway void requestRegistrationChange();
  oneway void queryServiceStatus();
  oneway void hold();
  oneway void resume();
  oneway void setConfig();
  oneway void getConfig();
  oneway void conference();
  oneway void getClip();
  oneway void getClir();
  oneway void setClir();
  oneway void getColr();
  oneway void setColr();
  oneway void exitEmergencyCallbackMode();
  oneway void sendDtmf();
  oneway void startDtmf();
  oneway void stopDtmf();
  oneway void setUiTtyMode();
  oneway void modifyCallInitiate();
  oneway void modifyCallConfirm();
  oneway void queryCallForwardStatus();
  oneway void setCallForwardStatus();
  oneway void getCallWaiting();
  oneway void setCallWaiting();
  oneway void setSuppServiceNotification();
  oneway void explicitCallTransfer();
  oneway void suppServiceStatus();
  oneway void getRtpStatistics();
  oneway void getRtpErrorStatistics();
  oneway void deflectCall();
  oneway void sendGeolocationInfo();
  oneway void getImsSubConfig();
  oneway void sendRttMessage();
  oneway void cancelModifyCall();
  oneway void sendSms();
  oneway void acknowledgeSms();
  oneway void acknowledgeSmsReport();
  oneway void getSmsFormat();
  oneway void registerMultiIdentityLines();
  oneway void queryVirtualLineInfo();
  oneway void emergencyDial();
  oneway void sendUssd();
  oneway void cancelPendingUssd();
  oneway void callComposerDial();
  oneway void sendSipDtmf();
  oneway void setMediaConfiguration();
  oneway void queryMultiSimVoicCapability();
  oneway void exitSmsCallBackMode();
  oneway void sendVosSupportStatus();
  oneway void sendVosActionInfo(int token);
}
