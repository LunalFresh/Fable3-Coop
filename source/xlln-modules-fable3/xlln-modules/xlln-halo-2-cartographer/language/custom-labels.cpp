#include "../../dllmain.hpp"
#include "custom-labels.hpp"
#include "custom-language.hpp"

const int CMLabelMenuId_Error =									0xFF000001;
const int CMLabelMenuId_Language =								0xFF000002;
const int CMLabelMenuId_Guide =									0xFF000003;
const int CMLabelMenuId_EscSettings =							0xFF000004;
const int CMLabelMenuId_AdvSettings =							0xFF000005;
const int CMLabelMenuId_Credits =								0xFF000006;
const int CMLabelMenuId_EditHudGui =							0xFF000007;
const int CMLabelMenuId_ToggleSkulls =							0xFF000008;
const int CMLabelMenuId_AccountList =							0xFF000009;
const int CMLabelMenuId_AccountAdd =							0xFF00000A;
const int CMLabelMenuId_VKeyTest =								0xFF00000B;
const int CMLabelMenuId_AccountCreate =							0xFF00000C;
const int CMLabelMenuId_AdvOtherSettings =						0xFF00000D;
const int CMLabelMenuId_EditFPS =								0xFF00000E;
const int CMLabelMenuId_EditFOV =								0xFF00000F;
const int CMLabelMenuId_EditFOVVehicle =						0xFF000010;
const int CMLabelMenuId_EditCrosshairOffset =					0xFF000011;
const int CMLabelMenuId_EditCrosshairSize =						0xFF000012;
const int CMLabelMenuId_Update =								0xFF000013;
const int CMLabelMenuId_Update_Note =							0xFF000014;
const int CMLabelMenuId_Login_Warn =							0xFF000015;
const int CMLabelMenuId_EditStaticLoD =							0xFF000016;
const int CMLabelMenuId_AdvLobbySettings =						0xFF000017;
const int CMLabelMenuId_Invalid_Login_Token =					0xFF000018;
const int CMLabelMenuId_AccountManagement =						0xFF000019;
//const int CMLabelMenuId_NetworkType =							0xFF00001A;
const int CMLabelMenuId_AdvControlsSettings =					0xFF00001B;
const int CMLabelMenuId_EditSensitivityNativeController =		0xFF00001C;
const int CMLabelMenuId_EditSensitivityNativeMouse =			0xFF00001D;
const int CMLabelMenuId_EditSensitivityRawMouse =				0xFF00001E;
const int CMLabelMenuId_EditSensitivityXYAxisRatioController =	0xFF00001F;
const int CMLabelMenuId_EditSensitivityXYAxisRatioMouse =		0xFF000020;
const int CMLabelMenuId_EditSensitivityVehicleRatioController =	0xFF000021;
const int CMLabelMenuId_EditSensitivityVehicleRatioMouse =		0xFF000022;
const int CMLabelMenuId_ChangeBsp	 =							0xFF000023;
const int CMLabelMenuId_ChangeDifficulty =						0xFF000024;
const int CMLabelMenuId_CoopMode =								0xFF000025;
const int CMLabelMenuId_EditFpsLimit =							0xFF000026;

int CM_Number_Of_Buttons_Credits = 0;

