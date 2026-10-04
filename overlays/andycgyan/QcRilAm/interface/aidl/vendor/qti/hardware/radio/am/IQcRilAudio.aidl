package vendor.qti.hardware.radio.am;

import vendor.qti.hardware.radio.am.AudioError;
import vendor.qti.hardware.radio.am.IQcRilAudioRequest;
import vendor.qti.hardware.radio.am.IQcRilAudioResponse;

@VintfStability
interface IQcRilAudio {
    IQcRilAudioResponse setRequestInterface(in IQcRilAudioRequest callback);
    oneway void setError(in AudioError errorCode);
}
