#pragma once
#include <map>
#include <set>

typedef struct _CONTENT_OFFER_REGIONS_DETAILS {
	uint32_t unkn1 = 0;
	uint32_t unkn2 = 0;
	uint32_t unkn3 = 0;
	// Probably an enum: "TaxNotApplicable", "TaxNotIncluded".
	uint8_t tax = 0;
} CONTENT_OFFER_REGIONS_DETAILS;

typedef struct _CONTENT_DETAILS {
	wchar_t* xcontentPath = 0;
	bool premium = false;
	uint32_t titleId = 0;
	// XCONTENTTYPE_
	uint32_t contentPackageType = 0;
	uint32_t licenseBits = 0;
	uint32_t baseVersion = 0;
	uint32_t updateVersion = 0;
	wchar_t* thumbnailImage = 0;
	uint64_t offeringId = 0;
	wchar_t* adminFriendlyName = 0;
	wchar_t* titleName = 0;
	bool purchaseOnceOnly = false;
	uint8_t contentFlags = 0;
	uint8_t contentId[XLIVE_CONTENT_ID_SIZE];
	uint8_t platform = 0;
	uint32_t bitFlags = 0;
	tm activationDate;
	wchar_t* defaultDisplayName = 0;
	// OfferRegions.<KEY>
	std::map<uint8_t, CONTENT_OFFER_REGIONS_DETAILS> offerRegions;
	XMARKETPLACE_OFFERING_TYPE offeringType = XMARKETPLACE_OFFERING_TYPE::XMARKETPLACE_OFFERING_TYPE_CONTENT;
	tm endDate;
	bool displayInDash = false;
	uint32_t version = 0;
	// DashDisplayName.<KEY>
	std::map<uint8_t, wchar_t*> dashDisplayName;
	// DashDescription.<KEY>
	std::map<uint8_t, wchar_t*> dashDescription;
	
	_CONTENT_DETAILS()
	{
		memset(this->contentId, 0, sizeof(this->contentId));
		memset(&this->activationDate, 0, sizeof(this->activationDate));
		memset(&this->endDate, 0, sizeof(this->endDate));
	}
	
	~_CONTENT_DETAILS()
	{
		if (this->xcontentPath) {
			delete[] this->xcontentPath;
			this->xcontentPath = 0;
		}
		if (this->thumbnailImage) {
			delete[] this->thumbnailImage;
			this->thumbnailImage = 0;
		}
		if (this->adminFriendlyName) {
			delete[] this->adminFriendlyName;
			this->adminFriendlyName = 0;
		}
		if (this->titleName) {
			delete[] this->titleName;
			this->titleName = 0;
		}
		if (this->defaultDisplayName) {
			delete[] this->defaultDisplayName;
			this->defaultDisplayName = 0;
		}
	}
} CONTENT_DETAILS;

typedef struct _XCONTENT_ENUMERATION_DETAILS {
	HANDLE enumeratorHandle = 0;
	uint32_t titleId = 0;
	// Key: offeringId.
	std::map<uint64_t, CONTENT_DETAILS*> contentDetails;
} XCONTENT_ENUMERATION_DETAILS;

extern CRITICAL_SECTION xlive_critsec_xcontent;
extern std::map<HANDLE, XCONTENT_ENUMERATION_DETAILS*> xlive_xcontent_enumerators;
extern std::map<HANDLE, void*> xlive_xcontent_access_handles;
