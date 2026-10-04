package vendor.qti.hardware.radio.am;

@VintfStability
interface IQcRilAudioRequest {
    oneway void queryParameters(in int token, in String params);
    oneway void setParameters(in int token, in String params);
}
