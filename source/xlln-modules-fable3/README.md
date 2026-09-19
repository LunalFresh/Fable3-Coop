# XLiveLessNess (XLLN)
Games for Windows LiveLessNess. A complete Games For Windows - LIVE<sup>TM</sup> (GFWL) rewrite.

# XLLN-Modules
The purpose of an XLiveLessNess-Module is to enhance some aspect of one or many GFWL Games/Titles or XLiveLessNess itself.
For example one XLLN-Module may only enhance a single Title in ways such as additional configuration, windowed or borderless modes or even bug fixes to the Title itself. Another XLLN-Module could instead focus on broad functionalities such as embedding/overlaying the XLiveLessNess interface on top of Title windows, or features across many Titles for example that share something in common. XLLN-Network-Adapter is an example of a simple mod that prompts users with a pop-up message box asking which network adapter they want to use for LAN or online play and works on any xlive.dll. So it does not even need to target a Title or XLLN to be a useful XLLN-Module.

It is expected that XLLN-Modules that target specific Titles are named after the Title itself. If the codebase is inherited/migrated from another existing project then the name should be `XLLN-Title-Name-Project-Name`. An example of this naming convention is Project `Cartographer` for which it only targets the `Halo 2` Title. Its module name is therefore `XLLN-Halo-2-Cartographer`.

Any XLLN-Module that only targets specific Titles should only activate (`return true;` in `bool InitXllnModule(...)` ) when it is invoked from the correct Title. For example via checksum checks on the Title PE. In the case that access to other XLLN-Modules is required on startup and shutdown then use the `XLLNModulePostInit@41101` and `XLLNModulePreUninit@41102` exports (with `GetModuleHandle`, `GetProcAddress` and the ordinal/export identifier).

