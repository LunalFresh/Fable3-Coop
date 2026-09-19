#pragma once
#include "xdefs.hpp"
#include <map>
#include <set>
#include <vector>
#include <sys/timeb.h>
// OPUS_STATIC_RUNTIME must be defined when generating/compiling/linking for DEBUG builds only.
#include <opus.h>
#include "../utils/util-cyclic-buffer.hpp"

// One channel per local talker.
#define XHV_PCM_CHANNEL_COUNT (1)

// Opus available frame sizes:
//	msec			2.5,   5,  10,  20,   40,   60
//	denominator		400, 200, 100,  50,   25, 16.6
//	frame			 40,  80, 160, 320,  640,  960
//	in bytes		 80, 160, 320, 640, 1280, 1920
//										^^^^
const uint32_t XHV_OPUS_PCM_FRAME_MSEC_LENGTH = 40;
const uint32_t XHV_OPUS_PCM_SAMPLE_COUNT_PER_FRAME = ((XHV_PCM_SAMPLE_RATE * XHV_OPUS_PCM_FRAME_MSEC_LENGTH) / 1000);
const uint32_t XHV_PCM_BLOCK_SIZE = (XHV_PCM_BYTES_PER_SAMPLE * XHV_PCM_CHANNEL_COUNT);
const uint32_t XHV_OPUS_PCM_FRAME_SIZE = (XHV_OPUS_PCM_SAMPLE_COUNT_PER_FRAME * XHV_PCM_BLOCK_SIZE);
const uint16_t XHV_CODEC_HEADER_SEQUENCE_MAX = (0b1 << 11);
// As wSeqNo is used as a whole frame counter, make this value reasonable and with respect to bad network ping times in times of packet loss and resends.
// Note cannot exceed wSeqNo's 11 bits: (0b1 << 11) == 2048.
// 800ms of allowance.
const uint16_t XHV_CODEC_SEQUENCE_MAX_PENDING = (800 / XHV_OPUS_PCM_FRAME_MSEC_LENGTH);
// Length of the wave in/out buffers in milliseconds for sending/receiving from OS.
const uint16_t XHV_PCM_WAVE_BUFFER_MSEC_LENGTH = 200;
// Iteration frequency of the thread that handles all voice data to be played back.
const uint16_t XHV_THREAD_WAVEOUT_ITR_FREQUENCY_MSEC = (XHV_PCM_WAVE_BUFFER_MSEC_LENGTH / 4);
// Milliseconds until the input audio will stop being used after the last time there was audio loud enough to break the minimum threshold.
const uint16_t XHV_VOICE_THRESHOLD_TRIGGER_DELAY_MSEC = 600;
// Milliseconds until incoming voice data will actually be submitted for consumption by audio devices.
const uint16_t XHV_REMOTE_START_CONSUME_DELAY_MSEC = 300;
// Milliseconds since the last whole buffer was filled and submitted that any remaining voice data that does not fill a whole buffer will be consumed in.
const uint16_t XHV_REMOTE_CONSUME_TIMEOUT_MSEC = 400;

// Minimum should be 2 (one being emptied while the other is in use by the OS being filled for microphone, opposite for speaker).
const uint32_t XHV_PCM_WAVE_BUFFER_COUNT = 2;

const uint32_t XHV_AUDIO_DEVICE_NONE = -2;

const uint16_t XHV_AUDIO_SETTING_VOLUME_MAX = 100;
const uint16_t XHV_AUDIO_SETTING_VOLUME_MAX_BOOST = 1000;
const uint8_t XHV_AUDIO_SETTING_INPUT_THRESHOLD_DEFAULT = 10;

#pragma pack(push, 1)
typedef struct _XHV_CODEC_FRAME_HEADER {
	uint16_t frame_size : 10;
	uint16_t reserved : 6;
} XHV_CODEC_FRAME_HEADER;
#pragma pack(pop)

#pragma pack(push, 4)

class XHVEngine;

extern CRITICAL_SECTION xlive_critsec_xhv_engines;
extern std::set<XHVEngine*> xlive_xhv_engines;

bool InitXhvEngine();
bool UninitXhvEngine();

typedef struct _PENDING_SEQUENCE_INFO {
	// new'd at size XHV_OPUS_PCM_FRAME_SIZE.
	uint8_t* data = 0;
	size_t data_size = 0;
	// When (data_size && data_received_size >= data_size) then the sequence is complete and ready for consumption.
	// >= not == because of likely excess data in last packet.
	size_t data_received_size = 0;
} PENDING_SEQUENCE_INFO;