bool InitCustomLabels()
{
	add_cartographer_label(CMLabelMenuId_EscSettings, 0xFFFFFFF0, "Esc Settings");
	add_cartographer_label(CMLabelMenuId_EscSettings, 0xFFFFFFF1, "Esc Settings Here.");
	add_cartographer_label(CMLabelMenuId_EscSettings, 1, "btn 1");
	add_cartographer_label(CMLabelMenuId_EscSettings, 2, "btn 2");
	
	
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFFFF0, "Error!", true);
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFFFF1, "Generic Error.", true);
	add_cartographer_label(CMLabelMenuId_Error, 0x1, "Error");
	add_cartographer_label(CMLabelMenuId_Error, 0x2, "That was an unconfigured action. Oops.");
	add_cartographer_label(CMLabelMenuId_Error, 0x4, "None");
	add_cartographer_label(CMLabelMenuId_Error, 0x5, "There are no custom languages catergorised as Other.");
	add_cartographer_label(CMLabelMenuId_Error, 0x6, "Error");
	add_cartographer_label(CMLabelMenuId_Error, 0x7, "An error occured when trying to read the custom language file.\r\nNo Changes have been made.\r\nReview the on screen debug log for more details.");
	add_cartographer_label(CMLabelMenuId_Error, 0x8, "Incomplete Feature");
	add_cartographer_label(CMLabelMenuId_Error, 0x9, "This feature is incomplete.");
	add_cartographer_label(CMLabelMenuId_Error, 0xA, "ERROR");
	add_cartographer_label(CMLabelMenuId_Error, 0xB, "NULL Pointer");
	add_cartographer_label(CMLabelMenuId_Error, 0xC, "XLLN API Error");
	add_cartographer_label(CMLabelMenuId_Error, 0xD, "An error occurred when calling an XLiveLessNess API.");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFFF02, "Glitchy Scripts");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFFF03, "");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF016, "Accounts in Use");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF017, "Another instance of Halo 2 / H2Server is currently signing in, please try again after it finishes.");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF01A, "Invalid Email!");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF01B, "The Email address you have entered is invalid! Please double check your spelling.");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF01C, "Invalid Username!");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF01D, "The Username you have entered is invalid! Please ensure you have formed it correctly using only allowed symbols!");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF01E, "Invalid Password!");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF01F, "The Password you have entered is invalid! Please ensure you have formed it correctly using only allowed symbols!");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF02A, "Restart Required");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF02B, "The setting you have just changed requires that you restart your game for it to take effect.");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF02C, "Creating Account...");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF02D, "Processing your new account...\r\nPlease wait.");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF02E, "Logging in...");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF02F, "Please wait while you are being logged in.");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF030, "Connection Failed!");
	add_cartographer_label(CMLabelMenuId_Error, 0xFFFFF031, "Please check your connection then try again.");
	
	
	add_cartographer_label(CMLabelMenuId_Language, 0xFFFFFFF0, "Select Language");
	add_cartographer_label(CMLabelMenuId_Language, 0xFFFFFFF1, "Select the language you would like to play the game in.");
	add_cartographer_label(CMLabelMenuId_Language, 1, "System Default");
	add_cartographer_label(CMLabelMenuId_Language, 2, "Other >");
	add_cartographer_label(CMLabelMenuId_Language, 3, "English >");
	add_cartographer_label(CMLabelMenuId_Language, 4, "Japanese >");
	add_cartographer_label(CMLabelMenuId_Language, 5, "German >");
	add_cartographer_label(CMLabelMenuId_Language, 6, "French >");
	add_cartographer_label(CMLabelMenuId_Language, 7, "Spanish >");
	add_cartographer_label(CMLabelMenuId_Language, 8, "Italian >");
	add_cartographer_label(CMLabelMenuId_Language, 9, "Korean >");
	add_cartographer_label(CMLabelMenuId_Language, 10, "Chinese >");
	add_cartographer_label(CMLabelMenuId_Language, 11, "Reload Language File");
	add_cartographer_label(CMLabelMenuId_Language, 12, "Save Language File");
	add_cartographer_label(CMLabelMenuId_Language, 0xFFFFF002, "\xE2\x9C\x93 Capture New Labels");
	add_cartographer_label(CMLabelMenuId_Language, 0xFFFFF003, "\xE2\x9C\x97 Capture New Labels");
	add_cartographer_label(CMLabelMenuId_Language, 0xFFFFFFF2, "--- Base %s Variant ---");
	add_cartographer_label(CMLabelMenuId_Language, 0xFFFFFFF3, "Select Language Variant");
	add_cartographer_label(CMLabelMenuId_Language, 0xFFFFFFF4, "Select the variant of the language you would like to play the game in.");
	
	
	add_cartographer_label(CMLabelMenuId_EditFPS, 0xFFFFFFF0, "Edit FPS Limit");
	add_cartographer_label(CMLabelMenuId_EditFPS, 0xFFFFFFF1, "Use the buttons below to modify the FPS limit of Halo 2.");
	add_cartographer_label(CMLabelMenuId_EditFPS, 1, "+10");
	add_cartographer_label(CMLabelMenuId_EditFPS, 2, "+1");
	add_cartographer_label(CMLabelMenuId_EditFPS, 0xFFFF0003, "FPS Limit: %d");
	add_cartographer_label(CMLabelMenuId_EditFPS, 0xFFFF0013, "Xlive FPS Limiter Disabled");
	add_cartographer_label(CMLabelMenuId_EditFPS, 4, "-1");
	add_cartographer_label(CMLabelMenuId_EditFPS, 5, "-10");
	
	
	add_cartographer_label(CMLabelMenuId_EditFOV, 0xFFFFFFF0, "Edit Field of View");
	add_cartographer_label(CMLabelMenuId_EditFOV, 0xFFFFFFF1, "Use the buttons below to modify the in-game Field of View (FoV).");
	add_cartographer_label(CMLabelMenuId_EditFOV, 1, "+10");
	add_cartographer_label(CMLabelMenuId_EditFOV, 2, "+1");
	add_cartographer_label(CMLabelMenuId_EditFOV, 0xFFFF0003, "FoV: %d");
	add_cartographer_label(CMLabelMenuId_EditFOV, 0xFFFF0013, "FoV is Default (70)");
	add_cartographer_label(CMLabelMenuId_EditFOV, 4, "-1");
	add_cartographer_label(CMLabelMenuId_EditFOV, 5, "-10");
	
	
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 0xFFFFFFF0, "Edit Vehicle Field of View");
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 0xFFFFFFF1, "Use the buttons below to modify the in-game Field of View (FoV) for vehicles.");
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 1, "+10");
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 2, "+1");
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 0xFFFF0003, "Vehicle FoV: %d");
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 0xFFFF0013, "Vehicle FoV is Default (70)");
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 4, "-1");
	add_cartographer_label(CMLabelMenuId_EditFOVVehicle, 5, "-10");
	
	
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 0xFFFFFFF0, "Edit Crosshair Offset");
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 0xFFFFFFF1, "Use the buttons below to modify the in-game Crosshair Offset.");
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 1, "+0.02");
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 2, "+0.001");
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 0xFFFF0003, "Offset: %f");
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 0xFFFF0013, "Offset Alteration Disabled");
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 4, "-0.001");
	add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 5, "-0.02");
	
	
	add_cartographer_label(CMLabelMenuId_EditCrosshairSize, 0xFFFFFFF0, "Crosshair Settings");
	add_cartographer_label(CMLabelMenuId_EditCrosshairSize, 0xFFFFFFF1, "Use the buttons below to set a preset crosshair size. Use the config file to modify crosshairs in more detail.");
	add_cartographer_label(CMLabelMenuId_EditCrosshairSize, 1, "Default");
	add_cartographer_label(CMLabelMenuId_EditCrosshairSize, 2, "Disabled");
	add_cartographer_label(CMLabelMenuId_EditCrosshairSize, 3, "Very Small");
	add_cartographer_label(CMLabelMenuId_EditCrosshairSize, 4, "Small");
	add_cartographer_label(CMLabelMenuId_EditCrosshairSize, 5, "Large");
	
	
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 0xFFFFFFF0, "Static Model Level of Detail");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 0xFFFFFFF1, "Use the buttons below to set a static level on a model's Level of Detail.");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 1, "Disabled");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 2, "L1 - Very Low");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 3, "L2 - Low");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 4, "L3 - Medium");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 5, "L4 - High");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 6, "L5 - Very High");
	add_cartographer_label(CMLabelMenuId_EditStaticLoD, 7, "L6 - Cinematic");
	
	
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFFF0, "Update");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFFF1, "Update Project Cartographer.");
	add_cartographer_label(CMLabelMenuId_Update, 1, (char*)0, true);
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFF0001, "Check for Updates");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFF01, "Checking For Updates...");
	add_cartographer_label(CMLabelMenuId_Update, 2, (char*)0, true);
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFF0002, "Download Updates");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFF02, "Downloading Updates...");
	add_cartographer_label(CMLabelMenuId_Update, 3, (char*)0, true);
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFF0003, "Install Updates");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFF03, "Installing Updates...");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFF003, "Failed to run updater app!");
	add_cartographer_label(CMLabelMenuId_Update, 4, "Cancel");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFFF2, "Download the following:\n");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFFF3, "Install the following:\n");
	add_cartographer_label(CMLabelMenuId_Update, 0xFFFFFFF4, "Up to date!");
	
	
	add_cartographer_label(CMLabelMenuId_Update_Note, 0xFFFFFFF0, "Outdated Version!");
	add_cartographer_label(CMLabelMenuId_Update_Note, 0xFFFFFFF1, "You are using an outdated version of Project Cartographer! Would you like to go install the latest version?");
	add_cartographer_label(CMLabelMenuId_Update_Note, 1, "Yes");
	add_cartographer_label(CMLabelMenuId_Update_Note, 2, "No");
	
	
	add_cartographer_label(CMLabelMenuId_Login_Warn, 0xFFFFFFF0, "NO CHEATING!");
	add_cartographer_label(CMLabelMenuId_Login_Warn, 0xFFFFFFF1, "DO NOT CHEAT/HACK ONLINE.\r\nWe have a complex system made to keep you banned if you do. Don't risk it. We will catch you otherwise!");
	
	
	add_cartographer_label(CMLabelMenuId_EditHudGui, 0xFFFFFFF0, "Customise HUD / GUI");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 0xFFFFFFF1, "Customise your heads up display and user interface with the following settings.");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 0xFFFFFFF2, "\xE2\x9C\x93 %s");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 0xFFFFFFF3, "\xE2\x9C\x97 %s");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 1, "> Field of View (FoV)");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 2, "> Vehicle FoV");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 3, "> Crosshair Offset");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 4, "> Crosshair Size");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 0xFFFF0005, "Hide Ingame Chat");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 0xFFFF0006, "Disable HUD");
	add_cartographer_label(CMLabelMenuId_EditHudGui, 0xFFFF0007, "Disable First Person Model");
	
	
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFFFFF0, "Toggle Skulls");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFFFFF1, "Enable and disable Halo 2's Skulls.\r\nWhat ice cream flavor would you like to choose today?");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFFFFF2, "\xE2\x9C\x97 %s");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFFFFF3, "\xE2\x9C\x93 %s");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFFFFF4, "???");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0000, "Anger");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0001, "Assassins");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0002, "Black Eye");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0003, "Blind");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0004, "Catch");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0005, "Envy");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0006, "Famine");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0007, "Ghost");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0008, "Grunt Birthday Party");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF0009, "Iron");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF000A, "IWHBYD");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF000B, "Mythic");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF000C, "Sputnik");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF000D, "Thunderstorm");
	add_cartographer_label(CMLabelMenuId_ToggleSkulls, 0xFFFF000E, "Whuppopotamus");
	
	
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 0xFFFFFFF0, "Other Settings");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 0xFFFFFFF1, "Customise other settings and features of Halo 2 / Project Cartographer.");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 0xFFFFFFF2, "\xE2\x9C\x93 %s");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 0xFFFFFFF3, "\xE2\x9C\x97 %s");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 1, "> FPS Limit");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 2, "> Static Model LoD");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 0xFFFF0003, "Warp Fix");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 0xFFFF0004, "Game Intro Video");
	add_cartographer_label(CMLabelMenuId_AdvOtherSettings, 0xFFFF0005, "Hitmarker Sound Effect");
	
	
	add_cartographer_label(CMLabelMenuId_AdvSettings, 0xFFFFFFF0, "Advanced Settings");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 0xFFFFFFF1, "Alter additional settings for the game.");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 1, "Change Language");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 2, "Customise HUD/GUI");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 3, "Customise Controls");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 4, "Other Settings");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 5, "Toggle Skulls");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 6, "Change BSP");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 7, "Change Difficulty");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 8, "Coop Mode Fix");
	add_cartographer_label(CMLabelMenuId_AdvSettings, 9, "Extra Lobby Host Settings");
	
	
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFFFFF0, "Extra Lobby Host Settings");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFFFFF1, "Customise your lobby with some extra features / hacks.");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFFFFF2, "\xE2\x9C\x93 %s");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFFFFF3, "\xE2\x9C\x97 %s");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFFFF02, "Change LAN Lobby Name");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFFFF03, "Enter the new name for your LAN lobby.");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFFF001, 32 * sizeof(wchar_t), true);
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 1, "> Change LAN Lobby Name");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0002, "xDelay");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0003, "Vehicle Flip Eject");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0004, "Kill Volumes");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0005, "MP Explosion Physics");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0006, "MP Sputnik");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0007, "MP Grunt B-Day Party");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0008, "Grenade Chain React");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF0009, "Banshee Bomb");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF000A, "HUD");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF000B, "First Person Model");
	add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF000C, "Flashlight");
	//add_cartographer_label(CMLabelMenuId_AdvLobbySettings, 0xFFFF000D, "Zombie Movement Speed");
	
	
	add_cartographer_label(CMLabelMenuId_Credits, 0xFFFFFFF0, "Credits");
	add_cartographer_label(CMLabelMenuId_Credits, 0xFFFFFFF1, "Praise the Following.");
	add_cartographer_label(CMLabelMenuId_Credits, ++CM_Number_Of_Buttons_Credits, "--- The Devs ---");
	add_cartographer_label(CMLabelMenuId_Credits, ++CM_Number_Of_Buttons_Credits, "Glitchy Scripts");
	add_cartographer_label(CMLabelMenuId_Credits, ++CM_Number_Of_Buttons_Credits, "--- Additional 7hanks ---");
	add_cartographer_label(CMLabelMenuId_Credits, ++CM_Number_Of_Buttons_Credits, "The Project Cartographer Team");
	
	
	add_cartographer_label(CMLabelMenuId_Invalid_Login_Token, 0xFFFFFFF0, "Invalid Login Token!");
	add_cartographer_label(CMLabelMenuId_Invalid_Login_Token, 0xFFFFFFF1, "Login Again.");
	
	
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFFFF0, "Create Account");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFFFF1, "Enter a username, email and password for your new account.");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFFFF2, "[Username]");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFFFF3, "[Email]");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFFFF4, "[Password]");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFF002, "Create a Username");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFF003, "Create the Username for your new account below.\r\nHowever beware! Not all special characters are supported!");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFF004, "Enter an Email Address");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFF005, "Enter the Email Address to be linked to your new account below.");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFF006, "Create a Password");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 0xFFFFF007, "Create the Password for your new account below.");
	add_cartographer_label(CMLabelMenuId_AccountCreate, 1, 255 * 2, true);
	add_cartographer_label(CMLabelMenuId_AccountCreate, 2, 255 * 2, true);
	add_cartographer_label(CMLabelMenuId_AccountCreate, 3, 255 * 2, true);
	add_cartographer_label(CMLabelMenuId_AccountCreate, 4, "Create Account");
	
	
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFFFF0, "Add Account");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFFFF1, "Enter your account's Username\r\n[or Email] and Password to Login.");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFFFF2, "-Remember me");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFFFF3, "-Don't remember me");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFFFF4, "[Username]");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFFFF5, "[Password]");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFF002, "Enter Account Username or Email Address");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFF003, "Enter the Username or Email Address of your account below.");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFF004, "Enter Account Password");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 0xFFFFF005, "Enter the Password of your account below.");
	add_cartographer_label(CMLabelMenuId_AccountAdd, 1, 255 * 2, true);
	add_cartographer_label(CMLabelMenuId_AccountAdd, 2, 255 * 2, true);
	add_cartographer_label(CMLabelMenuId_AccountAdd, 4, "Login");
	
	
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFFFFF0, "Online Accounts");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFFFFF1, "Select an Account to Sign in to or use options to create/add/remove them.");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFF0000, ">Add Account");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFF0001, "-Remove Account");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFF0002, "-Cancel Remove");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFF0003, ">Create Account");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFF0004, ">Play Offline");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFF0005, "<Unnamed>");
	add_cartographer_label(CMLabelMenuId_AccountList, 0xFFFF0006, "Guest @ %s");
	
	
	add_cartographer_label(CMLabelMenuId_AccountManagement, 0xFFFFFFF0, "Account Management");
	add_cartographer_label(CMLabelMenuId_AccountManagement, 0xFFFFFFF1, "Select a player to login or logout with.");
	add_cartographer_label(CMLabelMenuId_AccountManagement, 0xFFFF0001, "Logout P%d %hs @ %hs");
	add_cartographer_label(CMLabelMenuId_AccountManagement, 0xFFFF0002, "Login P%d");
	add_cartographer_label(CMLabelMenuId_AccountManagement, 0xFFFF0003, "Online");
	add_cartographer_label(CMLabelMenuId_AccountManagement, 0xFFFF0004, "Offline");
	
	
	add_cartographer_label(CMLabelMenuId_Guide, 0xFFFFFFF0, "Project Cartographer Guide");
	add_cartographer_label(CMLabelMenuId_Guide, 0xFFFFFFF2, "Press the %s Key to open this guide from anywhere.");
	add_cartographer_label(CMLabelMenuId_Guide, 1, "XLiveLessNess");
	add_cartographer_label(CMLabelMenuId_Guide, 2, "Advanced Settings");
	add_cartographer_label(CMLabelMenuId_Guide, 3, "Account Management");
	add_cartographer_label(CMLabelMenuId_Guide, 4, "Website");
	add_cartographer_label(CMLabelMenuId_Guide, 5, "Credits");
	add_cartographer_label(CMLabelMenuId_Guide, 6, "Update");
	
	
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 0xFFFFFFF0, "Advanced Controls");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 0xFFFFFFF1, "Customise game controls at a more advanced level.");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 0xFFFFFFF2, "\xE2\x9C\x93 %s");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 0xFFFFFFF3, "\xE2\x9C\x97 %s");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 1, "> Raw Mouse Sensitivity");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 2, "> Native Mouse Sensitivity");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 3, "> Mouse X-Y Axis Sens Ratio");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 4, "> Mouse Vehicle Sens Ratio");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 5, "> Controller Sensitivity");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 6, "> Controller X-Y Axis Sens Ratio");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 7, "> Controller Vehicle Sens Ratio");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 0xFFFF0008, "Controller Aim-Assist");
	add_cartographer_label(CMLabelMenuId_AdvControlsSettings, 0xFFFF0009, "In-game Keyb. CTRLs");
	
	
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeController, 0xFFFFFFF0, "Edit Native Controller Sensitivity");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeController, 0xFFFFFFF1, "Use the buttons below to modify the in-game native controller sensitivity.");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeController, 1, "+1.0");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeController, 2, "+0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeController, 0xFFFF0003, "Sensitivity: %f");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeController, 4, "-0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeController, 5, "-1.0");
	
	
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeMouse, 0xFFFFFFF0, "Edit Native Mouse Sensitivity");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeMouse, 0xFFFFFFF1, "Use the buttons below to modify the in-game native mouse sensitivity.");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeMouse, 1, "+1.0");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeMouse, 2, "+0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeMouse, 0xFFFF0003, "Sensitivity: %f");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeMouse, 4, "-0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityNativeMouse, 5, "-1.0");
	
	
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 0xFFFFFFF0, "Edit Raw Mouse Sensitivity");
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 0xFFFFFFF1, "Activating this raw mouse input setting overrides the native sensitivity setting.");
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 1, "+50");
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 2, "+1");
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 0xFFFF0003, "Raw Sensitivity: %u");
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 0xFFFF0013, "Raw Sensitivity Disabled");
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 4, "-1");
	add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 5, "-50");
	
	
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 0xFFFFFFF0, "Edit X-Y Controller Sensitivity Ratio");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 0xFFFFFFF1, "Use the buttons below to modify the in-game X-Y axis controller sensitivity ratio.");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 1, "X+10, Y-10");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 2, "X+1, Y-1");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 0xFFFF0003, "Y Axis: %u%%");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 0xFFFF0013, "X Axis: %u%%");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 0xFFFF0023, "X & Y axis are equal");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 4, "X-1, Y+1");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioController, 5, "X-10, Y+10");
	
	
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 0xFFFFFFF0, "Edit X-Y Mouse Sensitivity Ratio");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 0xFFFFFFF1, "Use the buttons below to modify the in-game X-Y axis mouse sensitivity ratio.");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 1, "X+10, Y-10");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 2, "X+1, Y-1");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 0xFFFF0003, "Y Axis: %u%%");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 0xFFFF0013, "X Axis: %u%%");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 0xFFFF0023, "X & Y axis are equal");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 4, "X-1, Y+1");
	add_cartographer_label(CMLabelMenuId_EditSensitivityXYAxisRatioMouse, 5, "X-10, Y+10");
	
	
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioController, 0xFFFFFFF0, "Edit Vehicle Controller Ratio Sensitivity");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioController, 0xFFFFFFF1, "Use the buttons below to modify the in-game controller sensitivity ratio that is applied when using vehicles.");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioController, 1, "+1.0");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioController, 2, "+0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioController, 0xFFFF0003, "Sensitivity: %f");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioController, 4, "-0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioController, 5, "-1.0");
	
	
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 0xFFFFFFF0, "Edit Vehicle Mouse Ratio Sensitivity");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 0xFFFFFFF1, "Use the buttons below to modify the in-game mouse sensitivity ratio that is applied when using vehicles.");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 1, "+1.0");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 2, "+0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 0xFFFF0003, "Sensitivity: %f");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 4, "-0.05");
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 5, "-1.0");
	
	
	add_cartographer_label(CMLabelMenuId_ChangeBsp, 0xFFFFFFF0, "Change BSP");
	add_cartographer_label(CMLabelMenuId_ChangeBsp, 0xFFFFFFF2, "Change the map Structure BSP by the BSP index. Current BSP index is (%hu).");
	add_cartographer_label(CMLabelMenuId_ChangeBsp, 0xFFFFFFF3, "BSP %u");
	
	
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 0xFFFFFFF0, "Change Difficulty");
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 0xFFFFFFF2, "Change the difficulty of campaign missions. Current difficulty is %s.");
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 0xFFFFFFF3, "Unknown");
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 1, "Easy");
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 2, "Normal");
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 3, "Hard");
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 4, "Legendary");
	
	
	add_cartographer_label(CMLabelMenuId_CoopMode, 0xFFFFFFF0, "Coop Mode Fix");
	add_cartographer_label(CMLabelMenuId_CoopMode, 0xFFFFFFF2, "Toggle the Cooperative Mode fix. Auto will toggle on the fix when a shipped campaign map is played. The fix is currently %s.");
	add_cartographer_label(CMLabelMenuId_CoopMode, 0xFFFFFFF3, "active");
	add_cartographer_label(CMLabelMenuId_CoopMode, 0xFFFFFFF4, "inactive");
	add_cartographer_label(CMLabelMenuId_CoopMode, 1, "Auto");
	add_cartographer_label(CMLabelMenuId_CoopMode, 2, "On");
	add_cartographer_label(CMLabelMenuId_CoopMode, 3, "Off");
	
	
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 0xFFFFFFF0, "Edit XLLN FPS Limit");
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 0xFFFFFFF1, "Use the buttons below to change or disable the FPS limit enforced by XLiveLessNess.");
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 1, "+10");
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 2, "+1");
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 0xFFFF0003, "XLLN FPS Limit: %u");
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 0xFFFF0013, "XLLN FPS Limiter Disabled");
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 4, "-1");
	add_cartographer_label(CMLabelMenuId_EditFpsLimit, 5, "-10");
	
	
	return true;
}

bool UninitCustomLabels()
{
	return true;
}