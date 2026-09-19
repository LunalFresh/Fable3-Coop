#include <winsock2.h>
#include "xhv-engine.hpp"
#include "xlive.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-main.hpp"
#include "../xlln/wnd-user-card.hpp"
#include "../utils/utils.hpp"
#include <thread>
#include <dsound.h>

bool xlive_xhv_engine_enabled = true;
// There are two different versions.
// See XHV_VOICECHAT_MODE_PACKET_SIZE_NEW and XHV_VOICECHAT_MODE_PACKET_SIZE_OLD.
// TODO figure out when to use which version.
bool xlive_xhv_engine_version_new = false;

CRITICAL_SECTION xlive_critsec_xhv_engines;
std::set<XHVEngine*> xlive_xhv_engines;

// 300 bytes per second to be sent over the network per speaker which is the absolute minimum that currently works.
uint32_t xlive_xhv_engine_voice_encoded_bitrate = (300*8);

bool InitXhvEngine()
{
	TRACE_FX();
	
	return true;
}

bool UninitXhvEngine()
{
	TRACE_FX();
	
	EnterCriticalSection(&xlive_critsec_xhv_engines);
	
	if (xlive_xhv_engines.size()) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s There are still (%u) XHV Engine(s) active.", __func__, xlive_xhv_engines.size());
	}
	
	LeaveCriticalSection(&xlive_critsec_xhv_engines);
	
	return true;
}