typedef struct _VOICE_REGISTERED_USER_REMOTE {
	XHVEngine* xhv_engine = 0;
	// If null, then this is for local user XHV_LOOPBACK_MODE.
	XUID user_xuid = 0;
	// In use when user_xuid is null purely for XHV_LOOPBACK_MODE.
	uint32_t local_user_index = 0;
	std::set<XHV_PROCESSING_MODE> processing_modes;
	bool is_talking = false;
	// Key: local_user_index. If no match then XHV_PLAYBACK_PRIORITY_MAX default.
	std::map<uint32_t, XHV_PLAYBACK_PRIORITY> playback_priority;
	// aka frame number.
	// % XHV_CODEC_HEADER_SEQUENCE_MAX
	uint16_t packet_sequence_number = 0;
	// Sequence index in vector begins with current value of packet_sequence_number.
	// Sequences will be dropped if vector size exceeds XHV_CODEC_SEQUENCE_MAX_PENDING.
	std::vector<PENDING_SEQUENCE_INFO*> xhv_data_in_pending;
	// Consume the data after this initial wait time to prevent stutter at beginning of audio stream.
	__timeb64 xhv_data_consume_after = {0, 0, 0, 0};
	// Consume all available data immediately. Used for when the audio stream ends.
	__timeb64 xhv_data_consume_timeout = {0, 0, 0, 0};
} VOICE_REGISTERED_USER_REMOTE;

typedef struct _AUDIO_DEVICE_INPUT_INFO {
	XHVEngine* xhv_engine = 0;
	HWAVEIN handle_wavein = 0;
	wchar_t* audio_device_name = 0;
	bool audio_device_capturing = false;
	
	CRITICAL_SECTION lock_wave_buffer;
	// Buffers returned from the system with data from the microphone.
	std::vector<WAVEHDR*> wavein_buffers_containing_data;
	// Buffers in the system queue that can have data pushed into them and returned later.
	std::set<WAVEHDR*> wavein_buffers_in_use;
} AUDIO_DEVICE_INPUT_INFO;

typedef struct _AUDIO_DEVICE_OUTPUT_INFO {
	XHVEngine* xhv_engine = 0;
	HWAVEOUT handle_waveout = 0;
	wchar_t* audio_device_name = 0;
	
	bool buffer_returned = false;
	CRITICAL_SECTION lock_wave_buffer;
	// Buffers not in use by the system for playing back sound.
	std::vector<WAVEHDR*> waveout_buffers_available;
	// Buffers currently in use by the system being used to play back sound.
	std::set<WAVEHDR*> waveout_buffers_in_use;
	// 1/2 a buffer when (!waveout_buffers_in_use.size()), 1 full buffer thereafter.
	__timeb64 waveout_submit_after = {0, 0, 0, 0};
	// The FIFO buffers in here will get mixed on next audio submission to the OS.
	std::map<VOICE_REGISTERED_USER_REMOTE*, std::vector<uint8_t*>> audio_mix_queue;
} AUDIO_DEVICE_OUTPUT_INFO;

typedef struct _VOICE_REGISTERED_USER_LOCAL {
	XHVEngine* xhv_engine = 0;
	uint32_t user_index = 0;
	std::set<XHV_PROCESSING_MODE> processing_modes;
	bool is_talking = false;
	// Only used when an XHVEngine custom VAD callback is not in use.
	__timeb64 was_last_talking_at = {0, 0, 0, 0};
	AUDIO_DEVICE_INPUT_INFO* audio_device_input_info = 0;
	AUDIO_DEVICE_OUTPUT_INFO* audio_device_output_info = 0;
	// 
	XHV_CODEC_HEADER xhv_codec_state;
	// FIFO raw buffer containing all encoded voice frames.
	CyclicBuffer* xhv_data_out = 0;
	// FIFO list of the size of each encoded voice frame.
	std::vector<size_t> xhv_data_out_frame_sizes;
} VOICE_REGISTERED_USER_LOCAL;

class XHVEngine : public IXHVEngine
{
	public:
		// Internal constructor and deconstructor.
		;
		XHVEngine(XHV_INIT_PARAMS* init_params);
		~XHVEngine();
		
