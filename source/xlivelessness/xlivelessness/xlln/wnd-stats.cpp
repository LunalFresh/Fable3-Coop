#include <winsock2.h>
#include <Windows.h>
#include "./wnd-stats.hpp"
#include "./debug-log.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xuser.hpp"
#include "../xlive/xlive.hpp"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <float.h>
#include <time.h>
#include <vector>
#include <CommCtrl.h>

HWND xlln_hwnd_stats;
static HWND hwndListView = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

CRITICAL_SECTION xlln_critsec_stats;
static uint32_t xlln_stats_user_index = 0;

// One entry per list row, in the order the rows are populated, so the Set Value handler can recover
// the stat a selected row refers to - plus which user the rows were painted for, so an edit lands
// on that user even if the shown user changes between paint and click. Only ever touched on the
// window thread (WM_PAINT populates it, WM_COMMAND reads it), so it needs no lock of its own.
struct XLLN_STATS_ROW_REF {
	uint32_t viewId;
	uint32_t columnId;
	uint8_t type;
};
static std::vector<XLLN_STATS_ROW_REF> xlln_stats_rows;
static uint32_t xlln_stats_rows_user_index = 0;

uint32_t XllnStatsShow(uint32_t user_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}

	EnterCriticalSection(&xlln_critsec_stats);

	bool update = xlln_stats_user_index != user_index;
	xlln_stats_user_index = user_index;

	LeaveCriticalSection(&xlln_critsec_stats);

	if (update) {
		PostMessageW(xlln_hwnd_stats, XLLNControlsMessageNumbers::EVENT_STATS_USERS_UPDATE, 0, 0);
	}

	XllnShowWindow(XllnShowType::STATS_SHOW);

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

void XllnWndStatsInvalidateStats()
{
	InvalidateRect(xlln_hwnd_stats, NULL, FALSE);
}

static const wchar_t* XllnStatsTypeName(uint8_t type)
{
	switch (type) {
		case XUSER_DATA_TYPE_CONTEXT: return L"CONTEXT";
		case XUSER_DATA_TYPE_INT32: return L"INT32";
		case XUSER_DATA_TYPE_INT64: return L"INT64";
		case XUSER_DATA_TYPE_DOUBLE: return L"DOUBLE";
		case XUSER_DATA_TYPE_UNICODE: return L"UNICODE";
		case XUSER_DATA_TYPE_FLOAT: return L"FLOAT";
		case XUSER_DATA_TYPE_BINARY: return L"BINARY";
		case XUSER_DATA_TYPE_DATETIME: return L"DATETIME";
		case XUSER_DATA_TYPE_NULL: return L"NULL";
		default: return L"?";
	}
}

// Returns a freshly allocated (delete[] it) display string for the value's type.
static wchar_t* XllnStatsFormatValue(const XUSER_DATA& value)
{
	switch (value.type) {
		case XUSER_DATA_TYPE_CONTEXT: return FormNewString(L"%u", (uint32_t)value.nData);
		case XUSER_DATA_TYPE_INT32: return FormNewString(L"%d", value.nData);
		case XUSER_DATA_TYPE_INT64: return FormNewString(L"%lld", value.i64Data);
		case XUSER_DATA_TYPE_DOUBLE: return FormNewString(L"%f", value.dblData);
		case XUSER_DATA_TYPE_FLOAT: return FormNewString(L"%f", value.fData);
		case XUSER_DATA_TYPE_UNICODE: {
			// Bound the print to the stored byte length so a value that is not null-terminated (a corrupt
			// stats.dat, or a title that reported length without the terminator) cannot over-read.
			if (!value.string.pwszData || value.string.cbData < sizeof(wchar_t)) {
				return FormNewString(L"%s", L"");
			}
			return FormNewString(L"%.*ls", (int)(value.string.cbData / sizeof(wchar_t)), value.string.pwszData);
		}
		case XUSER_DATA_TYPE_BINARY: {
			if (!value.binary.pbData || !value.binary.cbData) {
				return FormNewString(L"%s", L"");
			}
			char* hex = (char*)malloc(value.binary.cbData * 2 + 1);
			if (!hex) {
				return FormNewString(L"%s", L"");
			}
			ByteArrayToHexStr(hex, value.binary.pbData, value.binary.cbData);
			wchar_t* out = FormNewString(L"%hs", hex);
			free(hex);
			return out;
		}
		case XUSER_DATA_TYPE_DATETIME: {
			SYSTEMTIME st, localSt;
			if (!FileTimeToSystemTime(&value.ftData, &st)) {
				return FormNewString(L"%s", L"");
			}
			SystemTimeToTzSpecificLocalTime(NULL, &st, &localSt);
			wchar_t dateBuf[64] = {};
			wchar_t timeBuf[64] = {};
			GetDateFormatW(LOCALE_USER_DEFAULT, 0, &localSt, NULL, dateBuf, ARRAYSIZE(dateBuf));
			GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &localSt, NULL, timeBuf, ARRAYSIZE(timeBuf));
			return FormNewString(L"%s %s", dateBuf, timeBuf);
		}
		default: return FormNewString(L"%s", L"(unset)");
	}
}