// #5008
int32_t WINAPI XHVCreateEngine(XHV_INIT_PARAMS* init_params, HANDLE* worker_thread_handle, IXHVEngine** result_xhv_engine)
{
	TRACE_FX();
	if (!init_params) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!result_xhv_engine) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_xhv_engine is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (worker_thread_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s worker_thread_handle is not officially supported.", __func__);
		return E_INVALIDARG;
	}
	if (!init_params->dwMaxLocalTalkers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->dwMaxLocalTalkers is 0.", __func__);
		return E_INVALIDARG;
	}
	if (init_params->dwMaxLocalTalkers > XHV_MAX_LOCAL_TALKERS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->dwMaxLocalTalkers (%u) is greater than %u.", __func__, init_params->dwMaxLocalTalkers, XHV_MAX_LOCAL_TALKERS);
		return E_INVALIDARG;
	}
	if (init_params->dwMaxRemoteTalkers > XHV_MAX_REMOTE_TALKERS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->dwMaxRemoteTalkers (%u) is greater than %u.", __func__, init_params->dwMaxRemoteTalkers, XHV_MAX_REMOTE_TALKERS);
		return E_INVALIDARG;
	}
	if (init_params->dwNumLocalTalkerEnabledModes > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->dwNumLocalTalkerEnabledModes (%u) is greater than %u.", __func__, init_params->dwNumLocalTalkerEnabledModes, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	if (init_params->dwNumRemoteTalkerEnabledModes > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->dwNumRemoteTalkerEnabledModes (%u) is greater than %u.", __func__, init_params->dwNumRemoteTalkerEnabledModes, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	if (!init_params->dwNumLocalTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->dwNumLocalTalkerEnabledModes is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!init_params->dwMaxRemoteTalkers && init_params->dwNumRemoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!init_params->dwMaxRemoteTalkers && init_params->dwNumRemoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (init_params->dwMaxRemoteTalkers && !init_params->dwNumRemoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (init_params->dwMaxRemoteTalkers && !init_params->dwNumRemoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (init_params->dwNumLocalTalkerEnabledModes > 0 && !init_params->localTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (init_params->dwNumLocalTalkerEnabledModes > 0 && !init_params->localTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (init_params->dwNumRemoteTalkerEnabledModes > 0 && !init_params->remoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (init_params->dwNumRemoteTalkerEnabledModes > 0 && !init_params->remoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	for (uint32_t iProcessingMode = 0; iProcessingMode < init_params->dwNumLocalTalkerEnabledModes; iProcessingMode++) {
		if (init_params->localTalkerEnabledModes[iProcessingMode] != XHV_LOOPBACK_MODE && init_params->localTalkerEnabledModes[iProcessingMode] != XHV_VOICECHAT_MODE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid init_params->localTalkerEnabledModes item specified (0x%08x) Must be XHV_LOOPBACK_MODE and/or XHV_VOICECHAT_MODE.", __func__, init_params->localTalkerEnabledModes[iProcessingMode]);
			return E_INVALIDARG;
		}
	}
	// TODO is XHV_LOOPBACK_MODE allowed?
	for (uint32_t iProcessingMode = 0; iProcessingMode < init_params->dwNumRemoteTalkerEnabledModes; iProcessingMode++) {
		if (init_params->remoteTalkerEnabledModes[iProcessingMode] != XHV_LOOPBACK_MODE && init_params->remoteTalkerEnabledModes[iProcessingMode] != XHV_VOICECHAT_MODE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid init_params->remoteTalkerEnabledModes item specified (0x%08x) Must be XHV_LOOPBACK_MODE and/or XHV_VOICECHAT_MODE.", __func__, init_params->remoteTalkerEnabledModes[iProcessingMode]);
			return E_INVALIDARG;
		}
	}
	if (init_params->bCustomVADProvided && !init_params->pfnMicrophoneRawDataReady) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->bCustomVADProvided yet init_params->pfnMicrophoneRawDataReady is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!init_params->hwndFocus) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_params->hwndFocus is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	if (!xlive_xhv_engine_enabled) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s XHV Engines are disabled.", __func__);
		return DSERR_NODRIVER;
	}
	
	XHVEngine* xhvEngine = new XHVEngine(init_params);
	
	EnterCriticalSection(&xlive_critsec_xhv_engines);
	
	xlive_xhv_engines.insert(xhvEngine);
	
	LeaveCriticalSection(&xlive_critsec_xhv_engines);
	
	*result_xhv_engine = xhvEngine;
	
	return S_OK;
}

// #5314
int32_t WINAPI XUserMuteListQuery(uint32_t user_index, XUID xuid_remote_talker, BOOL* result_is_muted)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_INVALID_PARAMETER;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!result_is_muted) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_is_muted is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	// You cannot mute offline accounts on GFWL (i.e. LAN/offline lobbies).
	//if (!IsOnlineXUID(xuid_remote_talker)) {
	//	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker (0x%016I64x) is not an online account.", __func__, xuid_remote_talker);
	//	return ERROR_INVALID_PARAMETER;
	//}
	
	*result_is_muted = FALSE;
	
	{
		EnterCriticalSection(&xlive_critsec_local_user);
		auto &voiceRemoteVolumes = xlive_local_users[user_index].xhv_voice_remote_volume;
		auto itrVoiceRemoteVolume = voiceRemoteVolumes.find(xuid_remote_talker);
		if (itrVoiceRemoteVolume != voiceRemoteVolumes.end()) {
			if (itrVoiceRemoteVolume->second == 0) {
				*result_is_muted = TRUE;
			}
		}
		LeaveCriticalSection(&xlive_critsec_local_user);
	}
	
	return S_OK;
}

static DWORD WINAPI XHVThreadProcessAudioInput(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	XHVEngine* xhv_engine = (XHVEngine*)lpParam;
	
	while (1) {
		DWORD resultWait = WaitForSingleObject(xhv_engine->worker_thread_wavein_signal, INFINITE);
		if (resultWait != WAIT_OBJECT_0) {
			XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XHVEngine (0x%zx) worker_thread_wavein failed to wait on worker_thread_wavein_signal (0x%zx).", xhv_engine, xhv_engine->worker_thread_wavein_signal);
			break;
		}
		
		if (xhv_engine->worker_thread_wavein_exit) {
			break;
		}
		
		{
			xhv_engine->Lock(XHV_LOCK_TYPE_LOCK);
			EnterCriticalSection(&xhv_engine->lock_audio_devices);
			
			for (const auto &itrAudioDeviceInputInfo : xhv_engine->audio_devices_input) {
				AUDIO_DEVICE_INPUT_INFO* audioDeviceInputInfo = itrAudioDeviceInputInfo.second;
				
				std::vector<WAVEHDR*> audioDeviceBuffersToSubmit;
				
				{
					EnterCriticalSection(&audioDeviceInputInfo->lock_wave_buffer);
					
					if (audioDeviceInputInfo->handle_wavein) {
						for (const auto &waveinWavehdr : audioDeviceInputInfo->wavein_buffers_containing_data) {
							if (waveinWavehdr->dwBytesRecorded % (xhv_engine->wave_format.wBitsPerSample / 8)) {
								__debugbreak();
							}
							
							if (waveinWavehdr->dwBytesRecorded != 0) {
								for (const auto &itrRegisteredUserLocal : xhv_engine->registered_users_local) {
									VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal.second;
									
									// If this device/buffer is to be consumed by this local user.
									if (registeredUserLocal->audio_device_input_info == audioDeviceInputInfo && registeredUserLocal->processing_modes.size()) {
										uint8_t* pcmData = new uint8_t[waveinWavehdr->dwBytesRecorded];
										size_t pcmDataSize = waveinWavehdr->dwBytesRecorded;
										memcpy_s(
											pcmData
											, pcmDataSize
											, waveinWavehdr->lpData
											, pcmDataSize
										);
										
										uint32_t samplesRecorded = pcmDataSize / (registeredUserLocal->xhv_engine->wave_format.wBitsPerSample / 8);
										int32_t inputVolume = xlive_local_users[registeredUserLocal->user_index].audio_input_device_volume;
										
										// Determine if the audio from the microphone should be used for transmitting to other players.
										if (registeredUserLocal->xhv_engine->pfnMicrophoneRawDataReady) {
											BOOL voiceDetected = FALSE;
											registeredUserLocal->xhv_engine->pfnMicrophoneRawDataReady(registeredUserLocal->user_index, pcmData, pcmDataSize, &voiceDetected);
											
											registeredUserLocal->is_talking = !!(voiceDetected);
										}
										else {
											__timeb64 currentTime;
											_ftime64_s(&currentTime);
											if (registeredUserLocal->is_talking) {
												uint64_t differenceMilliseconds = (1000 * (currentTime.time - registeredUserLocal->was_last_talking_at.time)) + (currentTime.millitm - registeredUserLocal->was_last_talking_at.millitm);
												if (differenceMilliseconds > XHV_VOICE_THRESHOLD_TRIGGER_DELAY_MSEC) {
													registeredUserLocal->is_talking = false;
												}
											}
											
											int16_t minThreshold = (int16_t)(((uint32_t)INT16_MAX * (uint32_t)xlive_local_users[registeredUserLocal->user_index].audio_input_device_threshold) / (uint32_t)XHV_AUDIO_SETTING_VOLUME_MAX);
											
											for (uint32_t iSample = 0; iSample < samplesRecorded; iSample++) {
												int16_t sampleValue = ((int16_t*)pcmData)[iSample];
												if (sampleValue > minThreshold || sampleValue < -minThreshold) {
													registeredUserLocal->was_last_talking_at = currentTime;
													registeredUserLocal->is_talking = true;
													break;
												}
											}
										}
										
										if (!inputVolume) {
											registeredUserLocal->is_talking = false;
										}
										
										if (registeredUserLocal->is_talking) {
											// Use/transmit the audio.
											
											if (inputVolume != XHV_AUDIO_SETTING_VOLUME_MAX) {
												for (uint32_t iSample = 0; iSample < samplesRecorded; iSample++) {
													int32_t sampleValue = ((int16_t*)pcmData)[iSample];
													sampleValue = ((int32_t)sampleValue * inputVolume) / (int32_t)XHV_AUDIO_SETTING_VOLUME_MAX;
													if (sampleValue > INT16_MAX) {
														sampleValue = INT16_MAX;
													}
													else if (sampleValue < INT16_MIN) {
														sampleValue = INT16_MIN;
													}
													((int16_t*)pcmData)[iSample] = (int16_t)sampleValue;
												}
											}
											
											const size_t opusDecodedRawSize = XHV_OPUS_PCM_FRAME_SIZE;
											uint8_t opusDecodedRaw[opusDecodedRawSize];
											
											for (size_t iPcm = 0; iPcm < pcmDataSize; iPcm += XHV_OPUS_PCM_FRAME_SIZE) {
												if (iPcm + XHV_OPUS_PCM_FRAME_SIZE > pcmDataSize) {
													XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
														, "XHV Opus encode dropping (%u) bytes due to not aligning with frame size."
														, pcmDataSize - iPcm
													);
													break;
												}
												
												uint8_t* pcmDataFrame = &pcmData[iPcm];
												
												int bytesEncoded = opus_encode(
													xhv_engine->opus_encoder
													, (int16_t*)pcmDataFrame
													, XHV_OPUS_PCM_SAMPLE_COUNT_PER_FRAME
													, opusDecodedRaw
													, opusDecodedRawSize
												);
												if (bytesEncoded < 0) {
													XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
														, "XHV Opus opus_encode(...) failed with error: %s."
														, opus_strerror(bytesEncoded)
													);
													continue;
												}
												
												if (registeredUserLocal->processing_modes.count(XHV_VOICECHAT_MODE)) {
													if (!registeredUserLocal->xhv_data_out->PushData((uint8_t*)opusDecodedRaw, bytesEncoded)) {
														XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
															, "XHV xhv_data_out buffer full dropping encoded frame of size (%u)."
															, bytesEncoded
														);
													}
													else {
														registeredUserLocal->xhv_data_out_frame_sizes.push_back(bytesEncoded);
													}
												}
												
												if (registeredUserLocal->processing_modes.count(XHV_LOOPBACK_MODE)) {
													const auto &itrRegisteredUserLoopback = xhv_engine->registered_users_loopback.find(registeredUserLocal->user_index);
													if (itrRegisteredUserLoopback == xhv_engine->registered_users_loopback.end()) {
														__debugbreak();
													}
													VOICE_REGISTERED_USER_REMOTE* registeredUserLoopback = itrRegisteredUserLoopback->second;
													
													PENDING_SEQUENCE_INFO* pendingSequenceInfo = new PENDING_SEQUENCE_INFO;
													registeredUserLoopback->xhv_data_in_pending.push_back(pendingSequenceInfo);
													
													{
														EnterCriticalSection(&xhv_engine->lock_pending_voice_frame_buffers);
														if (xhv_engine->pending_voice_frame_buffers_available.size()) {
															auto itrAvailableBuffer = xhv_engine->pending_voice_frame_buffers_available.begin();
															pendingSequenceInfo->data = *itrAvailableBuffer;
															xhv_engine->pending_voice_frame_buffers_available.erase(itrAvailableBuffer);
															
															LeaveCriticalSection(&xhv_engine->lock_pending_voice_frame_buffers);
														}
														else {
															LeaveCriticalSection(&xhv_engine->lock_pending_voice_frame_buffers);
															
															pendingSequenceInfo->data = new uint8_t[XHV_OPUS_PCM_FRAME_SIZE];
														}
													}
													
													pendingSequenceInfo->data_size = bytesEncoded;
													pendingSequenceInfo->data_received_size = bytesEncoded;
													memcpy_s(
														pendingSequenceInfo->data
														, XHV_OPUS_PCM_FRAME_SIZE
														, opusDecodedRaw
														, pendingSequenceInfo->data_received_size
													);
												}
											}
										}
										
										delete[] pcmData;
										pcmData = 0;
									}
								}
							}
							
							audioDeviceInputInfo->wavein_buffers_in_use.insert(waveinWavehdr);
							audioDeviceBuffersToSubmit.push_back(waveinWavehdr);
							
							//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "XHV Passed wavein buffer back in.");
						}
						audioDeviceInputInfo->wavein_buffers_containing_data.clear();
					}
					
					LeaveCriticalSection(&audioDeviceInputInfo->lock_wave_buffer);
				}
				
				// Send the empty buffer to the system to use for putting microphone data into.
				for (const auto &waveinWavehdr : audioDeviceBuffersToSubmit) {
					MMRESULT resultWaveInAddBuffer = waveInAddBuffer(audioDeviceInputInfo->handle_wavein, waveinWavehdr, sizeof(*waveinWavehdr));
					if (resultWaveInAddBuffer) {
						XLLN_DEBUG_LOG_ECODE(resultWaveInAddBuffer, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XHVEngine (0x%zx) handle_wavein (0x%zx) failed to waveInAddBuffer(...).", xhv_engine, audioDeviceInputInfo->handle_wavein);
					}
				}
				audioDeviceBuffersToSubmit.clear();
			}
			
			LeaveCriticalSection(&xhv_engine->lock_audio_devices);
			xhv_engine->Lock(XHV_LOCK_TYPE_UNLOCK);
		}
	}
	
	return ERROR_SUCCESS;
}

static DWORD WINAPI XHVThreadProcessAudioOutput(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	XHVEngine* xhv_engine = (XHVEngine*)lpParam;
	
	while (1) {
		// TODO XHV_THREAD_WAVEOUT_ITR_FREQUENCY_MSEC should be from the last time this thread finished waiting (since a single iteration of the loop might take a few milliseconds).
		DWORD resultWait = WaitForSingleObject(xhv_engine->worker_thread_waveout_signal, XHV_THREAD_WAVEOUT_ITR_FREQUENCY_MSEC);
		if (resultWait != WAIT_OBJECT_0 && resultWait != STATUS_TIMEOUT) {
			XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XHVEngine (0x%zx) worker_thread_waveout failed to wait on worker_thread_waveout_signal (0x%zx).", xhv_engine, xhv_engine->worker_thread_waveout_signal);
			break;
		}
		
		if (xhv_engine->worker_thread_waveout_exit) {
			break;
		}
		
		{
			xhv_engine->Lock(XHV_LOCK_TYPE_LOCK);
			
			__timeb64 currentTime;
			_ftime64_s(&currentTime);
			
			// Iterate over both to check if there's audio to submit to the local device playback queue.
			auto itrRegisteredUserRemote = xhv_engine->registered_users_remote.begin();
			auto itrRegisteredUserLoopback = xhv_engine->registered_users_loopback.begin();
			while (1) {
				VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = 0;
				if (itrRegisteredUserRemote == xhv_engine->registered_users_remote.end()) {
					if (itrRegisteredUserLoopback == xhv_engine->registered_users_loopback.end()) {
						break;
					}
					registeredUserRemote = itrRegisteredUserLoopback->second;
					itrRegisteredUserLoopback++;
				}
				else {
					registeredUserRemote = itrRegisteredUserRemote->second;
					itrRegisteredUserRemote++;
				}
				
				if (registeredUserRemote->xhv_data_in_pending.size() && IsTimeUnset(registeredUserRemote->xhv_data_consume_timeout)) {
					registeredUserRemote->xhv_data_consume_after = currentTime;
					TimeAddMillis(registeredUserRemote->xhv_data_consume_after, XHV_REMOTE_START_CONSUME_DELAY_MSEC);
					
					registeredUserRemote->xhv_data_consume_timeout = currentTime;
					TimeAddMillis(registeredUserRemote->xhv_data_consume_timeout, XHV_REMOTE_START_CONSUME_DELAY_MSEC + XHV_REMOTE_CONSUME_TIMEOUT_MSEC);
					
					//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					//	, "%s REMOTE New sentence, delaying start."
					//	, __func__
					//);
				}
				
				bool timeoutOccurred = (!IsTimeUnset(registeredUserRemote->xhv_data_consume_timeout) && IsTimeLessThan(registeredUserRemote->xhv_data_consume_timeout, currentTime));
				
				const size_t framesPerBuffer = XHV_PCM_WAVE_BUFFER_MSEC_LENGTH / XHV_OPUS_PCM_FRAME_MSEC_LENGTH;
				
				{
					if (IsTimeLessThan(currentTime, registeredUserRemote->xhv_data_consume_after)) {
						if (timeoutOccurred) {
							// xhv_data_consume_after should never be greater than xhv_data_consume_timeout.
							__debugbreak();
						}
						continue;
					}
					
					// Abort if a timeout has occurred or there are not enough potential pending frames to fill a device buffer.
					if (!timeoutOccurred && registeredUserRemote->xhv_data_in_pending.size() < framesPerBuffer) {
						continue;
					}
					
					size_t completePendingFrames = 0;
					for (const auto &pendingSequenceInfo : registeredUserRemote->xhv_data_in_pending) {
						if (pendingSequenceInfo->data_size && pendingSequenceInfo->data_received_size >= pendingSequenceInfo->data_size) {
							completePendingFrames++;
						}
						else {
							break;
						}
					}
					
					const size_t frameDropThresholdFrameQueuedAboveCount = (XHV_CODEC_SEQUENCE_MAX_PENDING / 2);
					bool dropThresholdReached = (completePendingFrames < framesPerBuffer && completePendingFrames + frameDropThresholdFrameQueuedAboveCount <= registeredUserRemote->xhv_data_in_pending.size());
					
					if (!timeoutOccurred && completePendingFrames < framesPerBuffer && !dropThresholdReached) {
						continue;
					}
				}
				
				{
					std::vector<PENDING_SEQUENCE_INFO*> freePendingFrames;
					std::vector<uint8_t*> decodedBuffers;
					
					{
						size_t totalFrames = 0;
						size_t totalFramesSucceeded = 0;
						
						uint8_t* decodedBuffer = 0;
						size_t iFrame = 0;
						while (registeredUserRemote->xhv_data_in_pending.size() && (timeoutOccurred || iFrame < framesPerBuffer)) {
							if (iFrame == framesPerBuffer || !decodedBuffer) {
								iFrame = 0;
								decodedBuffer = new uint8_t[xhv_engine->wave_prepared_buffer_size];
								decodedBuffers.push_back(decodedBuffer);
							}
							size_t iFrameInner = iFrame++;
							
							const auto &itrPendingSequenceInfo = registeredUserRemote->xhv_data_in_pending.begin();
							PENDING_SEQUENCE_INFO* pendingSequenceInfo = *itrPendingSequenceInfo;
							registeredUserRemote->xhv_data_in_pending.erase(itrPendingSequenceInfo);
							registeredUserRemote->packet_sequence_number = ((registeredUserRemote->packet_sequence_number + 1) % XHV_CODEC_HEADER_SEQUENCE_MAX);
							freePendingFrames.push_back(pendingSequenceInfo);
							
							totalFrames++;
							
							if (!pendingSequenceInfo->data_size || pendingSequenceInfo->data_received_size < pendingSequenceInfo->data_size) {
								memset(&((uint8_t*)decodedBuffer)[iFrameInner * XHV_OPUS_PCM_FRAME_SIZE], 0, XHV_OPUS_PCM_FRAME_SIZE);
								continue;
							}
							
							int frameSize = opus_decode(
								xhv_engine->opus_decoder
								, pendingSequenceInfo->data
								, (uint32_t)pendingSequenceInfo->data_size
								, (int16_t*)&((uint8_t*)decodedBuffer)[iFrameInner * XHV_OPUS_PCM_FRAME_SIZE]
								, XHV_OPUS_PCM_SAMPLE_COUNT_PER_FRAME
								, 0
							);
							if (frameSize < 0) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
									, "%s REMOTE opus_decode(...) failed with error: %s."
									, __func__
									, opus_strerror(frameSize)
								);
								memset(&((uint8_t*)decodedBuffer)[iFrameInner * XHV_OPUS_PCM_FRAME_SIZE], 0, XHV_OPUS_PCM_FRAME_SIZE);
								continue;
							}
							
							totalFramesSucceeded++;
						}
						
						// If the number of frames was not enough to fill the entire buffer, then zero the remainder.
						size_t zeroSize = (framesPerBuffer - iFrame) * XHV_OPUS_PCM_FRAME_SIZE;
						if (decodedBuffer && zeroSize) {
							memset(&((uint8_t*)decodedBuffer)[iFrame * XHV_OPUS_PCM_FRAME_SIZE], 0, zeroSize);
						}
						
						//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						//	, "%s REMOTE totalFrames (%zu), failed count (%zu)."
						//	, __func__
						//	, totalFrames
						//	, totalFrames - totalFramesSucceeded
						//);
					}
					
					if (timeoutOccurred) {
						SetTimeUnset(registeredUserRemote->xhv_data_consume_timeout);
						
						//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						//	, "%s REMOTE Timeout occurred."
						//	, __func__
						//);
					}
					else {
						if (IsTimeUnset(registeredUserRemote->xhv_data_consume_after)) {
							registeredUserRemote->xhv_data_consume_timeout = currentTime;
							TimeAddMillis(registeredUserRemote->xhv_data_consume_timeout, XHV_REMOTE_CONSUME_TIMEOUT_MSEC);
						}
						else {
							SetTimeUnset(registeredUserRemote->xhv_data_consume_after);
							//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							//	, "%s REMOTE sentence is now starting."
							//	, __func__
							//);
						}
						//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						//	, "%s REMOTE pending pcm buffers: %u."
						//	, __func__
						//	, ((completePendingFrames * XHV_OPUS_PCM_FRAME_MSEC_LENGTH) / XHV_PCM_WAVE_BUFFER_MSEC_LENGTH)
						//);
					}
					
					for (auto &pendingSequenceInfo : freePendingFrames) {
						if (pendingSequenceInfo->data) {
							EnterCriticalSection(&xhv_engine->lock_pending_voice_frame_buffers);
							xhv_engine->pending_voice_frame_buffers_available.push_back(pendingSequenceInfo->data);
							LeaveCriticalSection(&xhv_engine->lock_pending_voice_frame_buffers);
							pendingSequenceInfo->data = 0;
						}
						delete pendingSequenceInfo;
					}
					freePendingFrames.clear();
					
					// If there is actually no audio data to be played back then abort.
					if (decodedBuffers.size() == 0) {
						bool isTalkingUpdate = (registeredUserRemote->is_talking != false);
						registeredUserRemote->is_talking = false;
						if (isTalkingUpdate) {
							PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_TALKING_UPDATE, 0, 0);
						}
						
						continue;
					}
					
					//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					//	, "%s REMOTE submitting part for playback."
					//	, __func__
					//);
					
					// Find all Audio Devices to playback on.
					std::map<AUDIO_DEVICE_OUTPUT_INFO*, std::pair<XHV_PLAYBACK_PRIORITY, uint16_t>> playbackOnDevices;
					
					for (const auto &itrRegisteredUserLocal : xhv_engine->registered_users_local) {
						VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal.second;
						
						if (!registeredUserLocal->audio_device_output_info) {
							continue;
						}
						
						XHV_PLAYBACK_PRIORITY playbackPriorityNew = XHV_PLAYBACK_PRIORITY_MAX;
						uint16_t playbackVolumeNew = XHV_AUDIO_SETTING_VOLUME_MAX;
						
						const auto &itrPlaybackPriority = registeredUserRemote->playback_priority.find(registeredUserLocal->user_index);
						if (itrPlaybackPriority != registeredUserRemote->playback_priority.end()) {
							playbackPriorityNew = itrPlaybackPriority->second;
						}
						
						if (playbackPriorityNew == XHV_PLAYBACK_PRIORITY_NEVER) {
							continue;
						}
						
						{
							EnterCriticalSection(&xlive_critsec_local_user);
							
							auto &voiceRemoteVolumes = xlive_local_users[registeredUserLocal->user_index].xhv_voice_remote_volume;
							auto itrVoiceRemoteVolume = voiceRemoteVolumes.find(registeredUserRemote->user_xuid);
							if (itrVoiceRemoteVolume != voiceRemoteVolumes.end()) {
								playbackVolumeNew = itrVoiceRemoteVolume->second;
							}
							
							// Combine with user set device output volume.
							playbackVolumeNew = (uint16_t)(((uint32_t)playbackVolumeNew * (uint32_t)xlive_local_users[registeredUserLocal->user_index].audio_output_device_volume) / (uint32_t)XHV_AUDIO_SETTING_VOLUME_MAX);
							
							LeaveCriticalSection(&xlive_critsec_local_user);
						}
						
						if (playbackVolumeNew == 0) {
							continue;
						}
						
						const auto &itrPlaybackOnDevice = playbackOnDevices.find(registeredUserLocal->audio_device_output_info);
						if (itrPlaybackOnDevice != playbackOnDevices.end()) {
							if (playbackPriorityNew > itrPlaybackOnDevice->second.first) {
								// Previous higher priority is used.
								playbackPriorityNew = itrPlaybackOnDevice->second.first;
							}
							if (playbackVolumeNew < itrPlaybackOnDevice->second.second) {
								// Previous higher volume is used.
								playbackVolumeNew = itrPlaybackOnDevice->second.second;
							}
						}
						
						playbackOnDevices[registeredUserLocal->audio_device_output_info] = std::make_pair(playbackPriorityNew, playbackVolumeNew);
					}
					
					// Set to not talking in the scenario that a remote user is part way through a sentence when muted.
					if (!playbackOnDevices.size()) {
						bool isTalkingUpdate = (registeredUserRemote->is_talking != false);
						registeredUserRemote->is_talking = false;
						if (isTalkingUpdate) {
							PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_TALKING_UPDATE, 0, 0);
						}
					}
					
					// Submit this audio to each of the local device playback queues.
					while (decodedBuffers.size()) {
						const auto &itrDecodedBuffer = decodedBuffers.begin();
						uint8_t* decodedBuffer = *itrDecodedBuffer;
						decodedBuffers.erase(itrDecodedBuffer);
						
						auto itrPlaybackOnDevice = playbackOnDevices.begin();
						while (itrPlaybackOnDevice != playbackOnDevices.end()) {
							AUDIO_DEVICE_OUTPUT_INFO* audioDeviceOutputInfo = itrPlaybackOnDevice->first;
							// TODO do something with playback_priority.
							XHV_PLAYBACK_PRIORITY playback_priority = itrPlaybackOnDevice->second.first;
							uint16_t playbackVolume = itrPlaybackOnDevice->second.second;
							
							itrPlaybackOnDevice++;
							
							uint8_t* decodedBufferToSubmit = decodedBuffer;
							// Copy the buffer if there are still more to go.
							if (itrPlaybackOnDevice != playbackOnDevices.end()) {
								decodedBufferToSubmit = new uint8_t[xhv_engine->wave_prepared_buffer_size];
								memcpy_s(
									decodedBufferToSubmit
									, xhv_engine->wave_prepared_buffer_size
									, decodedBuffer
									, xhv_engine->wave_prepared_buffer_size
								);
							}
							else {
								decodedBuffer = 0;
							}
							
							if (playbackVolume != XHV_AUDIO_SETTING_VOLUME_MAX) {
								for (uint32_t iSample = 0; iSample < (xhv_engine->wave_prepared_buffer_size / XHV_PCM_BYTES_PER_SAMPLE); iSample++) {
									int32_t sampleValue = ((int16_t*)decodedBufferToSubmit)[iSample];
									sampleValue = ((int32_t)sampleValue * playbackVolume) / (int32_t)XHV_AUDIO_SETTING_VOLUME_MAX;
									if (sampleValue > INT16_MAX) {
										sampleValue = INT16_MAX;
									}
									else if (sampleValue < INT16_MIN) {
										sampleValue = INT16_MIN;
									}
									((int16_t*)decodedBufferToSubmit)[iSample] = (int16_t)sampleValue;
								}
							}
							
							auto &itrSubmissionQueue = audioDeviceOutputInfo->audio_mix_queue[registeredUserRemote];
							itrSubmissionQueue.push_back(decodedBufferToSubmit);
						}
						
						if (decodedBuffer) {
							delete[] decodedBuffer;
							decodedBuffer = 0;
						}
					}
					
					playbackOnDevices.clear();
				}
			}
			
			EnterCriticalSection(&xhv_engine->lock_audio_devices);
			// Check if there is pending audio to mix and submit to the OS.
			for (const auto &itrAudioDeviceOutputInfo : xhv_engine->audio_devices_output) {
				AUDIO_DEVICE_OUTPUT_INFO* audioDeviceOutputInfo = itrAudioDeviceOutputInfo.second;
				
				if (audioDeviceOutputInfo->buffer_returned) {
					audioDeviceOutputInfo->buffer_returned = false;
					
					// Erase any empty queues and report stopped talking if no data queued elsewhere.
					for (auto itrUserAudioQueue = audioDeviceOutputInfo->audio_mix_queue.begin(); itrUserAudioQueue != audioDeviceOutputInfo->audio_mix_queue.end();) {
						VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = itrUserAudioQueue->first;
						auto &submissionQueue = itrUserAudioQueue->second;
						bool erased = (submissionQueue.size() == 0);
						if (erased) {
							audioDeviceOutputInfo->audio_mix_queue.erase(itrUserAudioQueue++);
							//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							//	, "%s DEVICE erased queue."
							//	, __func__
							//);
						}
						else {
							itrUserAudioQueue++;
						}
						
						if (erased && !registeredUserRemote->xhv_data_in_pending.size()) {
							bool isTalkingUpdate = (registeredUserRemote->is_talking != false);
							registeredUserRemote->is_talking = false;
							if (isTalkingUpdate) {
								PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_TALKING_UPDATE, 0, 0);
							}
						}
					}
				}
				
				if (IsTimeLessThan(currentTime, audioDeviceOutputInfo->waveout_submit_after)) {
					continue;
				}
				
				if (audioDeviceOutputInfo->audio_mix_queue.size()) {
					bool hasSubmission = false;
					for (auto &itrUserAudioQueue : audioDeviceOutputInfo->audio_mix_queue) {
						auto &submissionQueue = itrUserAudioQueue.second;
						if (submissionQueue.size()) {
							hasSubmission = true;
							break;
						}
					}
					
					if (!hasSubmission) {
						// Holding briefly for a delayed submission since there is at least one empty submissionQueue.
						continue;
					}
					
					WAVEHDR* waveoutWavehdr = 0;
					bool startingNewPlayback = false;
					{
						EnterCriticalSection(&audioDeviceOutputInfo->lock_wave_buffer);
						
						startingNewPlayback = !audioDeviceOutputInfo->waveout_buffers_in_use.size();
						
						auto itrWaveoutWavehdr = audioDeviceOutputInfo->waveout_buffers_available.begin();
						if (itrWaveoutWavehdr != audioDeviceOutputInfo->waveout_buffers_available.end()) {
							waveoutWavehdr = *itrWaveoutWavehdr;
							
							audioDeviceOutputInfo->waveout_buffers_available.erase(itrWaveoutWavehdr);
							audioDeviceOutputInfo->waveout_buffers_in_use.insert(waveoutWavehdr);
						}
						
						LeaveCriticalSection(&audioDeviceOutputInfo->lock_wave_buffer);
					}
					
					if (!waveoutWavehdr) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s DEVICE No waveout_buffers_available."
							, __func__
						);
						break;
					}
					
					waveoutWavehdr->dwBufferLength = xhv_engine->wave_prepared_buffer_size;
					
					if (startingNewPlayback) {
						audioDeviceOutputInfo->waveout_submit_after = currentTime;
						TimeAddMillis(audioDeviceOutputInfo->waveout_submit_after, (XHV_PCM_WAVE_BUFFER_MSEC_LENGTH/2));
						//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						//	, "%s DEVICE waveout_submit_after begin."
						//	, __func__
						//);
					}
					else {
						TimeAddMillis(audioDeviceOutputInfo->waveout_submit_after, XHV_PCM_WAVE_BUFFER_MSEC_LENGTH);
					}
					
					bool firstMix = true;
					for (auto &itrUserAudioQueue : audioDeviceOutputInfo->audio_mix_queue) {
						VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = itrUserAudioQueue.first;
						uint8_t* audioBufferToMix = 0;
						{
							auto &submissionQueue = itrUserAudioQueue.second;
							//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							//	, "%s DEVICE submissionQueue=%u."
							//	, __func__
							//	, submissionQueue.size()
							//);
							auto itrSubmissionQueue = submissionQueue.begin();
							if (itrSubmissionQueue != submissionQueue.end()) {
								audioBufferToMix = *itrSubmissionQueue;
								submissionQueue.erase(itrSubmissionQueue++);
							}
						}
						
						if (!audioBufferToMix) {
							continue;
						}
						
						bool isTalkingUpdate = (registeredUserRemote->is_talking != true);
						registeredUserRemote->is_talking = true;
						if (isTalkingUpdate) {
							PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_TALKING_UPDATE, 0, 0);
						}
						
						if (firstMix) {
							firstMix = false;
							memcpy_s(
								waveoutWavehdr->lpData
								, xhv_engine->wave_prepared_buffer_size
								, audioBufferToMix
								, xhv_engine->wave_prepared_buffer_size
							);
						}
						else {
							// Mix the two audio buffers and clip out of range values.
							for (uint32_t iSample = 0; iSample < (xhv_engine->wave_prepared_buffer_size / XHV_PCM_BYTES_PER_SAMPLE); iSample++) {
								int32_t sampleValue = ((int16_t*)waveoutWavehdr->lpData)[iSample];
								sampleValue += ((int16_t*)audioBufferToMix)[iSample];
								if (sampleValue > INT16_MAX) {
									sampleValue = INT16_MAX;
								}
								else if (sampleValue < INT16_MIN) {
									sampleValue = INT16_MIN;
								}
								((int16_t*)waveoutWavehdr->lpData)[iSample] = (int16_t)sampleValue;
							}
						}
						
						delete[] audioBufferToMix;
						audioBufferToMix = 0;
					}
					
					if (firstMix) {
						memset(
							waveoutWavehdr->lpData
							, 0
							, xhv_engine->wave_prepared_buffer_size
						);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s DEVICE empty submissionQueue."
							, __func__
						);
					}
					
					MMRESULT resultWaveOutPrepareHeader = waveOutPrepareHeader(audioDeviceOutputInfo->handle_waveout, waveoutWavehdr, sizeof(*waveoutWavehdr));
					if (resultWaveOutPrepareHeader) {
						__debugbreak();
					}
					
					MMRESULT resultWaveOutWrite = waveOutWrite(audioDeviceOutputInfo->handle_waveout, waveoutWavehdr, sizeof(*waveoutWavehdr));
					if (resultWaveOutWrite) {
						__debugbreak();
					}
				}
			}
			LeaveCriticalSection(&xhv_engine->lock_audio_devices);
			
			__timeb64 currentTimeUpdated;
			_ftime64_s(&currentTimeUpdated);
			uint64_t differenceMilliseconds = (1000 * (currentTimeUpdated.time - currentTime.time)) + (currentTimeUpdated.millitm - currentTime.millitm);
			if (differenceMilliseconds >= (XHV_THREAD_WAVEOUT_ITR_FREQUENCY_MSEC / 2)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s took %lu ms."
					, __func__
					, differenceMilliseconds
				);
			}
			
			xhv_engine->Lock(XHV_LOCK_TYPE_UNLOCK);
		}
	}
	
	return ERROR_SUCCESS;
}

