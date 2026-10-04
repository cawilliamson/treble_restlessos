package vendor.qti.hardware.radio.am;

import vendor.qti.hardware.radio.am.AudioError;

@VintfStability
interface IQcRilAudioResponse {
    oneway void queryParametersResponse(in int token, in String params);
    oneway void setParametersResponse(in int token, in AudioError errorCode);
}
