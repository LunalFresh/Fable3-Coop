#include "../../../dllmain.hpp"
#include "../../xlln-module.hpp"
#include "account-helper.hpp"
#include "../../config/accounts.hpp"
#include "../../xlive.hpp"
#include "../../../xlivelessness.hpp"
#include <stdio.h>

static int accountingActiveHandleCount = 0;

bool isAccountingActiveHandle()
{
	bool result = accountingActiveHandleCount > 0;
	return result;
}

int getAccountingActiveHandle()
{
	return accountingActiveHandleCount;
}

void closeAccountingActiveHandle()
{
	if (accountingActiveHandleCount > 0) {
		accountingActiveHandleCount = 0;
		XLLNDebugLog(0, "PCarto: Accounting Close.");
		SaveH2Accounts();
	}
}

void updateAccountingActiveHandle(bool active)
{
	accountingActiveHandleCount += active ? 1 : -1;
	char handleCountString[40];
	snprintf(handleCountString, 40, "Accounting Active: %d", accountingActiveHandleCount);
	XLLNDebugLog(0, handleCountString);
	if (accountingActiveHandleCount <= 0) {
		SaveH2Accounts();
	}
}