XHVEngine::XHVEngine(XHV_INIT_PARAMS* init_params)
{
	TRACE_FX();
	
	InitializeCriticalSection(&this->xhv_lock);
	
	this->xhv_reference_count = 1;
	
	this->registered_users_max_local = init_params->dwMaxLocalTalkers;
	this->registered_users_max_remote = init_params->dwMaxRemoteTalkers;
	this->relax_privileges = !!(init_params->bRelaxPrivileges);
	this->hwnd_title_focus = init_params->hwndFocus;
	this->pfnMicrophoneRawDataReady = init_params->bCustomVADProvided ? init_params->pfnMicrophoneRawDataReady : 0;
	
	if (this->pfnMicrophoneRawDataReady) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "XHVEngine (0x%zx) A custom voice activation detection (VAD) callback (0x%zx) has been provided.", this, this->pfnMicrophoneRawDataReady);
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL, "XHVEngine: The provided custom voice activation detection (VAD) callback has been discarded as implementation details are unknown.");
		this->pfnMicrophoneRawDataReady = 0;
	}
	
	for (uint32_t iProcessingMode = 0; iProcessingMode < init_params->dwNumLocalTalkerEnabledModes; iProcessingMode++) {
		this->processing_modes_enabled_local.insert(init_params->localTalkerEnabledModes[iProcessingMode]);
	}
	for (uint32_t iProcessingMode = 0; iProcessingMode < init_params->dwNumRemoteTalkerEnabledModes; iProcessingMode++) {
		this->processing_modes_enabled_remote.insert(init_params->remoteTalkerEnabledModes[iProcessingMode]);
	}
	
	this->wave_format.wFormatTag = WAVE_FORMAT_PCM;
	this->wave_format.nChannels = 1;
	this->wave_format.nSamplesPerSec = XHV_PCM_SAMPLE_RATE;
	// There is code that absolutely depends on the sample size being 16-bit/2-byte.
	this->wave_format.wBitsPerSample = XHV_PCM_BYTES_PER_SAMPLE * 8;
	this->wave_format.nBlockAlign = this->wave_format.nChannels * (this->wave_format.wBitsPerSample / 8);
	this->wave_format.nAvgBytesPerSec = this->wave_format.nSamplesPerSec * this->wave_format.nBlockAlign;
	this->wave_format.cbSize = 0;
	
	this->wave_prepared_buffer_size = this->wave_format.nAvgBytesPerSec * XHV_PCM_WAVE_BUFFER_MSEC_LENGTH / 1000;
	
	if (this->wave_prepared_buffer_size != (XHV_PCM_WAVE_BUFFER_MSEC_LENGTH / XHV_OPUS_PCM_FRAME_MSEC_LENGTH) * XHV_OPUS_PCM_FRAME_SIZE) {
		__debugbreak();
	}
	
	InitializeCriticalSection(&this->lock_audio_devices);
	InitializeCriticalSection(&this->lock_pending_voice_frame_buffers);
	
	int errorCode = 0;
	this->opus_encoder = opus_encoder_create(XHV_PCM_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &errorCode);
	if (errorCode < 0) {
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s opus_encoder_create(...) failed."
			, __func__
		);
	}
	
	errorCode = opus_encoder_ctl(this->opus_encoder, OPUS_SET_BITRATE(xlive_xhv_engine_voice_encoded_bitrate));
	if (errorCode < 0) {
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s opus_encoder_ctl( OPUS_SET_BITRATE ) failed."
			, __func__
		);
	}
	
	errorCode = opus_encoder_ctl(this->opus_encoder, OPUS_SET_COMPLEXITY(10));
	if (errorCode < 0) {
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s opus_encoder_ctl( OPUS_SET_COMPLEXITY ) failed."
			, __func__
		);
	}
	
	this->opus_decoder = opus_decoder_create(XHV_PCM_SAMPLE_RATE, 1, &errorCode);
	if (errorCode < 0) {
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s opus_decoder_create(...) failed."
			, __func__
		);
	}
	
	this->worker_thread_wavein_exit = false;
	this->worker_thread_wavein_signal = CreateEventA(NULL, FALSE, FALSE, NULL);
	this->worker_thread_wavein = CreateThread(NULL, 0, XHVThreadProcessAudioInput, (void*)this, 0, NULL);
	
	this->worker_thread_waveout_exit = false;
	this->worker_thread_waveout_signal = CreateEventA(NULL, FALSE, FALSE, NULL);
	this->worker_thread_waveout = CreateThread(NULL, 0, XHVThreadProcessAudioOutput, (void*)this, 0, NULL);
}