Need help figuring something out? Go take a look at the Wiki for answers:
[Wiki Page](https://gitlab.com/GlitchyScripts/xlivelessness/-/wikis)

## Noteworthy Considerations/Features
XLLN-Modules that are limited to run only on certain Title SHA-256 checksums calculate the same checksum even when **ONLY** the `IMAGE_FILE_LARGE_ADDRESS_AWARE` flag has been statically enabled/patched on the executable binary (so do not update the PE header checksum too). This feature is useful for increasing the RAM allocation limit of the Title from 2GB to 4GB. Read more about the LAA flag here <https://helloacm.com/large-address-aware/> and I recommend this tool to patch ONLY LAA <https://ntcore.com/explorer-suite/> (Use `CFF Explorer.exe`. Open Title.exe -> File Header -> Characteristics -> check "App can handle >2gb address space" -> Save).

## XLLN-Fable-3
**XLLN-Module:** `xlln-fable-3.dll`
<br>**Title:** Fable 3
<br>**Compatible Versions:**
<br>- `Fable3.exe` Shipped - **SHA-256:** `5a6dc0f02aae4fb2fd7b67662f3c9a65a6f1c446efb13d661f01730e0e01c914`
<br>**Features:**
<br>- Detours the built in but empty logging functions to instead actually output to the XLLN log.

## XLLN-FUEL
**XLLN-Module:** `xlln-fuel.dll`
<br>**Title:** FUEL
<br>**Compatible Versions:**
<br>- `FUEL.exe` - **SHA-256:** `ac1b2077137b7c6299c344111857b032635fe3d4794bc5135dad7c35feeda856`
<br>**Features:**
<br>- Enables running Title from main executable (`FUEL.exe`) rather than only via the launcher (`SecuLauncher.exe`).
<br>- Enables running multiple instances of the Title concurrently.
<br>- Removes loading achievements since XLLN does not have a good enough implementation yet.
<br>- Patches out Voice Chat if it is disabled in XLLN to prevent a crash.
<br>- Skips the auto-save warning screen.

## XLLN-Gears-Of-War
**XLLN-Module:** `xlln-gears-of-war.dll`
<br>**Title:** Gears of War
<br>**Compatible Versions:**
<br>- `WarGame-G4WLive.exe` Patch 3 - **SHA-256:** `968b7fa90f8579fed97d81d9dac7073fbe1cfabe519fb2af1f77fc499c01279e`
<br>**Features:**
<br>- Enables running Title from main executable (`WarGame-G4WLive.exe`) rather than only via the launcher (`Startup.exe`) (note that the game still needs to be launched with particular execution flags for it to function correctly).
<br>- Registers network socket ports and adequite offsets (Base Port Offset Mappings) with XLLN to allow running and connecting between multiple instances and when using the Base Port feature.
<br>- Bypassed the DLL integrity check function.
<br>- Allow modifying Title config files.

## XLLN-Halo-2
**XLLN-Module:** `xlln-halo-2.dll`
<br>**Title:** Halo 2
<br>**Compatible Versions:**
<br>- `halo2.exe` Shipped - **SHA-256:** `0bc0e90042fbdd6aa6848ecbac621f2617f1bd5ac49f9d4f4cb6d87d9fa70a2c`
<br>- `halo2.exe` Patch 1 - **SHA-256:** `8297d21585f46de5f88269c0a27c8bfedd13345fb0573720968277ca5019f24e`
<br>- `halo2.exe` Patch 2 - **SHA-256:** `33119398d3f68ad569c5010b652039105d3ce1b6512c418508e49ce69309f109`
<br>- `h2server.exe` Shipped - **SHA-256:** `e9388d479bceec1484def89acf664701d8b9404b79a31f4a3984b7ba79f0a07f`
<br>- `h2server.exe` Patch 1 - **SHA-256:** `850d84c5eef21ffc1f31f3c890e0fe741bed55ed8b66557c4b8839b01404c599`
<br>- `h2server.exe` Patch 2 - **SHA-256:** `422556f7a99ecde2e2cbb8fdda45e1c28b583d8f829d00ab24419213985e5a3e`
<br>- `halo2.exe` Patch 2 deobfuscated by Glitchy Scripts version 2 - **SHA-256:** `e2b8f259170182921a6922f8be3c4be3c9ae75b9f8530563b4c209e55d0c0aec`
<br>- `h2server.exe` Patch 2 deobfuscated by Glitchy Scripts version 2 - **SHA-256:** `5775d9607132f38a4190be445ae287cbdd87c5ea617cac6a7d1c42294432e6cb`
<br>**Features:**
<br>- Enables running multiple instances of the Title concurrently.
<br>- Enables running on a remote desktop connection.
<br>- Disables the ESRB warning (which only occurs for English language).
<br>- Set the NETWORK Server List Ping Frequency from 1500 to 3000 milliseconds.
<br>- Set the NETWORK Server List clear data older than from 2000 to 9000 milliseconds.
<br>- Show game details menu in NETWORK server list too.
<br>- Allow downloading custom maps when game is in progress. Otherwise you will get match has already begun error.
<br>- Better windowed mode experience which allows a resizable and maximisable window.
<br>- Disables profiles and game saves encryption.
<br>- Removes activation check.
<br>- Enables custom maps that have some custom tags.
<br>- Allow custom map downloading in NETWORK lobby.

## XLLN-Halo-2-Cartographer
**XLLN-Module:** `xlln-halo-2-cartographer.dll`
<br>**Title:** Halo 2
<br>**Compatible Versions:**
<br>- `halo2.exe` Patch 2
<br>- Other versions get minimal to no support.
<br>**Execution Arguments:**
<br>- Specify the config file to use: `-pcartoconfig=path-to-cartographer-config.ini`
<br>**Features:**
<br>- Added cooperative campaign multiplayer fixes / mode.
<br>- Added Custom Guide menu and settings menu.
<br>- Change in-game Field of View for the player and vehicles (separately).
<br>- Change in-game controller and mouse sensitivity (in and out of vehicles, axis-independent sensitivity, raw mouse).
<br>- Change the crosshair vertical offset.
<br>- Change the game's language.
<br>- Change LAN/NETWORK lobby name setting.
<br>- Change XLLN FPS limiter in custom menu settings.
<br>- Toggle hiding the HUD and or the First Person model.
<br>- Toggle hiding the in-game chat.
<br>- Toggle Campaign Skulls (does not save between restarts).
<br>- Toggle xDelay setting.
<br>- Toggle disabling kill volumes setting.
<br>- Toggle disabling auto eject on vehicle flip over setting.
<br>- Toggle warp fix.
<br>- Motion sensor fix.
<br>- Sun Flare fix.

## XLLN-Network-Adapter
**XLLN-Module:** `xlln-network-adapter.dll`

This module prompts users with a pop-up message box asking users to set which network adapter they want to use for LAN or online play. It works on any xlive.dll (even GFWL) provided it is loaded/injected on start.
It works by hooking `XLiveInitialize(...)@5000` and `XLiveInitializeEx(...)@5297` and overrides the preferred network adapter that the Title would usually specify (or sets it if it doesn't). Therefore it only prompts the user when the Title goes to initialise xlive.dll.

This is more of an old example than it is something anyone should actually use.

## XLLN-Quick-Patch
**XLLN-Module:** `xlln-quick-patch.dll`

This module is designed to read specially formatted `.json` files from the `XLiveLessNess/xlln-quick-patch/` directory. Those files contain simple definitions used to patch a specific running Title in various ways. Refer to the example JSON configuration file which contains comments describing each of the different options and how to use them: `bin-quick-patch/XLiveLessNess/xlln-quick-patch/xlln-example.json`.

## XLLN-Shadowrun
**XLLN-Module:** `xlln-shadowrun.dll`
<br>**Title:** Shadowrun
<br>**Compatible Versions:**
<br>- `Shadowrun.exe` Shipped - **SHA-256:** `360a7a55194b52343113dd9a42a89d16ec82c61e0ceb390f569f0cde090e5c52`
<br>- `Shadowrun.exe` Patch 3 - **SHA-256:** `c03c4e4e5b04ed972b013b5a124c8e37b339b5c58a1ae82ff1f2c0df95e6d567`
<br>**Features:**
<br>- Better windowed mode experience which allows a resizable and maximisable window.
<br>- Title debug logs are hooked and get logged to XLLN instead.
<br>- [Patch 3] Bypass Title asset cryptographic verification so they can be modified.

## XLLN-Universe-At-War-Earth-Assault
**XLLN-Module:** `xlln-universe-at-war-earth-assault.dll`
<br>**Title:** Universe at War: Earth Assault
<br>**Compatible Versions:**
<br>- `UAWEA.exe` Shipped - **SHA-256:** `5a1e7e33387892c1287facdfd21f75994c7c306388afd22d7d5ec5f1a365cc6a`
<br>- `UAWEA.exe` Patch 1 - **SHA-256:** `9dec30382798c38fbe190da05cff0cbbb923b4a5ada9b7dbfce3e16816dc987a`
<br>- `UAWEA.exe` Patch 2 - **SHA-256:** `21cd31086e501052505348da238d49fd30f9b1e723d6661d53e159b0467d6d84`
<br>- `UAWEA.exe` Patch 3 - **SHA-256:** `ef707daaaba7d1779f1312fdcc58c2e2336897ebf7e9e3cfca6cabc07943958c`
<br>**Execution Arguments:**
<br>- Play in windowed mode: `-windowed`
<br>**Features:**
<br>- Enables running Title from main executable (`UAWEA.exe`) rather than only via the launcher (`LaunchUAW.exe`).
<br>- Enables running multiple instances of the Title concurrently.
<br>- Disables parental controls.
<br>- Allow mods in LIVE multiplayer.
<br>- [Patch 2] Fix bug in Allow_User_Created_Content function.
<br>- Title debug logs are hooked and get logged to XLLN instead.

## XLLN-UPnP
**XLLN-Module:** `xlln-upnp.dll`

This XLLN-Module adds support for NAT-PMP (NAT Port Mapping Protocol) and UPnP (IGD v1 and v2) port forwarding. This XLLN-Module works with any Title but requires XLiveLessNess >v1.5.0.1 to function.

Only one of the supported port mapping services are required for this XLLN-Module to succeed. It will attempt to use NAT-PMP before trying UPnP. Once the socket/port is no longer required by the Title / XLLN the mapping will then be deleted. Port mappings are requested with a lease time of 60 minutes where supported (otherwise it is indefinite until generally when the modem/service restarts). Every 59 minutes any existing port mappings will automatically be renewed by this XLLN-Module.

UPnP support was implemented via the help of the MiniUPnP project library.

## XLLN-Warturd
**XLLN-Module:** `xlln-warturd.dll`

This module is a tool that targets any Titles that contain code obfuscation generated by what is believed to be Microsoft's Warbird obfuscator. Examples of such Titles are Halo 2 Vista, Halo 2 Vista Dedicated Server and Shadowrun.

To enable Warturd the `-xllnwarturd` execution flag must be present.<br>
Refer to the JSON configuration file for setting up deobfuscation attempts: `bin-warturd/XLiveLessNess/xlln-warturd.json`.<br>
Note: in runtime the JSON must be located in the same directory as the main XLiveLessNess Configuration file.

Warturd can be configured to search for and automatically deobfuscate the specified binary (.exe or .dll). Deobfuscating a binary is a multi-step process using this tool. It works by leveraging parts of the partial deobfuscation algorithm already inside the obfuscated binary. It takes all the partially deobfuscated bits it detects and gathers then stitches them all together into a whole linear deobfuscated assembly function. Therefore this tool actually works in runtime and cannot deobfuscate the binary statically / without executing code in it.

The multi-step process of successfully using this tool to deobfuscate a binary is:
1. Set `scanForObfuscation = true` and everything else false. Then run the binary and get the outputted scan at the location specified by `resultDestinationPath` (make sure this folder path exists).
2. Copy the results of the scan into `xlln-warturd.json` and `scanForObfuscation = false` `patchLimitedAccuracy = true`  and `patchStatically = true`.
3. Run the binary. The deobfuscated binary should now exist in the `resultDestinationPath` directory.
4. Disassemble the resultant binary (using your favourite disassembler-decompiler) and start fixing the offsets you copied into `xlln-warturd.json` as now the binary is a lot cleaner and references are easier to find (you will probably need to open up the unedited version of the binary too to help compare where automatically found offsets are slightly wrong).
5. Repeat steps 3 and 4 until all deobfuscated functions can be disassembled and have resolved references.
6. Turn off `patchLimitedAccuracy` and run the binary again to get your completed deobfuscated binary.

Additional notes:
 - I find using `patchStatically` easier to verify offsets than to do it in runtime via `patchRuntime`.
 - `scanForObfuscation` `patchLimitedAccuracy` `patchRuntime` `patchStatically` can all be on at the same time if you really want. Warbird's order of execution is: scan then patch runtime then export binary.
 - It may help to try this process on an already figured out and deobfuscated Warbird binary to understand where to copy the offsets and how the automatically found offsets can be slightly wrong.

## XLiveLessNess-Modules License

Copyright (C) 2025 Glitchy Scripts

This library is free software; you can redistribute it and/or
modify it under the terms of exclusively the GNU Lesser General Public
License version 2.1 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License version 2.1 along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

## MiniUPnP License
BSD 3-Clause License

Copyright (c) 2005-2024, Thomas BERNARD
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## RapidJSON License
Tencent is pleased to support the open source community by making RapidJSON available. 
 
Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.  All rights reserved.

If you have downloaded a copy of the RapidJSON binary from Tencent, please note that the RapidJSON binary is licensed under the MIT License.
If you have downloaded a copy of the RapidJSON source code from Tencent, please note that RapidJSON source code is licensed under the MIT License, except for the third-party components listed below which are subject to different license terms.  Your integration of RapidJSON into your own projects may require compliance with the MIT License, as well as the other licenses applicable to the third-party components included within RapidJSON. To avoid the problematic JSON license in your own projects, it's sufficient to exclude the bin/jsonchecker/ directory, as it's the only code under the JSON license.
A copy of the MIT License is included in this file.

Other dependencies and licenses:

### Open Source Software Licensed Under the BSD License:

The msinttypes r29 
Copyright (c) 2006-2013 Alexander Chemeris 
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of  copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

### Open Source Software Licensed Under the JSON License:

json.org 
Copyright (c) 2002 JSON.org
All Rights Reserved.

JSON_checker
Copyright (c) 2002 JSON.org
All Rights Reserved.

### Terms of the JSON License:

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

The Software shall be used for Good, not Evil.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### Terms of the MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Zyan Disassembler Library (Zydis) License
Original Author : Florian Bernd, Joel Hoener

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## FIPS-180-2 Compliant SHA-256 Implementation License
Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License"); you may
not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

## CRC32 Hash License
Copyright (C) 1986 Gary S. Brown.  You may use this program, or code or tables extracted from it, as desired without restriction.
