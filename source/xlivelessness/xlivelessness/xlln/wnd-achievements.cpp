#include <winsock2.h>
#include <Windows.h>
#include "./wnd-achievements.hpp"
#include "./debug-log.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xfuncs.hpp"
#include "../xlive/xuser.hpp"
#include "../xlive/xlive.hpp"
#include <stdint.h>
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_achievements;
static HWND hwndListView = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

CRITICAL_SECTION xlln_critsec_achievements;
static uint32_t xlln_achievements_user_index = 0;

uint32_t XllnAchievementsShow(uint32_t user_index) {
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}

	EnterCriticalSection(&xlln_critsec_achievements);

	bool update = xlln_achievements_user_index != user_index;
	xlln_achievements_user_index = user_index;
	
	LeaveCriticalSection(&xlln_critsec_achievements);

	if (update) {
		PostMessageW(xlln_hwnd_achievements, XLLNControlsMessageNumbers::EVENT_ACHIEVEMENTS_USERS_UPDATE, 0, 0);
	}

	XllnShowWindow(XllnShowType::ACHIEVEMENTS_SHOW);

	return ERROR_SUCCESS;
}

static int CreateColumn(HWND hwndLV, int iCol, const wchar_t* text, int iWidth)
{
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = iWidth;
	lvc.pszText = (wchar_t*)text;
	lvc.iSubItem = iCol;
	return ListView_InsertColumn(hwndLV, iCol, &lvc);
}

static int CreateItem(HWND hwndListView, int iItem)
{
	LV_ITEM item;
	item.mask = LVIF_TEXT;
	item.iItem = iItem;
	item.iIndent = 0;
	item.iSubItem = 0;
	item.state = 0;
	item.cColumns = 0;
	item.pszText = (wchar_t*)L"";
	return ListView_InsertItem(hwndListView, &item);
}