XHVEngine::~XHVEngine()
{
	TRACE_FX();
	
	EnterCriticalSection(&xlive_critsec_xhv_engines);
	
	xlive_xhv_engines.erase(this);
	
	LeaveCriticalSection(&xlive_critsec_xhv_engines);
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		{
			size_t usersCount = 0;
			size_t usersCountPrevious = 0;
			while ((usersCount = this->registered_users_remote.size()) && usersCount != usersCountPrevious) {
				// To prevent infinite loops on failure.
				usersCountPrevious = usersCount;
				
				VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = this->registered_users_remote.begin()->second;
				size_t processingModesCount = registeredUserRemote->processing_modes.size();
				XUID userRemote = registeredUserRemote->user_xuid;
				
				if (processingModesCount) {
					XHV_PROCESSING_MODE* processingModes = new XHV_PROCESSING_MODE[processingModesCount];
					size_t iProcessingMode = 0;
					for (const auto &processingMode : registeredUserRemote->processing_modes) {
						if (iProcessingMode >= processingModesCount) {
							__debugbreak();
						}
						
						processingModes[iProcessingMode++] = processingMode;
						
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
							, "%s Remote Processing Mode (0x%08x) on user (0x%016I64x) is still active, shutting it down."
							, __func__
							, processingMode
							, userRemote
						);
					}
					
					HRESULT resultStopRemoteProcessingModes = this->StopRemoteProcessingModes(userRemote, processingModes, (uint32_t)processingModesCount);
					if (resultStopRemoteProcessingModes) {
						XLLN_DEBUG_LOG_ECODE(resultStopRemoteProcessingModes, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s StopRemoteProcessingModes(0x%016I64x) failed."
							, __func__
							, userRemote
						);
					}
					
					delete[] processingModes;
					processingModes = 0;
				}
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s Remote Talker (0x%016I64x) is still registered, unregistering now."
					, __func__
					, userRemote
				);
				
				HRESULT resultUnregisterRemoteTalker = this->UnregisterRemoteTalker(userRemote);
				if (resultUnregisterRemoteTalker) {
					XLLN_DEBUG_LOG_ECODE(resultUnregisterRemoteTalker, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s UnregisterRemoteTalker(0x%016I64x) failed."
						, __func__
						, userRemote
					);
				}
			}
		}
		
		{
			size_t usersCount = 0;
			size_t usersCountPrevious = 0;
			while ((usersCount = this->registered_users_local.size()) && usersCount != usersCountPrevious) {
				// To prevent infinite loops on failure.
				usersCountPrevious = usersCount;
				
				VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = this->registered_users_local.begin()->second;
				size_t processingModesCount = registeredUserLocal->processing_modes.size();
				uint32_t userIndex = registeredUserLocal->user_index;
				
				if (processingModesCount) {
					XHV_PROCESSING_MODE* processingModes = new XHV_PROCESSING_MODE[processingModesCount];
					size_t iProcessingMode = 0;
					for (const auto &processingMode : registeredUserLocal->processing_modes) {
						if (iProcessingMode >= processingModesCount) {
							__debugbreak();
						}
						
						processingModes[iProcessingMode++] = processingMode;
						
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
							, "%s Remote Processing Mode (0x%08x) on user (%u) is still active, shutting it down."
							, __func__
							, processingMode
							, userIndex
						);
					}
					
					HRESULT resultStopLocalProcessingModes = this->StopLocalProcessingModes(userIndex, processingModes, (uint32_t)processingModesCount);
					if (resultStopLocalProcessingModes) {
						XLLN_DEBUG_LOG_ECODE(resultStopLocalProcessingModes, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s StopLocalProcessingModes(%u) failed."
							, __func__
							, userIndex
						);
					}
					
					delete[] processingModes;
					processingModes = 0;
				}
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s Remote Talker (%u) is still registered, unregistering now."
					, __func__
					, userIndex
				);
				
				HRESULT resultUnregisterLocalTalker = this->UnregisterLocalTalker(userIndex);
				if (resultUnregisterLocalTalker) {
					XLLN_DEBUG_LOG_ECODE(resultUnregisterLocalTalker, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s UnregisterLocalTalker(%u) failed."
						, __func__
						, userIndex
					);
				}
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	if (this->registered_users_loopback.size()) {
		__debugbreak();
	}
	
	this->worker_thread_wavein_exit = true;
	SetEvent(this->worker_thread_wavein_signal);
	
	DWORD resultWait = WaitForSingleObject(this->worker_thread_wavein, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XHVEngine (0x%zx) destructor failed to wait for worker_thread_wavein (0x%zx).", this, this->worker_thread_wavein);
	}
	
	CloseHandle(this->worker_thread_wavein_signal);
	CloseHandle(this->worker_thread_wavein);
	
	this->worker_thread_waveout_exit = true;
	SetEvent(this->worker_thread_waveout_signal);
	
	resultWait = WaitForSingleObject(this->worker_thread_waveout, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XHVEngine (0x%zx) destructor failed to wait for worker_thread_waveout (0x%zx).", this, this->worker_thread_waveout);
	}
	
	CloseHandle(this->worker_thread_waveout_signal);
	CloseHandle(this->worker_thread_waveout);
	
	for (const auto &frameBuffer : this->pending_voice_frame_buffers_available) {
		delete[] frameBuffer;
	}
	this->pending_voice_frame_buffers_available.clear();
	
	DeleteCriticalSection(&this->lock_pending_voice_frame_buffers);
	DeleteCriticalSection(&this->lock_audio_devices);
	
	opus_encoder_destroy(this->opus_encoder);
	opus_decoder_destroy(this->opus_decoder);
	
	DeleteCriticalSection(&this->xhv_lock);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "XHVEngine (0x%zx) destroyed.", this);
}

LONG __stdcall XHVEngine::AddRef()
{
	TRACE_FX();
	LONG result = 0;
	
	result = InterlockedIncrement(&this->xhv_reference_count);
	
	return result;
}

LONG __stdcall XHVEngine::Release()
{
	TRACE_FX();
	LONG result = 0;
	
	result = InterlockedDecrement(&this->xhv_reference_count);
	
	if (!result) {
		delete this;
	}
	return result;
}

HRESULT __stdcall XHVEngine::Lock(XHV_LOCK_TYPE lock_type)
{
	TRACE_FX();
	
	switch (lock_type) {
		case XHV_LOCK_TYPE_TRYLOCK: {
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Attempting XHV_LOCK_TYPE_TRYLOCK on XHVEngine (0x%zx) Thread ID (0x%08x).", __func__, this, currentThreadId);
			if (!TryEnterCriticalSection(&this->xhv_lock)) {
				uint32_t currentThreadId = GetCurrentThreadId();
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO, "%s XHV_LOCK_TYPE_TRYLOCK failed. XHVEngine (0x%zx) Thread ID (0x%08x) already locked.", __func__, this, currentThreadId);
				return E_ABORT;
			}
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s XHV_LOCK_TYPE_TRYLOCK success on XHVEngine (0x%zx) Thread ID (0x%08x).", __func__, this, currentThreadId);
			return S_OK;
		}
		case XHV_LOCK_TYPE_LOCK: {
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Attempting XHV_LOCK_TYPE_LOCK on XHVEngine (0x%zx) Thread ID (0x%08x).", __func__, this, currentThreadId);
			EnterCriticalSection(&this->xhv_lock);
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s XHV_LOCK_TYPE_LOCK success on XHVEngine (0x%zx) Thread ID (0x%08x).", __func__, this, currentThreadId);
			return S_OK;
		}
		case XHV_LOCK_TYPE_UNLOCK: {
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Attempting XHV_LOCK_TYPE_UNLOCK on XHVEngine (0x%zx) Thread ID (0x%08x).", __func__, this, currentThreadId);
			LeaveCriticalSection(&this->xhv_lock);
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s XHV_LOCK_TYPE_UNLOCK success on XHVEngine (0x%zx) Thread ID (0x%08x).", __func__, this, currentThreadId);
			return S_OK;
		}
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lock_type (0x%08x) is invalid.", __func__, lock_type);
	return E_INVALIDARG;
}

static void CALLBACK CallbackAudioDeviceWaveInput(HWAVEIN handle_wavein, UINT message, DWORD_PTR instance, DWORD_PTR message_parameter_1, DWORD_PTR message_parameter_2)
{
	TRACE_FX();
	
	AUDIO_DEVICE_INPUT_INFO* audio_device_input_info = (AUDIO_DEVICE_INPUT_INFO*)instance;
	
	switch (message) {
		case WIM_OPEN: {
			break;
		}
		case WIM_CLOSE: {
			audio_device_input_info->handle_wavein = 0;
			
			audio_device_input_info->audio_device_capturing = false;
			
			EnterCriticalSection(&audio_device_input_info->lock_wave_buffer);
			for (const auto &itrWaveHdr : audio_device_input_info->wavein_buffers_in_use) {
				audio_device_input_info->wavein_buffers_containing_data.push_back(itrWaveHdr);
			}
			audio_device_input_info->wavein_buffers_in_use.clear();
			LeaveCriticalSection(&audio_device_input_info->lock_wave_buffer);
			
			break;
		}
		case WIM_DATA: {
			WAVEHDR* wavein_wavehdr = (WAVEHDR*)message_parameter_1;
			
			EnterCriticalSection(&audio_device_input_info->lock_wave_buffer);
			
			if (wavein_wavehdr->dwFlags & WHDR_DONE) {
				audio_device_input_info->wavein_buffers_in_use.erase(wavein_wavehdr);
				audio_device_input_info->wavein_buffers_containing_data.push_back(wavein_wavehdr);
			}
			else {
				__debugbreak();
			}
			
			SetEvent(audio_device_input_info->xhv_engine->worker_thread_wavein_signal);
			
			LeaveCriticalSection(&audio_device_input_info->lock_wave_buffer);
			
			break;
		}
	}
}

static void CALLBACK CallbackAudioDeviceWaveOutput(HWAVEOUT handle_waveout, UINT message, DWORD_PTR instance, DWORD_PTR message_parameter_1, DWORD_PTR message_parameter_2)
{
	TRACE_FX();
	
	AUDIO_DEVICE_OUTPUT_INFO* audio_device_output_info = (AUDIO_DEVICE_OUTPUT_INFO*)instance;
	
	switch (message) {
		case WOM_OPEN: {
			break;
		}
		case WOM_CLOSE: {
			audio_device_output_info->handle_waveout = 0;
			
			EnterCriticalSection(&audio_device_output_info->lock_wave_buffer);
			for (const auto &itrWaveHdr : audio_device_output_info->waveout_buffers_in_use) {
				audio_device_output_info->waveout_buffers_available.push_back(itrWaveHdr);
			}
			audio_device_output_info->waveout_buffers_in_use.clear();
			LeaveCriticalSection(&audio_device_output_info->lock_wave_buffer);
			
			break;
		}
		case WOM_DONE: {
			WAVEHDR* waveout_wavehdr = (WAVEHDR*)message_parameter_1;
			
			EnterCriticalSection(&audio_device_output_info->lock_wave_buffer);
			
			if (waveout_wavehdr->dwFlags & WHDR_DONE) {
				waveout_wavehdr->dwFlags = 0;
				audio_device_output_info->waveout_buffers_in_use.erase(waveout_wavehdr);
				audio_device_output_info->waveout_buffers_available.push_back(waveout_wavehdr);
			}
			else {
				__debugbreak();
			}
			
			LeaveCriticalSection(&audio_device_output_info->lock_wave_buffer);
			
			audio_device_output_info->buffer_returned = true;
			break;
		}
	}
}