// Strict full-string parse of the edit box per the stat's type: decimal by default, "0x" for hex,
// and trailing garbage, overflow or a sign on an unsigned type are all rejected, so a typo cannot
// silently store a mangled value the title would later read back. Fills value on success and
// returns NULL (UNICODE/BINARY allocate; the caller frees via XllnStatsFreeUserData after storing);
// otherwise returns a user-facing description of the expected format.
static const wchar_t* XllnStatsParseValue(uint8_t type, const wchar_t* input, XUSER_DATA* value)
{
	if (type == XUSER_DATA_TYPE_UNICODE) {
		size_t count = wcslen(input) + 1;
		value->string.pwszData = new wchar_t[count];
		wcscpy_s(value->string.pwszData, count, input);
		value->string.cbData = count * sizeof(wchar_t);
		return 0;
	}
	if (type == XUSER_DATA_TYPE_BINARY) {
		size_t hexLength = wcslen(input);
		if (!hexLength || hexLength % 2) {
			return L"BINARY values are pairs of hex digits (e.g. 0A1B2C).";
		}
		size_t byteCount = hexLength / 2;
		uint8_t* bytes = new uint8_t[byteCount];
		if (!HexStrToByteArray(bytes, byteCount, input)) {
			delete[] bytes;
			return L"BINARY values are pairs of hex digits (e.g. 0A1B2C).";
		}
		value->binary.pbData = bytes;
		value->binary.cbData = byteCount;
		return 0;
	}

	const wchar_t* start = input;
	while (*start == L' ' || *start == L'\t') {
		start++;
	}
	const wchar_t* digits = (*start == L'-' || *start == L'+') ? start + 1 : start;
	int base = (digits[0] == L'0' && (digits[1] == L'x' || digits[1] == L'X')) ? 16 : 10;
	wchar_t* end = 0;
	errno = 0;

	switch (type) {
		case XUSER_DATA_TYPE_CONTEXT: {
			if (*start == L'-') {
				return L"CONTEXT values are unsigned 32-bit integers (decimal or 0x hex).";
			}
			unsigned long parsed = wcstoul(start, &end, base);
			if (end == start || errno == ERANGE) {
				return L"CONTEXT values are unsigned 32-bit integers (decimal or 0x hex).";
			}
			value->nData = (LONG)parsed;
			break;
		}
		case XUSER_DATA_TYPE_INT32: {
			long parsed = wcstol(start, &end, base);
			if (end == start || errno == ERANGE) {
				return L"INT32 values are signed 32-bit integers (decimal or 0x hex).";
			}
			value->nData = parsed;
			break;
		}
		case XUSER_DATA_TYPE_INT64: {
			long long parsed = _wcstoi64(start, &end, base);
			if (end == start || errno == ERANGE) {
				return L"INT64 values are signed 64-bit integers (decimal or 0x hex).";
			}
			value->i64Data = parsed;
			break;
		}
		case XUSER_DATA_TYPE_DATETIME: {
			if (*start == L'-') {
				return L"DATETIME values are the raw FILETIME as an unsigned 64-bit integer.";
			}
			unsigned long long parsed = _wcstoui64(start, &end, base);
			if (end == start || errno == ERANGE) {
				return L"DATETIME values are the raw FILETIME as an unsigned 64-bit integer.";
			}
			memcpy(&value->ftData, &parsed, sizeof(parsed));
			break;
		}
		case XUSER_DATA_TYPE_DOUBLE: {
			double parsed = wcstod(start, &end);
			if (end == start || errno == ERANGE) {
				return L"DOUBLE values are decimal numbers (e.g. 1.5).";
			}
			value->dblData = parsed;
			break;
		}
		case XUSER_DATA_TYPE_FLOAT: {
			double parsed = wcstod(start, &end);
			if (end == start || errno == ERANGE || parsed > FLT_MAX || parsed < -FLT_MAX) {
				return L"FLOAT values are decimal numbers within 32-bit float range.";
			}
			value->fData = (float)parsed;
			break;
		}
		default: {
			return L"This stat's type is not editable.";
		}
	}

	while (*end == L' ' || *end == L'\t') {
		end++;
	}
	if (*end) {
		return L"The value has trailing characters that are not part of a number.";
	}
	return 0;
}

