#pragma once
#include <stdio.h>
#include <stdint.h>
#include <sys/timeb.h>

char* FormMallocString(const char* const format, ...);
wchar_t* FormMallocString(const wchar_t* const format, ...);
wchar_t* FormNewString(const wchar_t* const format, ...);
char* CloneString(const char* str);
wchar_t* CloneString(const wchar_t* str);
size_t TrimRemoveConsecutiveSpaces(char* text);
int FindLineStart(FILE* fp, int lineStrLen);
bool GetFileLine(FILE* fp, char* &fileLine);
bool GetFileLine(FILE* fp, wchar_t* &fileLine);
bool GetNTStringLine(char* text, size_t lineNum, char* &line);
bool GetNTStringLine(wchar_t* text, size_t lineNum, wchar_t* &line);
uint8_t CmpVersions(const char* version_base, const char* version_alt);
uint8_t CmpVersions(const wchar_t* version_base, const wchar_t* version_alt);
void ReadIniFile(void* fileConfig, bool configIsFILE, const char* header, const char* headerVersion, int(interpretSettingFunc)(const char* fileLine, const char* version, size_t lineNumber, void* interpretationContext), void* interpretationContext);
void ReadIniFile(void* fileConfig, bool configIsFILE, const wchar_t* header, const wchar_t* headerVersion, int(interpretSettingFunc)(const wchar_t* fileLine, const wchar_t* version, size_t lineNumber, void* interpretationContext), void* interpretationContext);
void GetVKeyCodeString(int vkey, char* rtnString, int strLen);
void PadCStringWithChar(char* strToPad, size_t toFullLength, char c);
int GetWidePathFromFullWideFilename(wchar_t* filepath, wchar_t* rtnpath);
LONG GetDWORDRegKey(HKEY hKey, wchar_t* strValueName, DWORD* nValue);
char* custom_label_literal(char* label_escaped);
char* custom_label_escape(char* label_literal);
bool FloatIsNaN(float &vagueFloat);
int HostnameToIp(char* hostname, char* ip, DWORD buflen);
char* encode_rfc3986(char* label_literal, size_t label_literal_length);
bool StrnCaseInsensEqu(char* str1, char* str2, size_t chk_len);
char* GetModuleFilePathA(HMODULE hModule);
wchar_t* GetModuleFilePathW(HMODULE hModule);
void wcstombs2(char* buffer, const wchar_t* text, size_t buf_len);
void ReplaceFilePathSensitiveChars(char* filename, bool remove_path_slash);
void ReplaceFilePathSensitiveChars(char* filename);
void ReplaceFilePathSensitiveChars(wchar_t* filename, bool remove_path_slash);
void ReplaceFilePathSensitiveChars(wchar_t* filename);
const char* LastPathItem(const char* pathname);
const wchar_t* LastPathItem(const wchar_t* pathname);
char* LastPathItem(char* pathname, size_t end_length);
const char* LastPathItem(const char* pathname, size_t end_length);
wchar_t* LastPathItem(wchar_t* pathname, size_t end_length);
const wchar_t* LastPathItem(const wchar_t* pathname, size_t end_length);
const char* FilenameFromPathname(const char* pathname);
const wchar_t* FilenameFromPathname(const wchar_t* pathname);
wchar_t* FilenameFromPathname(wchar_t* pathname);
char* PathFromFilename(const char* filename_path);
wchar_t* PathFromFilename(const wchar_t* filename_path);
wchar_t* GetParentPath(const wchar_t* path);
size_t LenOfFirstPathItem(const char* pathname);
size_t LenOfFirstPathItem(const wchar_t* pathname);
size_t StrSameChrFor(const char* primary, const char* secondary);
size_t StrSameChrFor(const wchar_t* primary, const wchar_t* secondary);
size_t StrSamePathFor(const char* primary, const char* secondary);
size_t StrSamePathFor(const wchar_t* primary, const wchar_t* secondary);
char* GetItemBeforeLastPos(const char* full_pathname, const char* &end_position);
wchar_t* GetItemBeforeLastPos(const wchar_t* full_pathname, const wchar_t* &end_position);
bool EndsWith(const char* str, const char* suffix);
bool EndsWith(const wchar_t* str, const wchar_t* suffix);
bool EndsWithCaseInsensitive(const wchar_t* str, const wchar_t* suffix);
uint32_t EnsureDirectoryExists(const wchar_t* path);
void CustomMemCpy(void* dst, void* src, rsize_t len, bool direction_ascending);
const uint8_t* FindBytes(const uint8_t* haystack, size_t haystack_size, const uint8_t* needle, size_t needle_size, bool search_forwards);
uint32_t GetPEChecksum(const uint8_t* data, const size_t data_size, const size_t offset_checksum);
bool ByteArrayToHexStr(char* hex_string_terminated, const uint8_t* byte_array, size_t byte_array_size);
bool HexStrToByteArray(uint8_t* byte_array, size_t byte_array_size, const char* hex_string);
bool HexStrToByteArray(uint8_t* byte_array, size_t byte_array_size, const wchar_t* hex_string);
bool WndTextboxMessageHandling(MSG* msg, HWND window_hwnd, const uint32_t* textbox_control_message_numbers, size_t textbox_count);
bool WndTrackBarMessageHandling(MSG* msg, HWND window_hwnd, const uint32_t* trackbar_control_message_numbers, size_t trackbar_count);

#define IsTimeLessThan(time_old, time_new) ((time_old).time < (time_new).time || ((time_old).time == (time_new).time && (time_old).millitm < (time_new).millitm))
#define SetTimeUnset(time_1) {(time_1).time = 0; (time_1).millitm = 0; (time_1).timezone = 0; (time_1).dstflag = 0;}
#define IsTimeUnset(time_1) ((time_1).time == 0 && (time_1).millitm == 0 && (time_1).timezone == 0 && (time_1).dstflag == 0)
#define TimeAddMillis(time_1, time_ms) { time_t seconds = (time_ms) / 1000; (time_1).millitm += (time_ms) % 1000; seconds += (time_1).millitm / 1000; (time_1).millitm = (time_1).millitm % 1000; (time_1).time += seconds; }
int64_t TimeDiffMilliseconds(__timeb64 time_old, __timeb64 time_new);