HRESULT __stdcall XHVEngine::StartStopLocalProcessingModes(bool start, uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (start && xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return XONLINE_E_NO_USER;
	}
	if (!processing_modes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s processing_modes is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	for (uint32_t iProcessingMode = 0; iProcessingMode < processing_mode_count; iProcessingMode++) {
		if (processing_modes[iProcessingMode] != XHV_LOOPBACK_MODE && processing_modes[iProcessingMode] != XHV_VOICECHAT_MODE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid processing mode specified (0x%08x) Must be XHV_LOOPBACK_MODE and/or XHV_VOICECHAT_MODE.", __func__, processing_modes[iProcessingMode]);
			return E_INVALIDARG;
		}
	}
	
	if (processing_mode_count > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s processing_mode_count (0x%zx) is greater than XHV_MAX_PROCESSING_MODES (0x%08x).", __func__, processing_mode_count, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
		if (itrRegisteredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, user_index);
			result = E_UNEXPECTED;
		}
		else {
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
			bool error = false;
			for (uint32_t iProcessingMode = 0; iProcessingMode < processing_mode_count; iProcessingMode++) {
				if (start && !this->processing_modes_enabled_local.count(processing_modes[iProcessingMode])) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local enabled processing modes does not include 0x%08x.", __func__, processing_modes[iProcessingMode]);
					result = E_UNEXPECTED;
					error = true;
				}
				if ((registeredUserLocal->processing_modes.count(processing_modes[iProcessingMode]) > 0) == start) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local processing mode 0x%08x has already %s for user 0x%08x.", __func__, processing_modes[iProcessingMode], start ? "started" : "stopped", user_index);
					result = E_UNEXPECTED;
					error = true;
				}
			}
			if (!error) {
				for (uint32_t iProcessingMode = 0; iProcessingMode < processing_mode_count; iProcessingMode++) {
					if (start) {
						registeredUserLocal->processing_modes.insert(processing_modes[iProcessingMode]);
					}
					else {
						registeredUserLocal->processing_modes.erase(processing_modes[iProcessingMode]);
					}
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Local processing mode 0x%08x has been %s for user 0x%08x.", __func__, processing_modes[iProcessingMode], start ? "started" : "stopped", user_index);
				}
				
				{
					EnterCriticalSection(&this->lock_audio_devices);
					
					// Ensure audio device is capturing or not.
					if (registeredUserLocal->audio_device_input_info && registeredUserLocal->audio_device_input_info->handle_wavein) {
						if (registeredUserLocal->processing_modes.size()) {
							if (!registeredUserLocal->audio_device_input_info->audio_device_capturing) {
								MMRESULT resultWaveInStart = waveInStart(registeredUserLocal->audio_device_input_info->handle_wavein);
								if (resultWaveInStart) {
									__debugbreak();
								}
								registeredUserLocal->audio_device_input_info->audio_device_capturing = true;
							}
						}
						else {
							if (registeredUserLocal->audio_device_input_info->audio_device_capturing) {
								bool needDeviceCapturing = false;
								for (const auto &itrRegisteredUserLocal : this->registered_users_local) {
									VOICE_REGISTERED_USER_LOCAL* registeredUserLocalOther = itrRegisteredUserLocal.second;
									if (registeredUserLocalOther->audio_device_input_info != registeredUserLocal->audio_device_input_info) {
										continue;
									}
									if (registeredUserLocalOther->processing_modes.size()) {
										needDeviceCapturing = true;
									}
								}
								
								if (!needDeviceCapturing) {
									MMRESULT resultWaveInStop = waveInStop(registeredUserLocal->audio_device_input_info->handle_wavein);
									if (resultWaveInStop) {
										__debugbreak();
									}
									registeredUserLocal->audio_device_input_info->audio_device_capturing = false;
								}
							}
						}
					}
					
					if (!registeredUserLocal->processing_modes.size()
						|| !registeredUserLocal->audio_device_input_info
						|| !registeredUserLocal->audio_device_input_info->handle_wavein
					) {
						registeredUserLocal->is_talking = false;
					}
					
					LeaveCriticalSection(&this->lock_audio_devices);
				}
				
				const auto &itrRegisteredUserLoopback = this->registered_users_loopback.find(user_index);
				if ((registeredUserLocal->processing_modes.count(XHV_LOOPBACK_MODE) > 0)) {
					if (itrRegisteredUserLoopback == this->registered_users_loopback.end()) {
						VOICE_REGISTERED_USER_REMOTE* registeredUserLoopback = new VOICE_REGISTERED_USER_REMOTE;
						registeredUserLoopback->xhv_engine = this;
						registeredUserLoopback->user_xuid = 0;
						registeredUserLoopback->local_user_index = user_index;
						
						this->registered_users_loopback[registeredUserLoopback->local_user_index] = registeredUserLoopback;
					}
				}
				else {
					if (itrRegisteredUserLoopback != this->registered_users_loopback.end()) {
						VOICE_REGISTERED_USER_REMOTE* registeredUserLoopback = itrRegisteredUserLoopback->second;
						this->registered_users_loopback.erase(itrRegisteredUserLoopback);
						
						for (const auto &pendingSequenceInfo : registeredUserLoopback->xhv_data_in_pending) {
							if (pendingSequenceInfo->data) {
								EnterCriticalSection(&this->lock_pending_voice_frame_buffers);
								this->pending_voice_frame_buffers_available.push_back(pendingSequenceInfo->data);
								LeaveCriticalSection(&this->lock_pending_voice_frame_buffers);
								pendingSequenceInfo->data = 0;
							}
							delete pendingSequenceInfo;
						}
						registeredUserLoopback->xhv_data_in_pending.clear();
						
						delete registeredUserLoopback;
						registeredUserLoopback = 0;
					}
				}
				
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::StartLocalProcessingModes(uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count)
{
	TRACE_FX();
	HRESULT result = this->StartStopLocalProcessingModes(true, user_index, processing_modes, processing_mode_count);
	return result;
}

HRESULT __stdcall XHVEngine::StopLocalProcessingModes(uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count)
{
	TRACE_FX();
	HRESULT result = this->StartStopLocalProcessingModes(false, user_index, processing_modes, processing_mode_count);
	return result;
}

HRESULT __stdcall XHVEngine::StartStopRemoteProcessingModes(bool start, XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count)
{
	TRACE_FX();
	if (!xuid_remote_talker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!processing_modes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s processing_modes is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (processing_mode_count != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s There must be only 1 processing_modes specified, not 0x%zx.", __func__, processing_mode_count);
		return E_INVALIDARG;
	}
	if (processing_modes[0] != XHV_VOICECHAT_MODE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s The only processing mode allowed is XHV_VOICECHAT_MODE.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserRemote = this->registered_users_remote.find(xuid_remote_talker);
		if (itrRegisteredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker 0x%016I64x is not registered.", __func__, xuid_remote_talker);
			result = E_UNEXPECTED;
		}
		else {
			VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = itrRegisteredUserRemote->second;
			
			bool error = false;
			for (uint32_t iProcessingMode = 0; iProcessingMode < processing_mode_count; iProcessingMode++) {
				if (start && !this->processing_modes_enabled_local.count(processing_modes[iProcessingMode])) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local enabled processing modes does not include 0x%08x.", __func__, processing_modes[iProcessingMode]);
					result = E_UNEXPECTED;
					error = true;
				}
				if ((registeredUserRemote->processing_modes.count(processing_modes[iProcessingMode]) > 0) == start) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local processing mode 0x%08x has already %s for xuid_remote_talker 0x%016I64x.", __func__, processing_modes[iProcessingMode], start ? "started" : "stopped", xuid_remote_talker);
					result = E_UNEXPECTED;
					error = true;
				}
			}
			if (!error) {
				for (uint32_t iProcessingMode = 0; iProcessingMode < processing_mode_count; iProcessingMode++) {
					if (start) {
						registeredUserRemote->processing_modes.insert(processing_modes[iProcessingMode]);
					}
					else {
						registeredUserRemote->processing_modes.erase(processing_modes[iProcessingMode]);
					}
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Local processing mode has 0x%08x been %s for xuid_remote_talker 0x%016I64x.", __func__, processing_modes[iProcessingMode], start ? "started" : "stopped", xuid_remote_talker);
				}
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::StartRemoteProcessingModes(XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count)
{
	TRACE_FX();
	HRESULT result = this->StartStopRemoteProcessingModes(true, xuid_remote_talker, processing_modes, processing_mode_count);
	return result;
}

HRESULT __stdcall XHVEngine::StopRemoteProcessingModes(XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count)
{
	TRACE_FX();
	HRESULT result = this->StartStopRemoteProcessingModes(false, xuid_remote_talker, processing_modes, processing_mode_count);
	return result;
}

HRESULT __stdcall XHVEngine::SetMaxDecodePackets(size_t max_decode_packets)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s is not implemented in GFWL.", __func__);
	return S_OK;
}

HRESULT __stdcall XHVEngine::OpenAudioDeviceInput(const wchar_t* audio_device_name, uint32_t audio_device_index, AUDIO_DEVICE_INPUT_INFO** result_audio_device_input_info)
{
	if (WAVE_MAPPER != (uint32_t)-1) {
		__debugbreak();
	}
	*result_audio_device_input_info = 0;
	WAVEINCAPSW waveInCaps;
	
	if (audio_device_name && audio_device_index == XHV_AUDIO_DEVICE_NONE) {
		uint32_t deviceCount = waveInGetNumDevs();
		for (uint32_t iDevice = (uint32_t)-1; iDevice == (uint32_t)-1 || iDevice < deviceCount; iDevice++) {
			MMRESULT resultDeviceCaps = waveInGetDevCapsW(iDevice, &waveInCaps, sizeof(waveInCaps));
			if (resultDeviceCaps == MMSYSERR_NOERROR) {
				if (wcsncmp(audio_device_name, waveInCaps.szPname, sizeof(waveInCaps.szPname)/sizeof(waveInCaps.szPname[0])) == 0) {
					audio_device_index = iDevice;
					break;
				}
			}
		}
	}
	else if (!audio_device_name && audio_device_index != XHV_AUDIO_DEVICE_NONE) {
		MMRESULT resultDeviceCaps = waveInGetDevCapsW(audio_device_index, &waveInCaps, sizeof(waveInCaps));
		if (resultDeviceCaps == MMSYSERR_NOERROR) {
			audio_device_name = waveInCaps.szPname;
		}
	}
	
	if (!audio_device_name || audio_device_index == XHV_AUDIO_DEVICE_NONE) {
		return E_INVALIDARG;
	}
	
	{
		EnterCriticalSection(&this->lock_audio_devices);
		
		for (const auto &itrAudioDeviceInputInfo : this->audio_devices_input) {
			AUDIO_DEVICE_INPUT_INFO* audioDeviceInputInfo = itrAudioDeviceInputInfo.second;
			if (wcsncmp(audioDeviceInputInfo->audio_device_name, audio_device_name, sizeof(waveInCaps.szPname)/sizeof(waveInCaps.szPname[0])) == 0) {
				if (!audioDeviceInputInfo->handle_wavein) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s Selected existing but dead device: \"%ls\"."
						, __func__
						, audioDeviceInputInfo->audio_device_name
					);
				}
				*result_audio_device_input_info = audioDeviceInputInfo;
				break;
			}
		}
		
		if (*result_audio_device_input_info) {
			LeaveCriticalSection(&this->lock_audio_devices);
			return S_OK;
		}
		
		{
			AUDIO_DEVICE_INPUT_INFO* audioDeviceInputInfo = new AUDIO_DEVICE_INPUT_INFO;
			audioDeviceInputInfo->xhv_engine = this;
			
			MMRESULT resultWaveInOpen = waveInOpen(&audioDeviceInputInfo->handle_wavein, audio_device_index, &this->wave_format, (DWORD_PTR)CallbackAudioDeviceWaveInput, (DWORD_PTR)audioDeviceInputInfo, CALLBACK_FUNCTION | (audio_device_index != WAVE_MAPPER ? 0 : WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE));
			if (resultWaveInOpen != MMSYSERR_NOERROR) {
				LeaveCriticalSection(&this->lock_audio_devices);
				XLLN_DEBUG_LOG_ECODE(resultWaveInOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Microphone failed to initialise.", __func__);
				delete audioDeviceInputInfo;
				audioDeviceInputInfo = 0;
				return E_ABORT;
			}
			
			audioDeviceInputInfo->audio_device_name = CloneString(audio_device_name);
			
			InitializeCriticalSection(&audioDeviceInputInfo->lock_wave_buffer);
			
			for (uint32_t iBuffer = 0; iBuffer < XHV_PCM_WAVE_BUFFER_COUNT; iBuffer++) {
				uint8_t* wavein_data = new uint8_t[this->wave_prepared_buffer_size];
				
				WAVEHDR* waveinWavehdr = new WAVEHDR;
				waveinWavehdr->dwFlags = 0;
				waveinWavehdr->lpData = (char*)wavein_data;
				waveinWavehdr->dwBufferLength = this->wave_prepared_buffer_size;
				
				MMRESULT resultWaveInPrepareHeader = waveInPrepareHeader(audioDeviceInputInfo->handle_wavein, waveinWavehdr, sizeof(*waveinWavehdr));
				if (resultWaveInPrepareHeader) {
					__debugbreak();
				}
				
				MMRESULT resultWaveInAddBuffer = waveInAddBuffer(audioDeviceInputInfo->handle_wavein, waveinWavehdr, sizeof(*waveinWavehdr));
				if (resultWaveInAddBuffer) {
					__debugbreak();
				}
				
				audioDeviceInputInfo->wavein_buffers_in_use.insert(waveinWavehdr);
			}
			
			this->audio_devices_input[audioDeviceInputInfo->audio_device_name] = audioDeviceInputInfo;
			*result_audio_device_input_info = audioDeviceInputInfo;
		}
		
		LeaveCriticalSection(&this->lock_audio_devices);
	}
	
	return S_OK;
}

void __stdcall XHVEngine::CloseUnusedAudioDeviceInput(AUDIO_DEVICE_INPUT_INFO* audio_device_input_info)
{
	EnterCriticalSection(&this->lock_audio_devices);
	
	bool closeDevice = true;
	for (const auto &itrRegisteredUserLocal : this->registered_users_local) {
		VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal.second;
		if (registeredUserLocal->audio_device_input_info == audio_device_input_info) {
			closeDevice = false;
		}
	}
	
	if (!closeDevice) {
		LeaveCriticalSection(&this->lock_audio_devices);
		return;
	}
	
	size_t erased = this->audio_devices_input.erase(audio_device_input_info->audio_device_name);
	
	LeaveCriticalSection(&this->lock_audio_devices);
	
	if (!erased) {
		__debugbreak();
	}
	
	if (audio_device_input_info->handle_wavein) {
		if (audio_device_input_info->audio_device_capturing) {
			MMRESULT resultWaveInStop = waveInStop(audio_device_input_info->handle_wavein);
			if (resultWaveInStop) {
				__debugbreak();
			}
			audio_device_input_info->audio_device_capturing = false;
		}
		
		MMRESULT resultWaveInReset = waveInReset(audio_device_input_info->handle_wavein);
		if (resultWaveInReset) {
			__debugbreak();
		}
		
		MMRESULT resultWaveInClose = waveInClose(audio_device_input_info->handle_wavein);
		if (resultWaveInClose) {
			__debugbreak();
		}
		
		if (audio_device_input_info->handle_wavein) {
			__debugbreak();
		}
	}
	
	delete[] audio_device_input_info->audio_device_name;
	audio_device_input_info->audio_device_name = 0;
	
	for (const auto &waveinWavehdr : audio_device_input_info->wavein_buffers_containing_data) {
		delete[] waveinWavehdr->lpData;
		waveinWavehdr->lpData = 0;
		delete waveinWavehdr;
	}
	audio_device_input_info->wavein_buffers_containing_data.clear();
	
	if (audio_device_input_info->wavein_buffers_in_use.size()) {
		__debugbreak();
	}
	
	DeleteCriticalSection(&audio_device_input_info->lock_wave_buffer);
	
	delete audio_device_input_info;
	audio_device_input_info = 0;
}

HRESULT __stdcall XHVEngine::OpenAudioDeviceOutput(const wchar_t* audio_device_name, uint32_t audio_device_index, AUDIO_DEVICE_OUTPUT_INFO** result_audio_device_output_info)
{
	if (WAVE_MAPPER != (uint32_t)-1) {
		__debugbreak();
	}
	*result_audio_device_output_info = 0;
	WAVEOUTCAPSW waveOutCaps;
	
	if (audio_device_name && audio_device_index == XHV_AUDIO_DEVICE_NONE) {
		uint32_t deviceCount = waveOutGetNumDevs();
		for (uint32_t iDevice = (uint32_t)-1; iDevice == (uint32_t)-1 || iDevice < deviceCount; iDevice++) {
			MMRESULT resultDeviceCaps = waveOutGetDevCapsW(iDevice, &waveOutCaps, sizeof(waveOutCaps));
			if (resultDeviceCaps == MMSYSERR_NOERROR) {
				if (wcsncmp(audio_device_name, waveOutCaps.szPname, sizeof(waveOutCaps.szPname)/sizeof(waveOutCaps.szPname[0])) == 0) {
					audio_device_index = iDevice;
					break;
				}
			}
		}
	}
	else if (!audio_device_name && audio_device_index != XHV_AUDIO_DEVICE_NONE) {
		MMRESULT resultDeviceCaps = waveOutGetDevCapsW(audio_device_index, &waveOutCaps, sizeof(waveOutCaps));
		if (resultDeviceCaps == MMSYSERR_NOERROR) {
			audio_device_name = waveOutCaps.szPname;
		}
	}
	
	if (!audio_device_name || audio_device_index == XHV_AUDIO_DEVICE_NONE) {
		return E_INVALIDARG;
	}
	
	{
		EnterCriticalSection(&this->lock_audio_devices);
		
		for (const auto &itrAudioDeviceOutputInfo : this->audio_devices_output) {
			AUDIO_DEVICE_OUTPUT_INFO* audioDeviceOutputInfo = itrAudioDeviceOutputInfo.second;
			if (wcsncmp(audioDeviceOutputInfo->audio_device_name, audio_device_name, sizeof(waveOutCaps.szPname)/sizeof(waveOutCaps.szPname[0])) == 0) {
				if (!audioDeviceOutputInfo->handle_waveout) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s Selected existing but dead device: \"%ls\"."
						, __func__
						, audioDeviceOutputInfo->audio_device_name
					);
				}
				*result_audio_device_output_info = audioDeviceOutputInfo;
				break;
			}
		}
		
		if (*result_audio_device_output_info) {
			LeaveCriticalSection(&this->lock_audio_devices);
			return S_OK;
		}
		
		{
			AUDIO_DEVICE_OUTPUT_INFO* audioDeviceOutputInfo = new AUDIO_DEVICE_OUTPUT_INFO;
			audioDeviceOutputInfo->xhv_engine = this;
			
			MMRESULT resultWaveOutOpen = waveOutOpen(&audioDeviceOutputInfo->handle_waveout, audio_device_index, &this->wave_format, (DWORD_PTR)CallbackAudioDeviceWaveOutput, (DWORD_PTR)audioDeviceOutputInfo, CALLBACK_FUNCTION | (audio_device_index != WAVE_MAPPER ? 0 : WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE));
			if (resultWaveOutOpen != MMSYSERR_NOERROR) {
				LeaveCriticalSection(&this->lock_audio_devices);
				XLLN_DEBUG_LOG_ECODE(resultWaveOutOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Speaker failed to initialise.", __func__);
				delete audioDeviceOutputInfo;
				audioDeviceOutputInfo = 0;
				return E_ABORT;
			}
			
			MMRESULT resultWaveOutSetVolume = waveOutSetVolume(audioDeviceOutputInfo->handle_waveout, 0xFFFFFFFF);
			if (resultWaveOutSetVolume != MMSYSERR_NOERROR) {
				XLLN_DEBUG_LOG_ECODE(resultWaveOutOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Failed to set speaker (0x%zx) volume to 100%%."
					, __func__
					, (size_t)audioDeviceOutputInfo->handle_waveout
				);
			}
			
			audioDeviceOutputInfo->audio_device_name = CloneString(audio_device_name);
			
			InitializeCriticalSection(&audioDeviceOutputInfo->lock_wave_buffer);
			
			for (uint32_t iBuffer = 0; iBuffer < XHV_PCM_WAVE_BUFFER_COUNT; iBuffer++) {
				uint8_t* waveout_data = new uint8_t[this->wave_prepared_buffer_size];
				
				WAVEHDR* waveoutWavehdr = new WAVEHDR;
				waveoutWavehdr->dwFlags = 0;
				waveoutWavehdr->lpData = (char*)waveout_data;
				waveoutWavehdr->dwBufferLength = this->wave_prepared_buffer_size;
				waveoutWavehdr->dwLoops = 0;
				
				audioDeviceOutputInfo->waveout_buffers_available.push_back(waveoutWavehdr);
			}
			
			this->audio_devices_output[audioDeviceOutputInfo->audio_device_name] = audioDeviceOutputInfo;
			*result_audio_device_output_info = audioDeviceOutputInfo;
		}
		
		LeaveCriticalSection(&this->lock_audio_devices);
	}
	
	return S_OK;
}

void __stdcall XHVEngine::CloseUnusedAudioDeviceOutput(AUDIO_DEVICE_OUTPUT_INFO* audio_device_output_info)
{
	EnterCriticalSection(&this->lock_audio_devices);
	
	bool closeDevice = true;
	for (const auto &itrRegisteredUserLocal : this->registered_users_local) {
		VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal.second;
		if (registeredUserLocal->audio_device_output_info == audio_device_output_info) {
			closeDevice = false;
		}
	}
	
	if (!closeDevice) {
		LeaveCriticalSection(&this->lock_audio_devices);
		return;
	}
	
	size_t erased = this->audio_devices_output.erase(audio_device_output_info->audio_device_name);
	
	LeaveCriticalSection(&this->lock_audio_devices);
	
	if (!erased) {
		__debugbreak();
	}
	
	if (audio_device_output_info->handle_waveout) {
		MMRESULT resultWaveOutReset = waveOutReset(audio_device_output_info->handle_waveout);
		if (resultWaveOutReset) {
			__debugbreak();
		}
		
		MMRESULT resultWaveOutClose = waveOutClose(audio_device_output_info->handle_waveout);
		if (resultWaveOutClose) {
			__debugbreak();
		}
		
		if (audio_device_output_info->handle_waveout) {
			__debugbreak();
		}
	}
	
	delete[] audio_device_output_info->audio_device_name;
	audio_device_output_info->audio_device_name = 0;
	
	for (const auto &waveoutWavehdr : audio_device_output_info->waveout_buffers_available) {
		delete[] waveoutWavehdr->lpData;
		waveoutWavehdr->lpData = 0;
		delete waveoutWavehdr;
	}
	audio_device_output_info->waveout_buffers_available.clear();
	
	if (audio_device_output_info->waveout_buffers_in_use.size()) {
		__debugbreak();
	}
	
	DeleteCriticalSection(&audio_device_output_info->lock_wave_buffer);
	
	delete audio_device_output_info;
	audio_device_output_info = 0;
}

HRESULT SetAudioDeviceInput(uint32_t user_index, uint32_t audio_device_index)
{
	HRESULT resultCode = DSERR_NODRIVER;
	
	EnterCriticalSection(&xlive_critsec_xhv_engines);
	
	for (const auto &xhvEngine : xlive_xhv_engines) {
		HRESULT resultSetAudioDeviceInput = xhvEngine->SetAudioDeviceInput(user_index, audio_device_index);
		// Overall resultCode is S_OK if at least one call succeeds.
		if (resultCode != S_OK) {
			resultCode = resultSetAudioDeviceInput;
		}
	}
	
	LeaveCriticalSection(&xlive_critsec_xhv_engines);
	
	return resultCode;
}

HRESULT __stdcall XHVEngine::SetAudioDeviceInput(uint32_t user_index, uint32_t audio_device_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return XONLINE_E_NO_USER;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
		if (itrRegisteredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, user_index);
			result = E_UNEXPECTED;
		}
		else {
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
			
			{
				EnterCriticalSection(&this->lock_audio_devices);
				
				AUDIO_DEVICE_INPUT_INFO* audioDeviceInputInfoOld = registeredUserLocal->audio_device_input_info;
				registeredUserLocal->audio_device_input_info = 0;
				
				this->OpenAudioDeviceInput(0, audio_device_index, &registeredUserLocal->audio_device_input_info);
				
				if (audioDeviceInputInfoOld) {
					// Check if the audio device needs to be closed.
					this->CloseUnusedAudioDeviceInput(audioDeviceInputInfoOld);
				}
				
				// Ensure the device is capturing audio if a processing mode is active.
				if (
					registeredUserLocal->audio_device_input_info
					&& registeredUserLocal->processing_modes.size()
					&& registeredUserLocal->audio_device_input_info->handle_wavein
					&& !registeredUserLocal->audio_device_input_info->audio_device_capturing
				) {
					MMRESULT resultWaveInStart = waveInStart(registeredUserLocal->audio_device_input_info->handle_wavein);
					if (resultWaveInStart) {
						__debugbreak();
					}
					registeredUserLocal->audio_device_input_info->audio_device_capturing = true;
				}
				
				LeaveCriticalSection(&this->lock_audio_devices);
			}
			
			XLLNWindowUpdateUserInputBoxes(user_index);
			
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT SetAudioDeviceOutput(uint32_t user_index, uint32_t audio_device_index)
{
	HRESULT resultCode = DSERR_NODRIVER;
	
	EnterCriticalSection(&xlive_critsec_xhv_engines);
	
	for (const auto &xhvEngine : xlive_xhv_engines) {
		HRESULT resultSetAudioDeviceOutput = xhvEngine->SetAudioDeviceOutput(user_index, audio_device_index);
		// Overall resultCode is S_OK if at least one call succeeds.
		if (resultCode != S_OK) {
			resultCode = resultSetAudioDeviceOutput;
		}
	}
	
	LeaveCriticalSection(&xlive_critsec_xhv_engines);
	
	return resultCode;
}

HRESULT __stdcall XHVEngine::SetAudioDeviceOutput(uint32_t user_index, uint32_t audio_device_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return XONLINE_E_NO_USER;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
		if (itrRegisteredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, user_index);
			result = E_UNEXPECTED;
		}
		else {
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
			
			{
				EnterCriticalSection(&this->lock_audio_devices);
				
				AUDIO_DEVICE_OUTPUT_INFO* audioDeviceOutputInfoOld = registeredUserLocal->audio_device_output_info;
				registeredUserLocal->audio_device_output_info = 0;
				
				this->OpenAudioDeviceOutput(0, audio_device_index, &registeredUserLocal->audio_device_output_info);
				
				if (audioDeviceOutputInfoOld) {
					// Check if the audio device needs to be closed.
					this->CloseUnusedAudioDeviceOutput(audioDeviceOutputInfoOld);
				}
				
				LeaveCriticalSection(&this->lock_audio_devices);
			}
			
			XLLNWindowUpdateUserInputBoxes(user_index);
			
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::RegisterLocalTalker(uint32_t user_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return XONLINE_E_NO_USER;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		if (this->registered_users_local.count(user_index)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is already registered.", __func__, user_index);
			result = E_UNEXPECTED;
		}
		else if (this->registered_users_local.size() + 1 > XHV_MAX_LOCAL_TALKERS) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XHV_MAX_LOCAL_TALKERS (0x%08x) limit reached.", __func__, XHV_MAX_LOCAL_TALKERS);
			result = E_FAIL;
		}
		else if (this->registered_users_local.size() + 1 > this->registered_users_max_local) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s this->registered_users_max_local (0x%08x) limit reached.", __func__, this->registered_users_max_local);
			result = E_FAIL;
		}
		else {
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = new VOICE_REGISTERED_USER_LOCAL;
			registeredUserLocal->xhv_engine = this;
			registeredUserLocal->user_index = user_index;
			
			registeredUserLocal->xhv_codec_state.bMsgNo = 0;
			registeredUserLocal->xhv_codec_state.wSeqNo = 0;
			registeredUserLocal->xhv_codec_state.bFriendsOnly = 0;
			
			registeredUserLocal->xhv_data_out = new CyclicBuffer(registeredUserLocal->xhv_engine->wave_format.nAvgBytesPerSec * 2);
			
			this->registered_users_local[registeredUserLocal->user_index] = registeredUserLocal;
			
			this->OpenAudioDeviceInput(xlive_local_users[registeredUserLocal->user_index].audio_input_device_name, XHV_AUDIO_DEVICE_NONE, &registeredUserLocal->audio_device_input_info);
			this->OpenAudioDeviceOutput(xlive_local_users[registeredUserLocal->user_index].audio_output_device_name, XHV_AUDIO_DEVICE_NONE, &registeredUserLocal->audio_device_output_info);
			
			XLLNWindowUpdateUserInputBoxes(user_index);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s User 0x%08x has been registered.", __func__, registeredUserLocal->user_index);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::UnregisterLocalTalker(uint32_t user_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
		if (itrRegisteredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, user_index);
			result = E_UNEXPECTED;
		}
		else {
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
			
			{
				EnterCriticalSection(&this->lock_audio_devices);
				
				{
					AUDIO_DEVICE_INPUT_INFO* audioDeviceInputInfoOld = registeredUserLocal->audio_device_input_info;
					registeredUserLocal->audio_device_input_info = 0;
					
					// Check if the audio device needs to be closed.
					this->CloseUnusedAudioDeviceInput(audioDeviceInputInfoOld);
				}
				{
					AUDIO_DEVICE_OUTPUT_INFO* audioDeviceOutputInfoOld = registeredUserLocal->audio_device_output_info;
					registeredUserLocal->audio_device_output_info = 0;
					
					// Check if the audio device needs to be closed.
					this->CloseUnusedAudioDeviceOutput(audioDeviceOutputInfoOld);
				}
				
				LeaveCriticalSection(&this->lock_audio_devices);
			}
			
			for (const auto &itrRegisteredUserRemote : this->registered_users_remote) {
				itrRegisteredUserRemote.second->playback_priority.erase(registeredUserLocal->user_index);
			}
			
			delete registeredUserLocal->xhv_data_out;
			registeredUserLocal->xhv_data_out = 0;
			registeredUserLocal->xhv_data_out_frame_sizes.clear();
			
			this->registered_users_local.erase(user_index);
			
			delete registeredUserLocal;
			registeredUserLocal = 0;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s User 0x%08x has been unregistered.", __func__, user_index);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::RegisterRemoteTalker(XUID xuid_remote_talker, XAUDIOVOICEFXCHAIN* remote_talker_fx, XAUDIOVOICEFXCHAIN* talker_pair_fx, XAUDIOSUBMIXVOICE* output_voice)
{
	TRACE_FX();
	if (!xuid_remote_talker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (remote_talker_fx) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s remote_talker_fx must be NULL.", __func__);
		return E_INVALIDARG;
	}
	if (talker_pair_fx) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s talker_pair_fx must be NULL.", __func__);
		return E_INVALIDARG;
	}
	if (output_voice) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s output_voice must be NULL.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		if (this->registered_users_remote.count(xuid_remote_talker)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker 0x%016I64x is already registered.", __func__, xuid_remote_talker);
			result = E_UNEXPECTED;
		}
		else if (this->registered_users_remote.size() + 1 > XHV_MAX_REMOTE_TALKERS) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XHV_MAX_REMOTE_TALKERS (0x%08x) limit reached.", __func__, XHV_MAX_REMOTE_TALKERS);
			result = E_FAIL;
		}
		else if (this->registered_users_remote.size() + 1 > this->registered_users_max_remote) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s this->registered_users_max_remote (0x%08x) limit reached.", __func__, this->registered_users_max_remote);
			result = E_FAIL;
		}
		else {
			VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = new VOICE_REGISTERED_USER_REMOTE;
			registeredUserRemote->xhv_engine = this;
			registeredUserRemote->user_xuid = xuid_remote_talker;
			
			this->registered_users_remote[registeredUserRemote->user_xuid] = registeredUserRemote;
			
			{
				EnterCriticalSection(&xlive_critsec_remote_user);
				
				// Make an entry if one didn't already exist for this user.
				xlln_remote_user_usernames[xuid_remote_talker];
				PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USERS_UPDATE, 0, 0);
				
				LeaveCriticalSection(&xlive_critsec_remote_user);
			}
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s xuid_remote_talker 0x%016I64x has been registered.", __func__, registeredUserRemote->user_xuid);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::UnregisterRemoteTalker(XUID xuid_remote_talker)
{
	TRACE_FX();
	if (!xuid_remote_talker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserRemote = this->registered_users_remote.find(xuid_remote_talker);
		if (itrRegisteredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker 0x%016I64x is not registered.", __func__, xuid_remote_talker);
			result = E_UNEXPECTED;
		}
		else {
			VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = itrRegisteredUserRemote->second;
			
			this->registered_users_remote.erase(xuid_remote_talker);
			
			for (const auto &pendingSequenceInfo : registeredUserRemote->xhv_data_in_pending) {
				if (pendingSequenceInfo->data) {
					EnterCriticalSection(&this->lock_pending_voice_frame_buffers);
					this->pending_voice_frame_buffers_available.push_back(pendingSequenceInfo->data);
					LeaveCriticalSection(&this->lock_pending_voice_frame_buffers);
					pendingSequenceInfo->data = 0;
				}
				delete pendingSequenceInfo;
			}
			registeredUserRemote->xhv_data_in_pending.clear();
			
			delete registeredUserRemote;
			registeredUserRemote = 0;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s xuid_remote_talker 0x%016I64x has been unregistered.", __func__, xuid_remote_talker);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::GetRemoteTalkers(uint32_t* remote_talker_count, XUID* remote_talkers)
{
	TRACE_FX();
	if (!remote_talker_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s remote_talker_count is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!remote_talkers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s remote_talkers is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		uint32_t numberOfRemoteTalkers = 0;
		
		for (const auto &itrRegisteredUserRemote : this->registered_users_remote) {
			remote_talkers[numberOfRemoteTalkers++] = itrRegisteredUserRemote.first;
		}
		
		if (numberOfRemoteTalkers > this->registered_users_max_remote) {
			__debugbreak();
		}
		
		result = S_OK;
		
		*remote_talker_count = numberOfRemoteTalkers;
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

BOOL __stdcall XHVEngine::IsHeadsetPresent(uint32_t user_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return FALSE;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return FALSE;
	}
	
	BOOL result = FALSE;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
		if (itrRegisteredUserLocal != this->registered_users_local.end()) {
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
			if (registeredUserLocal->audio_device_input_info && registeredUserLocal->audio_device_input_info->handle_wavein) {
				result = TRUE;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

BOOL __stdcall XHVEngine::IsLocalTalking(uint32_t user_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return FALSE;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return FALSE;
	}
	
	BOOL result = FALSE;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
		if (itrRegisteredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, user_index);
		}
		else {
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
			if (registeredUserLocal->is_talking
				&& registeredUserLocal->audio_device_input_info
				&& registeredUserLocal->audio_device_input_info->handle_wavein
			) {
				result = TRUE;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

BOOL __stdcall XHVEngine::isRemoteTalking(XUID xuid_remote_talker)
{
	TRACE_FX();
	if (!xuid_remote_talker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker is NULL.", __func__);
		return FALSE;
	}
	
	BOOL result = FALSE;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserRemote = this->registered_users_remote.find(xuid_remote_talker);
		if (itrRegisteredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker 0x%016I64x is not registered.", __func__, xuid_remote_talker);
		}
		else {
			VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = itrRegisteredUserRemote->second;
			if (registeredUserRemote->is_talking) {
				result = TRUE;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

uint32_t __stdcall XHVEngine::GetDataReadyFlags()
{
	TRACE_FX();
	
	uint32_t result = 0;
	
	// GFWL assembly function aquires the lock yet definition comments suggests locking is not done.
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		for (uint32_t iLocalTalker = 0; iLocalTalker < XLIVE_LOCAL_USER_COUNT; iLocalTalker++) {
			const auto &itrRegisteredUserLocal = this->registered_users_local.find(iLocalTalker);
			if (itrRegisteredUserLocal != this->registered_users_local.end()) {
				VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
				if (registeredUserLocal->xhv_data_out_frame_sizes.size()) {
					result |= (1 << iLocalTalker);
				}
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::GetLocalChatData(uint32_t user_index, uint8_t* chat_data, size_t* chat_data_size, size_t* result_packet_count)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (!chat_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s chat_data is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!chat_data_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s chat_data_size is NULL.", __func__);
		return E_INVALIDARG;
	}
	size_t numOfPacketsCanFit = 0;
	if (xlive_xhv_engine_version_new) {
		if (*chat_data_size < XHV_VOICECHAT_MODE_PACKET_SIZE_NEW) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s *chat_data_size (0x%zx) is not big enough to hold at least one XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *chat_data_size, XHV_VOICECHAT_MODE_PACKET_SIZE_NEW);
			return E_OUTOFMEMORY;
		}
		numOfPacketsCanFit = *chat_data_size / XHV_VOICECHAT_MODE_PACKET_SIZE_NEW;
		if (*chat_data_size > (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_NEW)) {
			// A buffer larger than the most packets that can be produced is allowed and is used as far
			// as it needs to be, so this is only worth seeing when tracing the voice engine itself.
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_TRACE, "%s *chat_data_size (0x%zx) is bigger than (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE) (0x%08x).", __func__, *chat_data_size, XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_NEW);
		}
	}
	else {
		if (*chat_data_size < XHV_VOICECHAT_MODE_PACKET_SIZE_OLD) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s *chat_data_size (0x%zx) is not big enough to hold at least one XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *chat_data_size, XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
			return E_OUTOFMEMORY;
		}
		numOfPacketsCanFit = *chat_data_size / XHV_VOICECHAT_MODE_PACKET_SIZE_OLD;
		if (*chat_data_size > (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_OLD)) {
			// A buffer larger than the most packets that can be produced is allowed and is used as far
			// as it needs to be, so this is only worth seeing when tracing the voice engine itself.
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_TRACE, "%s *chat_data_size (0x%zx) is bigger than (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE) (0x%08x).", __func__, *chat_data_size, XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
		}
	}
	
	memset(chat_data, 0, *chat_data_size);
	
	*chat_data_size = 0;
	
	if (result_packet_count) {
		*result_packet_count = 0;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
		if (itrRegisteredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, user_index);
			result = E_UNEXPECTED;
		}
		else if (!itrRegisteredUserLocal->second->processing_modes.size()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x local processing mode not started.", __func__, user_index);
			result = E_UNEXPECTED;
		}
		else {
			const size_t xhvPacketSize = (xlive_xhv_engine_version_new ? XHV_VOICECHAT_MODE_PACKET_SIZE_NEW : XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
			VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
			const size_t frameDataBufferSize = XHV_OPUS_PCM_FRAME_SIZE;
			uint8_t frameData[frameDataBufferSize];
			
			size_t iPacket = 0;
			while (registeredUserLocal->xhv_data_out_frame_sizes.size() > 0 && iPacket < numOfPacketsCanFit) {
				size_t frameSize = *registeredUserLocal->xhv_data_out_frame_sizes.begin();
				size_t dataWrittenSize = 0;
				
				if (frameSize > frameDataBufferSize) {
					__debugbreak();
				}
				
				// After XHV_CODEC_HEADER and only in the first packet is always XHV_CODEC_FRAME_HEADER.
				// There's only 4 bits allocated to bMsgNo so make sure the frame will take no more than that number of packets to transfer.
				if (frameSize > ((0b1 << 4) * (xhvPacketSize - sizeof(XHV_CODEC_HEADER))) - sizeof(XHV_CODEC_FRAME_HEADER)) {
					__debugbreak();
				}
				
				// Peek at the data. Do not dequeue it yet.
				if (!registeredUserLocal->xhv_data_out->PopData(frameSize, frameData, &dataWrittenSize, true) || dataWrittenSize != frameSize) {
					__debugbreak();
				}
				
				size_t iFrameData = 0;
				// Skip the frame data that has already been sent if any.
				if (registeredUserLocal->xhv_codec_state.bMsgNo) {
					iFrameData = (registeredUserLocal->xhv_codec_state.bMsgNo * (xhvPacketSize - sizeof(XHV_CODEC_HEADER))) - sizeof(XHV_CODEC_FRAME_HEADER);
				}
				
				// Write data into the packets.
				while (iFrameData < frameSize && iPacket < numOfPacketsCanFit) {
					uint8_t* packetData = &chat_data[xhvPacketSize * iPacket];
					
					XHV_CODEC_HEADER &packetXhvCodecHeader = *(XHV_CODEC_HEADER*)packetData;
					packetXhvCodecHeader = registeredUserLocal->xhv_codec_state;
					
					uint8_t* packetDataContents = &packetData[sizeof(XHV_CODEC_HEADER)];
					size_t packetDataContentsSize = xhvPacketSize - sizeof(XHV_CODEC_HEADER);
					
					if (registeredUserLocal->xhv_codec_state.bMsgNo == 0) {
						XHV_CODEC_FRAME_HEADER &packetXhvCodecFrameHeader = *(XHV_CODEC_FRAME_HEADER*)packetDataContents;
						packetDataContents = &packetDataContents[sizeof(XHV_CODEC_FRAME_HEADER)];
						packetDataContentsSize -= sizeof(XHV_CODEC_FRAME_HEADER);
						
						packetXhvCodecFrameHeader.frame_size = frameSize;
					}
					
					size_t remainingSize = frameSize - iFrameData;
					if (remainingSize > packetDataContentsSize) {
						remainingSize = 0;
					}
					
					memcpy_s(
						packetDataContents
						, packetDataContentsSize
						, &frameData[iFrameData]
						, (remainingSize ? remainingSize : packetDataContentsSize)
					);
					
					registeredUserLocal->xhv_codec_state.bMsgNo++;
					iPacket++;
					iFrameData += packetDataContentsSize;
					
					if (remainingSize) {
						// Zero out the excess (if any).
						memset(&packetDataContents[remainingSize], 0, packetDataContentsSize - remainingSize);
						
						// Frame has been packaged and sent in its entirety.
						registeredUserLocal->xhv_data_out_frame_sizes.erase(registeredUserLocal->xhv_data_out_frame_sizes.begin());
						if (!registeredUserLocal->xhv_data_out->PopData(frameSize, 0, &dataWrittenSize) || dataWrittenSize != frameSize) {
							__debugbreak();
						}
						registeredUserLocal->xhv_codec_state.bMsgNo = 0;
						registeredUserLocal->xhv_codec_state.wSeqNo++;
						
						break;
					}
				}
			}
			
			if (!iPacket) {
				result = E_PENDING;
			}
			else {
				*chat_data_size = (xlive_xhv_engine_version_new ? XHV_VOICECHAT_MODE_PACKET_SIZE_NEW : XHV_VOICECHAT_MODE_PACKET_SIZE_OLD) * iPacket;
				
				//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Sending %u packets.", __func__, iPacket);
				
				if (result_packet_count) {
					*result_packet_count = iPacket;
				}
				
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::SetPlaybackPriority(XUID xuid_remote_talker, uint32_t user_index, XHV_PLAYBACK_PRIORITY playback_priority)
{
	TRACE_FX();
	if (!xuid_remote_talker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (playback_priority > XHV_PLAYBACK_PRIORITY_MIN && playback_priority != XHV_PLAYBACK_PRIORITY_NEVER) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s playback_priority must be between XHV_PLAYBACK_PRIORITY_MAX (0x%08x) and XHV_PLAYBACK_PRIORITY_MIN (0x%08x) or set to XHV_PLAYBACK_PRIORITY_NEVER (0x%08x).", __func__, XHV_PLAYBACK_PRIORITY_MAX, XHV_PLAYBACK_PRIORITY_MIN, XHV_PLAYBACK_PRIORITY_NEVER);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserRemote = this->registered_users_remote.find(xuid_remote_talker);
		if (itrRegisteredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker 0x%016I64x is not registered.", __func__, xuid_remote_talker);
			result = E_UNEXPECTED;
		}
		else {
			const auto &itrRegisteredUserLocal = this->registered_users_local.find(user_index);
			if (itrRegisteredUserLocal == this->registered_users_local.end()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, user_index);
				result = E_UNEXPECTED;
			}
			else {
				VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = itrRegisteredUserRemote->second;
				VOICE_REGISTERED_USER_LOCAL* registeredUserLocal = itrRegisteredUserLocal->second;
				
				registeredUserRemote->playback_priority[registeredUserLocal->user_index] = playback_priority;
				
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::SubmitIncomingChatData(XUID xuid_remote_talker, const uint8_t* chat_data, size_t* chat_data_size)
{
	TRACE_FX();
	if (!xuid_remote_talker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!chat_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s chat_data is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!chat_data_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s chat_data_size is NULL.", __func__);
		return E_INVALIDARG;
	}
	size_t numOfPacketsInside = 0;
	if (xlive_xhv_engine_version_new) {
		if (*chat_data_size % XHV_VOICECHAT_MODE_PACKET_SIZE_NEW) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *chat_data_size (0x%zx) is not a multiple of XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *chat_data_size, XHV_VOICECHAT_MODE_PACKET_SIZE_NEW);
			return E_INVALIDARG;
		}
		numOfPacketsInside = *chat_data_size / XHV_VOICECHAT_MODE_PACKET_SIZE_NEW;
	}
	else {
		if (*chat_data_size % XHV_VOICECHAT_MODE_PACKET_SIZE_OLD) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *chat_data_size (0x%zx) is not a multiple of XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *chat_data_size, XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
			return E_INVALIDARG;
		}
		numOfPacketsInside = *chat_data_size / XHV_VOICECHAT_MODE_PACKET_SIZE_OLD;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		const auto &itrRegisteredUserRemote = this->registered_users_remote.find(xuid_remote_talker);
		if (itrRegisteredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_remote_talker 0x%016I64x is not registered.", __func__, xuid_remote_talker);
			result = E_UNEXPECTED;
		}
		else {
			VOICE_REGISTERED_USER_REMOTE* registeredUserRemote = itrRegisteredUserRemote->second;
			
			if (!registeredUserRemote->processing_modes.count(XHV_VOICECHAT_MODE)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%016I64x remote processing mode not started.", __func__, registeredUserRemote->user_xuid);
				result = E_UNEXPECTED;
			}
			else {
				//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Receiving %u.", __func__, numOfPacketsInside);
				
				const size_t xhvPacketSize = (xlive_xhv_engine_version_new ? XHV_VOICECHAT_MODE_PACKET_SIZE_NEW : XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
				
				for (size_t iPacket = 0; iPacket < numOfPacketsInside; iPacket++) {
					const uint8_t* packetData = &chat_data[xhvPacketSize * iPacket];
					const XHV_CODEC_HEADER &packetXhvCodecHeader = *(const XHV_CODEC_HEADER*)packetData;
					const uint8_t* packetDataContents = &packetData[sizeof(XHV_CODEC_HEADER)];
					size_t packetDataContentsSize = xhvPacketSize - sizeof(XHV_CODEC_HEADER);
					
					// Reset sequence number when we first get data.
					if (registeredUserRemote->xhv_data_in_pending.size() == 0) {
						registeredUserRemote->packet_sequence_number = packetXhvCodecHeader.wSeqNo;
					}
					
					// Determine if there are old pending sequences that need dropping.
					bool wrappedAround = (packetXhvCodecHeader.wSeqNo < XHV_CODEC_SEQUENCE_MAX_PENDING);
					uint16_t sequenceNumberMin = (
						wrappedAround
						? (XHV_CODEC_HEADER_SEQUENCE_MAX - (XHV_CODEC_SEQUENCE_MAX_PENDING - packetXhvCodecHeader.wSeqNo))
						: (packetXhvCodecHeader.wSeqNo - XHV_CODEC_SEQUENCE_MAX_PENDING)
					);
					bool dropRequired = (
						(
							wrappedAround
							&& registeredUserRemote->packet_sequence_number < sequenceNumberMin
							&& registeredUserRemote->packet_sequence_number > packetXhvCodecHeader.wSeqNo
						)
						|| (
							!wrappedAround
							&& (
								registeredUserRemote->packet_sequence_number > packetXhvCodecHeader.wSeqNo
								|| registeredUserRemote->packet_sequence_number < sequenceNumberMin
							)
						)
					);
					
					if (dropRequired) {
						while (registeredUserRemote->packet_sequence_number != sequenceNumberMin) {
							auto itrPendingSequenceInfo = registeredUserRemote->xhv_data_in_pending.begin();
							if (itrPendingSequenceInfo == registeredUserRemote->xhv_data_in_pending.end()) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
									, "%s The new packet sequence (%hu) was so far ahead that the highest pending sequence (%hu) was not in range of max pending below the packet sequence."
									, __func__
									, packetXhvCodecHeader.wSeqNo
									, registeredUserRemote->packet_sequence_number
								);
								registeredUserRemote->packet_sequence_number = packetXhvCodecHeader.wSeqNo;
								break;
							}
							PENDING_SEQUENCE_INFO* pendingSequenceInfoOld = *itrPendingSequenceInfo;
							if (pendingSequenceInfoOld->data) {
								EnterCriticalSection(&this->lock_pending_voice_frame_buffers);
								this->pending_voice_frame_buffers_available.push_back(pendingSequenceInfoOld->data);
								LeaveCriticalSection(&this->lock_pending_voice_frame_buffers);
								pendingSequenceInfoOld->data = 0;
							}
							
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
								, "%s Dropping pending frame/sequence (%hu)."
								, __func__
								, registeredUserRemote->packet_sequence_number
							);
							
							//TODO try to consume the pending frame?
							delete pendingSequenceInfoOld;
							pendingSequenceInfoOld = 0;
							registeredUserRemote->xhv_data_in_pending.erase(itrPendingSequenceInfo);
							registeredUserRemote->packet_sequence_number = ((registeredUserRemote->packet_sequence_number + 1) % XHV_CODEC_HEADER_SEQUENCE_MAX);
						}
					}
					
					// Get the PENDING_SEQUENCE_INFO.
					wrappedAround = (packetXhvCodecHeader.wSeqNo < registeredUserRemote->packet_sequence_number);
					size_t pendingSequenceIndex = (
						wrappedAround
						? ((XHV_CODEC_HEADER_SEQUENCE_MAX - registeredUserRemote->packet_sequence_number) + packetXhvCodecHeader.wSeqNo)
						: (packetXhvCodecHeader.wSeqNo - registeredUserRemote->packet_sequence_number)
					);
					while (registeredUserRemote->xhv_data_in_pending.size() <= pendingSequenceIndex) {
						PENDING_SEQUENCE_INFO* pendingSequenceInfoNew = new PENDING_SEQUENCE_INFO;
						registeredUserRemote->xhv_data_in_pending.push_back(pendingSequenceInfoNew);
					}
					PENDING_SEQUENCE_INFO* pendingSequenceInfo = registeredUserRemote->xhv_data_in_pending.at(pendingSequenceIndex);
					
					// If this is the first part of a frame then before the data is the XHV_CODEC_FRAME_HEADER.
					if (packetXhvCodecHeader.bMsgNo == 0) {
						const XHV_CODEC_FRAME_HEADER &packetXhvCodecFrameHeader = *(const XHV_CODEC_FRAME_HEADER*)packetDataContents;
						packetDataContents = &packetDataContents[sizeof(XHV_CODEC_FRAME_HEADER)];
						packetDataContentsSize -= sizeof(XHV_CODEC_FRAME_HEADER);
						
						pendingSequenceInfo->data_size = packetXhvCodecFrameHeader.frame_size;
					}
					
					// If there is no buffer allocated to this pending sequence/frame.
					if (!pendingSequenceInfo->data) {
						EnterCriticalSection(&this->lock_pending_voice_frame_buffers);
						if (this->pending_voice_frame_buffers_available.size()) {
							auto itrAvailableBuffer = this->pending_voice_frame_buffers_available.begin();
							pendingSequenceInfo->data = *itrAvailableBuffer;
							this->pending_voice_frame_buffers_available.erase(itrAvailableBuffer);
							
							LeaveCriticalSection(&this->lock_pending_voice_frame_buffers);
						}
						else {
							LeaveCriticalSection(&this->lock_pending_voice_frame_buffers);
							
							pendingSequenceInfo->data = new uint8_t[XHV_OPUS_PCM_FRAME_SIZE];
						}
					}
					
					// Get the location that this part of the frame needs to be copied to inside the pending frame buffer.
					size_t iData = (packetXhvCodecHeader.bMsgNo * (xhvPacketSize - sizeof(XHV_CODEC_HEADER))) - (packetXhvCodecHeader.bMsgNo == 0 ? 0 : sizeof(XHV_CODEC_FRAME_HEADER));
					
					memcpy_s(
						&pendingSequenceInfo->data[iData]
						, packetDataContentsSize
						, packetDataContents
						, packetDataContentsSize
					);
					pendingSequenceInfo->data_received_size += packetDataContentsSize;
				}
				
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}