// Parses the edit box against the selected row's type, stores it (an empty box clears the stat back
// to unset), then persists and refreshes. Runs on the window thread.
static void XllnStatsHandleSetValue()
{
	int selectedRow = ListView_GetNextItem(hwndListView, -1, LVNI_SELECTED);
	if (selectedRow < 0 || (size_t)selectedRow >= xlln_stats_rows.size()) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s no stat row selected.", __func__);
		return;
	}
	XLLN_STATS_ROW_REF row = xlln_stats_rows[selectedRow];

	uint32_t index = xlln_stats_rows_user_index;
	if (index >= XLIVE_LOCAL_USER_COUNT) {
		return;
	}
	XUID xuid = xlive_local_users[index].xuid;

	HWND edit = GetDlgItem(xlln_hwnd_stats, XLLNControlsMessageNumbers::STATS_TBX_VALUE);
	int inputLength = GetWindowTextLengthW(edit);
	wchar_t* input = new wchar_t[(size_t)inputLength + 1];
	GetWindowTextW(edit, input, inputLength + 1);

	XUSER_DATA value = {};
	value.type = row.type;
	const wchar_t* parseError = 0;

	if (inputLength == 0) {
		// A blank box clears the stat; the store treats a NULL value as "remove this cell".
		value.type = XUSER_DATA_TYPE_NULL;
	}
	else {
		parseError = XllnStatsParseValue(row.type, input, &value);
	}

	if (!parseError) {
		XllnStatsStoreValue(xuid, row.viewId, row.columnId, value);
		XllnStatsFreeUserData(value);
		// The store keeps the edit either way, but only a user signed in to LIVE has a profile directory
		// to persist into. Saying so beats a silent memory-only edit that disappears at exit.
		uint32_t errorSave = XllnStatsSaveUserToDisk(index);
		XllnWndStatsInvalidateStats();
		if (errorSave != ERROR_SUCCESS) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s set view 0x%08x column %u for user %u but the save failed (0x%08x).", __func__, row.viewId, row.columnId, index, errorSave);
			MessageBoxW(xlln_hwnd_stats, errorSave == ERROR_INVALID_OPERATION
				? L"The value was set but not saved: the user is not signed in to LIVE, so there is no profile to write to. It will be lost when the title exits."
				: L"The value was set but could not be saved to disk. It will be lost when the title exits."
				, L"XLLN Stats Save Error", MB_OK);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG, "%s set view 0x%08x column %u type %hhu for user %u.", __func__, row.viewId, row.columnId, row.type, index);
		}
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s could not parse \"%ls\" as type %hhu.", __func__, input, row.type);
		MessageBoxW(xlln_hwnd_stats, parseError, L"XLLN Stats Set Value Error", MB_OK);
	}

	delete[] input;
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);

			HWND statsList = GetDlgItem(xlln_hwnd_stats, XLLNControlsMessageNumbers::STATS_LST_STATS);
			MoveWindow(statsList, 10, 45, width - 20, height - 55, TRUE);

			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(xlln_hwnd_stats, &ps);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);

			HBRUSH hBrush = CreateSolidBrush(0x00C8C8C8);
			// DeleteObject refuses an object still selected into a DC, so restore the previous brush first.
			HGDIOBJ previousBrush = SelectObject(hdc, hBrush);
			RECT bgRect;
			GetClientRect(hWnd, &bgRect);
			HRGN bgRgn = CreateRectRgnIndirect(&bgRect);
			FillRgn(hdc, bgRgn, hBrush);
			DeleteObject(bgRgn);
			SelectObject(hdc, previousBrush);
			DeleteObject(hBrush);

			// A repaint rebuilds every row and drops the selection, and a stats write from the title
			// thread repaints mid-edit. Restore by stat identity, not row index, which the filter moves.
			XLLN_STATS_ROW_REF previousSelection = {};
			bool hadSelection = false;
			{
				int previousRow = ListView_GetNextItem(hwndListView, -1, LVNI_SELECTED);
				if (previousRow >= 0 && (size_t)previousRow < xlln_stats_rows.size()) {
					previousSelection = xlln_stats_rows[previousRow];
					hadSelection = true;
				}
			}

			// Suspend the list's redraw across the bulk rebuild so it paints once at the end rather than
			// after every insert/set-text - otherwise showing the full (unfiltered) schema visibly lags.
			SendMessageW(hwndListView, WM_SETREDRAW, FALSE, 0);
			ListView_DeleteAllItems(hwndListView);
			xlln_stats_rows.clear();

			uint32_t index = xlln_stats_user_index;
			xlln_stats_rows_user_index = index;
			if (index < XLIVE_LOCAL_USER_COUNT) {
				XUID xuid = xlive_local_users[index].xuid;
				int k = 0;
				bool onlyPopulated = IsDlgButtonChecked(xlln_hwnd_stats, XLLNControlsMessageNumbers::STATS_CHK_ONLY_SET) == BST_CHECKED;
				for (auto &itrView : xlive_stat_view_defs) {
					uint32_t viewId = itrView.first;
					const XLLN_STAT_VIEW_DEF &viewDef = itrView.second;
					for (auto &column : viewDef.columns) {
						// Platform system columns (Rank, Gamer Name, ...) are not title stats and collide
						// with a title column of the same low id; the game fills them in itself.
						if (column.isSystem) {
							continue;
						}
						// Resolve the stored value first so the "only populated" filter can drop unset rows before
						// they are added to the list.
						wchar_t* valueText = 0;
						EnterCriticalSection(&xlive_critsec_xuser_stats);
						auto itrUser = xlive_written_stats.find(xuid);
						if (itrUser != xlive_written_stats.end()) {
							auto itrStatView = itrUser->second.find(viewId);
							if (itrStatView != itrUser->second.end()) {
								auto itrColumn = itrStatView->second.find(column.columnId);
								if (itrColumn != itrStatView->second.end()) {
									valueText = XllnStatsFormatValue(itrColumn->second);
								}
							}
						}
						LeaveCriticalSection(&xlive_critsec_xuser_stats);

						if (onlyPopulated && !valueText) {
							continue;
						}

						CreateItem(hwndListView, k);

						wchar_t* text;
						text = FormNewString(L"0x%08X %s", viewId, viewDef.name.c_str());
						ListView_SetItemText(hwndListView, k, 0, text);
						delete[] text;
						text = FormNewString(L"%u", column.columnId);
						ListView_SetItemText(hwndListView, k, 1, text);
						delete[] text;
						ListView_SetItemText(hwndListView, k, 2, (wchar_t*)column.name.c_str());
						ListView_SetItemText(hwndListView, k, 3, (wchar_t*)XllnStatsTypeName(column.type));
						if (!valueText) {
							valueText = FormNewString(L"%s", L"(unset)");
						}
						ListView_SetItemText(hwndListView, k, 4, valueText ? valueText : (wchar_t*)L"");
						if (valueText) {
							delete[] valueText;
						}

						xlln_stats_rows.push_back(XLLN_STATS_ROW_REF { viewId, column.columnId, column.type });
						k++;
					}
				}

				// Titles can store stats the SPA never declared: XSessionWriteStats does not consult the
				// schema, and stats.dat may hold records from a different game version. List those under
				// their raw ids (typed from the stored value) so stored data is never invisible here.
				struct XLLN_STATS_EXTRA_ROW {
					uint32_t viewId;
					uint32_t columnId;
					uint8_t type;
					std::wstring viewName;
					wchar_t* valueText;
				};
				std::vector<XLLN_STATS_EXTRA_ROW> extraRows;
				EnterCriticalSection(&xlive_critsec_xuser_stats);
				auto itrUser = xlive_written_stats.find(xuid);
				if (itrUser != xlive_written_stats.end()) {
					for (auto &itrView : itrUser->second) {
						auto itrViewDef = xlive_stat_view_defs.find(itrView.first);
						for (auto &itrColumn : itrView.second) {
							bool inSchema = false;
							if (itrViewDef != xlive_stat_view_defs.end()) {
								for (auto &column : itrViewDef->second.columns) {
									if (!column.isSystem && column.columnId == itrColumn.first) {
										inSchema = true;
										break;
									}
								}
							}
							if (inSchema) {
								continue;
							}
							extraRows.push_back(XLLN_STATS_EXTRA_ROW {
								itrView.first,
								itrColumn.first,
								itrColumn.second.type,
								itrViewDef != xlive_stat_view_defs.end() ? itrViewDef->second.name : std::wstring(),
								XllnStatsFormatValue(itrColumn.second)
							});
						}
					}
				}
				LeaveCriticalSection(&xlive_critsec_xuser_stats);
				for (auto &extra : extraRows) {
					CreateItem(hwndListView, k);
					wchar_t* text = extra.viewName.empty()
						? FormNewString(L"0x%08X", extra.viewId)
						: FormNewString(L"0x%08X %s", extra.viewId, extra.viewName.c_str());
					ListView_SetItemText(hwndListView, k, 0, text);
					delete[] text;
					text = FormNewString(L"%u", extra.columnId);
					ListView_SetItemText(hwndListView, k, 1, text);
					delete[] text;
					ListView_SetItemText(hwndListView, k, 2, (wchar_t*)L"(not in SPA)");
					ListView_SetItemText(hwndListView, k, 3, (wchar_t*)XllnStatsTypeName(extra.type));
					ListView_SetItemText(hwndListView, k, 4, extra.valueText ? extra.valueText : (wchar_t*)L"");
					if (extra.valueText) {
						delete[] extra.valueText;
					}
					xlln_stats_rows.push_back(XLLN_STATS_ROW_REF { extra.viewId, extra.columnId, extra.type });
					k++;
				}
			}

			if (hadSelection) {
				for (size_t iRow = 0; iRow < xlln_stats_rows.size(); iRow++) {
					const XLLN_STATS_ROW_REF &row = xlln_stats_rows[iRow];
					if (row.viewId == previousSelection.viewId && row.columnId == previousSelection.columnId) {
						ListView_SetItemState(hwndListView, (int)iRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
						break;
					}
				}
			}

			SendMessageW(hwndListView, WM_SETREDRAW, TRUE, 0);
			InvalidateRect(hwndListView, NULL, TRUE);

			EndPaint(xlln_hwnd_stats, &ps);

			break;
		}
		case XLLNControlsMessageNumbers::EVENT_STATS_USERS_UPDATE: {
			HWND controlComboBoxUsers = GetDlgItem(xlln_hwnd_stats, XLLNControlsMessageNumbers::STATS_CMB_USERS);

			EnterCriticalSection(&xlln_critsec_stats);

			while (SendMessageA(controlComboBoxUsers, CB_DELETESTRING, 0, 0) > 0);

			for (auto &itrLocalUser : xlive_local_users) {
				char* itemLabel = CloneString(itrLocalUser.username);
				SendMessageA(controlComboBoxUsers, CB_ADDSTRING, 0, (LPARAM)itemLabel);
				delete[] itemLabel;
				itemLabel = 0;
			}

			SendMessageA(controlComboBoxUsers, CB_SETCURSEL, (WPARAM)xlln_stats_user_index, 0);

			XllnWndStatsInvalidateStats();

			LeaveCriticalSection(&xlln_critsec_stats);

			break;
		}
		case WM_COMMAND: {
			switch (wParam) {
				case ((CBN_SELCHANGE << 16) | XLLNControlsMessageNumbers::STATS_CMB_USERS): {
					HWND controlComboBoxUsers = (HWND)lParam;
					LRESULT resultItemIndex = SendMessageA(controlComboBoxUsers, CB_GETCURSEL, 0, 0);
					if (resultItemIndex != CB_ERR) {
						EnterCriticalSection(&xlln_critsec_stats);

						xlln_stats_user_index = (uint32_t)resultItemIndex;

						PostMessageW(xlln_hwnd_stats, XLLNControlsMessageNumbers::EVENT_STATS_USERS_UPDATE, 0, 0);

						LeaveCriticalSection(&xlln_critsec_stats);
					}

					break;
				}
				case XLLNControlsMessageNumbers::STATS_BTN_SET_VALUE: {
					XllnStatsHandleSetValue();
					break;
				}
				case XLLNControlsMessageNumbers::STATS_CHK_ONLY_SET: {
					// BS_AUTOCHECKBOX has already toggled the state; just repopulate under the new filter.
					XllnWndStatsInvalidateStats();
					break;
				}
			}

			return 0;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				XllnShowWindow(XllnShowType::STATS_HIDE);
				return 0;
			}
			break;
		}
		case WM_CTLCOLORSTATIC: {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			// One brush for the window's lifetime; creating one per message leaks a GDI handle each paint.
			static HBRUSH controlBackgroundBrush = CreateSolidBrush(0x00C8C8C8);
			return (INT_PTR)controlBackgroundBrush;
		}
		case WM_CREATE: {
			CreateWindowA(WC_COMBOBOXA, 0, WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_TABSTOP,
				10, 10, 200, 300, hWnd, (HMENU)XLLNControlsMessageNumbers::STATS_CMB_USERS, xlln_hModule, 0);

			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
				220, 10, 200, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::STATS_TBX_VALUE, xlln_hModule, 0);

			CreateWindowA(WC_BUTTONA, "Set Value", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				430, 10, 90, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::STATS_BTN_SET_VALUE, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "Only populated", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
				535, 12, 150, 22, hWnd, (HMENU)XLLNControlsMessageNumbers::STATS_CHK_ONLY_SET, xlln_hModule, NULL);
			CheckDlgButton(hWnd, XLLNControlsMessageNumbers::STATS_CHK_ONLY_SET, BST_CHECKED);

			hwndListView = CreateWindowA(WC_LISTVIEWA, 0,
				WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT | LVS_NOSORTHEADER | WS_TABSTOP,
				10, 45, 940, 545,
				hWnd, (HMENU)XLLNControlsMessageNumbers::STATS_LST_STATS, xlln_hModule, 0);

			ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_FULLROWSELECT);

			uint32_t j = 0;
			CreateColumn(hwndListView, j++, L"View", 160);
			CreateColumn(hwndListView, j++, L"Column", 70);
			CreateColumn(hwndListView, j++, L"Name", 200);
			CreateColumn(hwndListView, j++, L"Type", 90);
			CreateColumn(hwndListView, j++, L"Value", 340);

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

static DWORD WINAPI XllnThreadWndStats(void* lpParam)
{
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);

	const wchar_t* windowclassname = L"XLLNDLLWindowStatsClass";
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
		wchar_t* windowTitle = FormNewString(L"XLLN Stats #%d v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);

		HWND hwdParent = NULL;
		xlln_hwnd_stats = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 600, hwdParent, 0, hModule, NULL);

		delete[] windowTitle;
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

	PostMessageW(xlln_hwnd_stats, XLLNControlsMessageNumbers::EVENT_STATS_USERS_UPDATE, 0, 0);

	ShowWindow(xlln_hwnd_stats, SW_HIDE);

	SetEvent(xlln_window_initialised_event);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		// Handle tab ordering
		if (!IsDialogMessage(xlln_hwnd_stats, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	SetEvent(xlln_window_destroy_event);

	return ERROR_SUCCESS;
}

uint32_t InitXllnWndStats()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);

	CreateThread(0, NULL, XllnThreadWndStats, (void*)xlln_hModule, NULL, NULL);

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

uint32_t UninitXllnWndStats()
{
	SendMessage(xlln_hwnd_stats, WM_CLOSE, (WPARAM)0, (LPARAM)0);

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