void XllnWndAchievementsInvalidateAchievements()
{
	InvalidateRect(xlln_hwnd_achievements, NULL, FALSE);
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);

			HWND achievementsList = GetDlgItem(xlln_hwnd_achievements, XLLNControlsMessageNumbers::ACHIEVEMENTS_LST_ACHIEVEMENTS);
			MoveWindow(achievementsList, 10, 60, width - 20, height - 70, TRUE);

			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(xlln_hwnd_achievements, &ps);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);

			HBRUSH hBrush = CreateSolidBrush(0x00C8C8C8);
			SelectObject(hdc, hBrush);
			RECT bgRect;
			GetClientRect(hWnd, &bgRect);
			HRGN bgRgn = CreateRectRgnIndirect(&bgRect);
			FillRgn(hdc, bgRgn, hBrush);
			DeleteObject(bgRgn);
			DeleteObject(hBrush);

			size_t k = 0;
			uint32_t achievementsCount = 0;
			uint32_t achievementsTotal = 0;
			uint32_t credsCount = 0;
			uint32_t credsTotal = 0;

			ListView_DeleteAllItems(hwndListView);
			{
				size_t result_buffer_size = 0;
				HANDLE enumerator_handle = 0;
				uint32_t status = XUserCreateAchievementEnumerator(0, xlln_achievements_user_index, INVALID_XUID, XACHIEVEMENT_DETAILS_ALL, 0, XACHIEVEMENT_MAX_COUNT, &result_buffer_size, &enumerator_handle);

				if (status != ERROR_SUCCESS) {
					XLLN_DEBUG_LOG_ECODE(status, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
						, "%s failed to create achievement enumerator."
						, __func__
					);
					break;
				}

				uint8_t buffer[0x1000];
				size_t result_item_count = 0;

				SYSTEMTIME st, localSt;
				int timeStringSize = 0;
				wchar_t* timeString = 0;

				while (true)
				{
					status = XEnumerate(enumerator_handle, (void*)buffer, sizeof(buffer), &result_item_count, nullptr);

					if (status != ERROR_SUCCESS && status != ERROR_NO_MORE_FILES) {
						XLLN_DEBUG_LOG_ECODE(status, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
							, "%s failed to enumerate achievements."
							, __func__
						);
						break;
					}

					if (result_item_count == 0) {
						break;
					}

					for (size_t i = 0; i < result_item_count; ++i, ++k) {
						XACHIEVEMENT_DETAILS* achievementDetails = (XACHIEVEMENT_DETAILS*)buffer + i;
						uint32_t creds = achievementDetails->dwCred;
						bool achieved = AchievementEarned(achievementDetails->dwFlags);

						credsTotal += creds;
						++achievementsTotal;
						if (achieved) {
							credsCount += creds;
							++achievementsCount;
						}

						wchar_t* textItemLabel;
						uint32_t j = 0;
						CreateItem(hwndListView, k);
						ListView_SetCheckState(hwndListView, k, achieved);

#define AddAchievementItemColumn(format, ...) textItemLabel = FormMallocString(format, __VA_ARGS__); ListView_SetItemText(hwndListView, k, ++j, textItemLabel); free(textItemLabel)
#define AddAchievementItemColumnString(string) ListView_SetItemText(hwndListView, k, ++j, string)

						AddAchievementItemColumn(L"%u", achievementDetails->dwId);
						AddAchievementItemColumnString(achievementDetails->pwszLabel);
						AddAchievementItemColumnString(achievementDetails->pwszDescription);
						AddAchievementItemColumnString(achievementDetails->pwszUnachieved);
						AddAchievementItemColumn(L"%u", achievementDetails->dwImageId);
						AddAchievementItemColumn(L"%u", creds);
						AddAchievementItemColumn(L"0x%08X", achievementDetails->dwFlags);

						uint8_t type = AchievementType(achievementDetails->dwFlags);
						switch (type)
						{
							case XACHIEVEMENT_TYPE_COMPLETION:
								AddAchievementItemColumnString((LPWSTR)L"Completion");
								break;
							case XACHIEVEMENT_TYPE_LEVELING:
								AddAchievementItemColumnString((LPWSTR)L"Leveling");
								break;
							case XACHIEVEMENT_TYPE_UNLOCK:
								AddAchievementItemColumnString((LPWSTR)L"Unlock");
								break;
							case XACHIEVEMENT_TYPE_EVENT:
								AddAchievementItemColumnString((LPWSTR)L"Event");
								break;
							case XACHIEVEMENT_TYPE_TOURNAMENT:
								AddAchievementItemColumnString((LPWSTR)L"Tournament");
								break;
							case XACHIEVEMENT_TYPE_CHECKPOINT:
								AddAchievementItemColumnString((LPWSTR)L"Checkpoint");
								break;
							case XACHIEVEMENT_TYPE_OTHER:
								AddAchievementItemColumnString((LPWSTR)L"Other");
								break;
							default:
								AddAchievementItemColumn(L"%hhu", type);
								break;
						}

						AddAchievementItemColumnString((LPWSTR)(AchievementEarned(achievementDetails->dwFlags) ? L"Y" : L"N"));
						AddAchievementItemColumnString((LPWSTR)(AchievementEarnedOnline(achievementDetails->dwFlags) ? L"Y" : L"N"));
						AddAchievementItemColumnString((LPWSTR)(AchievementUnknown(achievementDetails->dwFlags) ? L"Y" : L"N"));

						if (achieved) {
							FileTimeToSystemTime(&achievementDetails->ftAchieved, &st);
							SystemTimeToTzSpecificLocalTime(NULL, &st, &localSt);

							int dateSize = GetDateFormatW(LOCALE_USER_DEFAULT, 0, &localSt, NULL, NULL, 0);
							int timeSize = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &localSt, NULL, NULL, 0);

							int requiredSize = (dateSize + timeSize) * sizeof(wchar_t);
							if (!timeString || timeStringSize < requiredSize) {
								timeStringSize = requiredSize;
								timeString = (wchar_t*)realloc(timeString, timeStringSize);
							}

							if (!timeString ||
								!GetDateFormatW(LOCALE_USER_DEFAULT, 0, &localSt, NULL, timeString, dateSize) ||
								!GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &localSt, NULL, timeString + dateSize, timeSize)) {
								continue;
							}

							timeString[dateSize - 1] = L' ';

							AddAchievementItemColumnString(timeString);
						}

#undef AddAchievementItemColumn
#undef AddAchievementItemColumnString
					}
				}

				if (timeString) {
					free(timeString);
				}

				XCloseHandle(enumerator_handle);
			}

			SetTextColor(hdc, RGB(0, 0, 0));

			{
				uint32_t achievementsPercent = achievementsTotal ? achievementsCount * 100 / achievementsTotal : 0;

				uint32_t credsPercent = credsTotal ? credsCount * 100 / credsTotal : 0;

				const char* labelToUse = FormMallocString("Achievements %u/%u %u%% Creds %u/%u %u%%", achievementsCount, achievementsTotal, achievementsPercent, credsCount, credsTotal, credsPercent);
				if (labelToUse) {
					TextOutA(hdc, 10, 40, labelToUse, (uint32_t)strlen(labelToUse));
					free((void*)labelToUse);
					labelToUse = 0;
				}
			}

			EndPaint(xlln_hwnd_achievements, &ps);

			break;
		}
		case XLLNControlsMessageNumbers::EVENT_ACHIEVEMENTS_USERS_UPDATE: {
			HWND controlComboBoxUsers = GetDlgItem(xlln_hwnd_achievements, XLLNControlsMessageNumbers::ACHIEVEMENTS_CMB_USERS);

			EnterCriticalSection(&xlln_critsec_achievements);

			while (SendMessageA(controlComboBoxUsers, CB_DELETESTRING, 0, 0) > 0);

			for (auto& itrLocalUser : xlive_local_users) {
				char* itemLabel = FormMallocString("%s", itrLocalUser.username);
				SendMessageA(controlComboBoxUsers, CB_ADDSTRING, 0, (LPARAM)itemLabel);
				free(itemLabel);
				itemLabel = 0;
			}

			SendMessageA(controlComboBoxUsers, CB_SETCURSEL, (WPARAM)xlln_achievements_user_index, 0);

			XllnWndAchievementsInvalidateAchievements();

			LeaveCriticalSection(&xlln_critsec_achievements);

			break;
		}
		case WM_COMMAND: {
			switch (wParam) {
				case ((CBN_SELCHANGE << 16) | XLLNControlsMessageNumbers::ACHIEVEMENTS_CMB_USERS): {
					HWND controlComboBoxUsers = (HWND)lParam;
					LRESULT resultItemIndex = SendMessageA(controlComboBoxUsers, CB_GETCURSEL, 0, 0);
					if (resultItemIndex != CB_ERR) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG, "CMB changed to: %u.", resultItemIndex);

						EnterCriticalSection(&xlln_critsec_achievements);

						xlln_achievements_user_index = (uint32_t)resultItemIndex;

						PostMessageW(xlln_hwnd_achievements, XLLNControlsMessageNumbers::EVENT_ACHIEVEMENTS_USERS_UPDATE, 0, 0);

						LeaveCriticalSection(&xlln_critsec_achievements);
					}

					break;
				}
			}

			return 0;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				XllnShowWindow(XllnShowType::ACHIEVEMENTS_HIDE);
				return 0;
			}
			break;
		}
		case WM_CTLCOLORSTATIC: {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
		}
		case WM_NOTIFY: {
			LPNMHDR lpnmhdr = (LPNMHDR)lParam;
			if (lpnmhdr->hwndFrom == hwndListView && lpnmhdr->code == LVN_ITEMCHANGED) {
				LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lParam;
				if ((lpnmlv->uChanged & LVIF_STATE) && ((lpnmlv->uOldState ^ lpnmlv->uNewState) & LVIS_STATEIMAGEMASK)) {
					int iItem = lpnmlv->iItem;

					wchar_t text[0x100];
					ListView_GetItemText(hwndListView, iItem, 9, text, ARRAYSIZE(text));
					if (*text != L'Y' && *text != L'N') {
						break;
					}
					bool achieved = *text == L'Y';
					bool checked = ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1;
					if (achieved == checked) {
						break;
					}

					ListView_GetItemText(hwndListView, iItem, 1, text, ARRAYSIZE(text));
					uint32_t achievementId = 0;
					if (swscanf_s(text, L"%u", &achievementId) != 1) {
						break;
					}

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG, "%s Item %d (%u) is %s and %s.", __func__, iItem, achievementId, checked ? "checked" : "unchecked", achieved ? "achieved" : "unachieved");

					XUSER_ACHIEVEMENT xuserAchievement = {};
					xuserAchievement.dwUserIndex = xlln_achievements_user_index;
					xuserAchievement.dwAchievementId = achievementId;
					uint32_t resultAchievement = ERROR_FUNCTION_FAILED;
					if (checked) {
						resultAchievement = XUserWriteAchievements(1, &xuserAchievement, NULL);
					}
					else {
						resultAchievement = XUserEraseAchievements(1, &xuserAchievement, NULL);
					}
					
					if (resultAchievement != ERROR_SUCCESS) {
						XLLN_DEBUG_LOG_ECODE(resultAchievement, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
							, "%s failed to %s achievement id (%u) for user (%u)."
							, __func__
							, checked ? "write" : "erase"
							, achievementId
							, xlln_achievements_user_index
						);
					}
				}
			}
			break;
		}
		case WM_CREATE: {
			CreateWindowA(WC_COMBOBOXA, 0, WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_TABSTOP,
				10, 10, 330, 300, hWnd, (HMENU)XLLNControlsMessageNumbers::ACHIEVEMENTS_CMB_USERS, xlln_hModule, 0);

			hwndListView = CreateWindowA(WC_LISTVIEWA, 0,
				WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT | LVS_NOSORTHEADER | WS_TABSTOP,
				10, 60, 940, 530,
				hWnd, (HMENU)XLLNControlsMessageNumbers::ACHIEVEMENTS_LST_ACHIEVEMENTS, xlln_hModule, 0);

			ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

			uint32_t j = 0;
			CreateColumn(hwndListView, ++j, L"", 30);
			CreateColumn(hwndListView, ++j, L"ID", 60);
			CreateColumn(hwndListView, ++j, L"Label", 110);
			CreateColumn(hwndListView, ++j, L"Description", 110);
			CreateColumn(hwndListView, ++j, L"Unachieved", 110);
			CreateColumn(hwndListView, ++j, L"Image ID", 60);
			CreateColumn(hwndListView, ++j, L"Cred", 60);
			CreateColumn(hwndListView, ++j, L"Flags", 80);
			CreateColumn(hwndListView, ++j, L"Type", 60);
			CreateColumn(hwndListView, ++j, L"Achieved", 30);
			CreateColumn(hwndListView, ++j, L"Online", 30);
			CreateColumn(hwndListView, ++j, L"Unknown", 30);
			CreateColumn(hwndListView, ++j, L"Time", 130);

			SetEvent(xlln_window_create_event);

			break;
		}
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}
	}

	return DefWindowProcW(hWnd, message, wParam, lParam);
}