		// Public methods from the interface.
		;
		virtual LONG __stdcall AddRef();
		virtual LONG __stdcall Release();
		virtual HRESULT __stdcall Lock(XHV_LOCK_TYPE lock_type);
		virtual HRESULT __stdcall StartLocalProcessingModes(uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count);
		virtual HRESULT __stdcall StopLocalProcessingModes(uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count);
		virtual HRESULT __stdcall StartRemoteProcessingModes(XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count);
		virtual HRESULT __stdcall StopRemoteProcessingModes(XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count);
		virtual HRESULT __stdcall SetMaxDecodePackets(size_t max_decode_packets);
		virtual HRESULT __stdcall RegisterLocalTalker(uint32_t user_index);
		virtual HRESULT __stdcall UnregisterLocalTalker(uint32_t user_index);
		virtual HRESULT __stdcall RegisterRemoteTalker(XUID xuid_remote_talker, XAUDIOVOICEFXCHAIN* remote_talker_fx, XAUDIOVOICEFXCHAIN* talker_pair_fx, XAUDIOSUBMIXVOICE* output_voice);
		virtual HRESULT __stdcall UnregisterRemoteTalker(XUID xuid_remote_talker);
		virtual HRESULT __stdcall GetRemoteTalkers(uint32_t* remote_talker_count, XUID* remote_talkers);
		virtual BOOL __stdcall IsHeadsetPresent(uint32_t user_index);
		virtual BOOL __stdcall IsLocalTalking(uint32_t user_index);
		virtual BOOL __stdcall isRemoteTalking(XUID xuid_remote_talker);
		virtual uint32_t __stdcall GetDataReadyFlags();
		virtual HRESULT __stdcall GetLocalChatData(uint32_t user_index, uint8_t* chat_data, size_t* chat_data_size, size_t* result_packet_count);
		virtual HRESULT __stdcall SetPlaybackPriority(XUID xuid_remote_talker, uint32_t user_index, XHV_PLAYBACK_PRIORITY playback_priority);
		virtual HRESULT __stdcall SubmitIncomingChatData(XUID xuid_remote_talker, const uint8_t* chat_data, size_t* chat_data_size);
		
		// Internal helper methods and variables.
		;
		HRESULT __stdcall SetAudioDeviceInput(uint32_t user_index, uint32_t audio_device_index);
		HRESULT __stdcall SetAudioDeviceOutput(uint32_t user_index, uint32_t audio_device_index);
		
		WAVEFORMATEX wave_format;
		uint32_t wave_prepared_buffer_size;
		
		bool worker_thread_wavein_exit;
		HANDLE worker_thread_wavein;
		HANDLE worker_thread_wavein_signal;
		bool worker_thread_waveout_exit;
		HANDLE worker_thread_waveout;
		HANDLE worker_thread_waveout_signal;
		
		CRITICAL_SECTION lock_audio_devices;
		// Key: audio_device_name.
		// Note: Only find by same string reference, contents won't work.
		std::map<wchar_t*, AUDIO_DEVICE_INPUT_INFO*> audio_devices_input;
		// Key: audio_device_name.
		// Note: Only find by same string reference, contents won't work.
		std::map<wchar_t*, AUDIO_DEVICE_OUTPUT_INFO*> audio_devices_output;
		
		// Key: user_index.
		std::map<uint32_t, VOICE_REGISTERED_USER_LOCAL*> registered_users_local;
		// Key: xuid_remote_user.
		std::map<XUID, VOICE_REGISTERED_USER_REMOTE*> registered_users_remote;
		// Key: user_index.
		std::map<uint32_t, VOICE_REGISTERED_USER_REMOTE*> registered_users_loopback;
		// Optional Title provided function that can alter the captured audio and also determines if the audio should be transmitted.
		PFNMICRAWDATAREADY pfnMicrophoneRawDataReady;
		
		CRITICAL_SECTION lock_pending_voice_frame_buffers;
		std::vector<uint8_t*> pending_voice_frame_buffers_available;
		
		OpusEncoder* opus_encoder;
		OpusDecoder* opus_decoder;
		
	private:
		CRITICAL_SECTION xhv_lock;
		LONG xhv_reference_count;
		
		uint32_t registered_users_max_local;
		uint32_t registered_users_max_remote;
		bool relax_privileges;
		HWND hwnd_title_focus;
		std::set<XHV_PROCESSING_MODE> processing_modes_enabled_local;
		std::set<XHV_PROCESSING_MODE> processing_modes_enabled_remote;
		
		HRESULT __stdcall StartStopLocalProcessingModes(bool start, uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count);
		HRESULT __stdcall StartStopRemoteProcessingModes(bool start, XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count);
		HRESULT __stdcall OpenAudioDeviceInput(const wchar_t* audio_device_name, uint32_t audio_device_index, AUDIO_DEVICE_INPUT_INFO** result_audio_device_input_info);
		void __stdcall CloseUnusedAudioDeviceInput(AUDIO_DEVICE_INPUT_INFO* audio_device_input_info);
		HRESULT __stdcall OpenAudioDeviceOutput(const wchar_t* audio_device_name, uint32_t audio_device_index, AUDIO_DEVICE_OUTPUT_INFO** result_audio_device_output_info);
		void __stdcall CloseUnusedAudioDeviceOutput(AUDIO_DEVICE_OUTPUT_INFO* audio_device_output_info);
};

#pragma pack (pop)

HRESULT SetAudioDeviceInput(uint32_t user_index, uint32_t audio_device_index);
HRESULT SetAudioDeviceOutput(uint32_t user_index, uint32_t audio_device_index);

extern bool xlive_xhv_engine_enabled;
extern bool xlive_xhv_engine_version_new;
extern uint32_t xlive_xhv_engine_voice_encoded_bitrate;