static DWORD WINAPI XllnThreadWndAchievements(void* lpParam)
{
	srand((unsigned int)time(NULL));

	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);

	const wchar_t* windowclassname = L"XLLNDLLWindowAchievementsClass";
	HINSTANCE hModule = reinterpret_cast<HINSTANCE>(lpParam);

	WNDCLASSEXW wc;
	wc.hInstance = hModule;
	wc.lpszClassName = windowclassname;
	wc.lpfnWndProc = DLLWindowProc;
	wc.style = CS_DBLCLKS;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hIcon = LoadIconW(xlln_hModule, XLLN_LOGO_COMPACT_SQUARE_ICON);
	wc.hIconSm = LoadIconW(xlln_hModule, XLLN_LOGO_COMPACT_SQUARE_ICON);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName = NULL;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
	if (!RegisterClassExW(&wc)) {
		return FALSE;
	}

	{
		wchar_t* windowTitle = FormMallocString(L"XLLN Achievements #%d v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);

		HWND hwdParent = NULL;
		xlln_hwnd_achievements = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 600, hwdParent, 0, hModule, NULL);

		free(windowTitle);
		windowTitle = 0;
	}

	DWORD resultWait = WaitForSingleObject(xlln_window_create_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s failed to wait for the window to be created."
			, __func__
		);
	}
	CloseHandle(xlln_window_create_event);
	xlln_window_create_event = INVALID_HANDLE_VALUE;

	PostMessageW(xlln_hwnd_achievements, XLLNControlsMessageNumbers::EVENT_ACHIEVEMENTS_USERS_UPDATE, 0, 0);

	ShowWindow(xlln_hwnd_achievements, SW_HIDE);

	SetEvent(xlln_window_initialised_event);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		// Do message processing here.

		// Handle tab ordering
		if (!IsDialogMessage(xlln_hwnd_achievements, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}

	SetEvent(xlln_window_destroy_event);

	return ERROR_SUCCESS;
}

uint32_t InitXllnWndAchievements()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);

	CreateThread(0, NULL, XllnThreadWndAchievements, (void*)xlln_hModule, NULL, NULL);

	DWORD resultWait = WaitForSingleObject(xlln_window_initialised_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s failed to wait for the window to finish initialising."
			, __func__
		);
	}
	CloseHandle(xlln_window_initialised_event);
	xlln_window_initialised_event = INVALID_HANDLE_VALUE;

	return ERROR_SUCCESS;
}

uint32_t UninitXllnWndAchievements()
{
	SendMessage(xlln_hwnd_achievements, WM_CLOSE, (WPARAM)0, (LPARAM)0);

	DWORD resultWait = WaitForSingleObject(xlln_window_destroy_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s failed to wait for the window to be destroyed."
			, __func__
		);
	}
	CloseHandle(xlln_window_destroy_event);
	xlln_window_destroy_event = INVALID_HANDLE_VALUE;

	return ERROR_SUCCESS;
}
