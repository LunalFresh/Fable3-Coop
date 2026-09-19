# [XLiveLessNess](https://xlln.glitchyscripts.com/) (XLLN)
Games for Windows LiveLessNess. A complete Games For Windows - LIVE<sup>TM</sup> (GFWL) rewrite.

# Purpose
With GFWL being slowly phased out and many online features disappearing across the range of titles using that library, the idea is to rewrite or stub all aspects of it. Doing so also improves one's ability to reverse engineer (and improve) the titles themselves due to the anti-debugging measures present within the original module. Title specific modifications however do not belong here and instead should be implemented as an [XLLN-Module](https://gitlab.com/GlitchyScripts/xlln-modules). Additionally this library should be as modular and minimalistic as possible so entire features are able to be disabled (for example network functionality or voice chat) or instead implemented as a separate and importantly optional XLLN-Module (for example UPnP / NAT-PMP / PCP port forwarding).

# Wiki
Please visit the [Wiki](https://gitlab.com/GlitchyScripts/xlivelessness/-/wikis) for anything this readme does not cover (such as User FAQ's, developer FAQ's, debug logging, etc.).

# XLLN-Modules
The purpose of an XLiveLessNess-Module is to enhance some aspect of one or many GFWL Games/Titles or XLiveLessNess itself. All XLLN-Modules installed in the `XLiveLessNess/modules/` folder (where the config is located) will be loaded.

On process attach, XLiveLessNess (or an [XLLN-Wrapper](https://gitlab.com/GlitchyScripts/xlln-wrappers) library) will codecave/hook the entry point (where AddressOfEntryPoint points to in the PE header) of the Title and load all XLLN-Modules from there in an arbitrary order. After loading all available, it will call the `XLLNModulePostInit@41101` export in each XLLN-Module (also in an arbitrary order). If that exported function returns with a value other than 0 which implies an error then the XLLN-Module will be immediately unloaded. Otherwise on success and later before the XLLN-Modules gets unloaded on regular shutdown, the `XLLNModulePreUninit@41102` export will **try to be invoked** (again in arbitrary order) which can be necessary for several clean-up and finalisation steps that cannot be done in `DllMain`.

The benefits of the `XLLNModulePostInit@41101` and `XLLNModulePreUninit@41102` exports are:
1. XLiveLessNess (`xlive.dll`) and all other dynamically linked libraries (DLLs) have definitely been internally initialised at the point of the `XLLNModulePostInit@41101` export being invoked so we can call additional XLLN logging / setup functions from within them etc. (via GetModuleHandle, GetProcAddress and the ordinal/export identifier).
2. Thread synchronisation, thread joining, `LoadLibrary`/`FreeLibrary` and invoking functions from other DLLs can be done from them.
2. If `xlive.dll` is not XLLN then other [XLLN-Wrapper](https://gitlab.com/GlitchyScripts/xlln-wrappers) libraries can load the XLLN-Modules via the same AddressOfEntryPoint codecave idea.

# Building From Source

## Required Tools
- [CMake](https://cmake.org/download/) version 3.12+
- [DirectX SDK (June 2010)](https://www.microsoft.com/download/details.aspx?id=6812)
  - Install on Windows OS
  - Install in a WINE Prefix if on Unix OS
  - Or a copy of the installed files taken from the default install location: `C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)`
- **If OS: Windows**
  - Visual Studio (15) 2017 / MSVC Build Tools v141
  - [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/)
- **If OS: Unix**
  - Make
  - [LLVM & Clang](https://releases.llvm.org/) version 18+
  - Windows Sysroot ([llwin](https://gitlab.com/superewald/llwin))
- **(Optional)** git
- **(Optional)** VSCode / VSCodium

## Configuring Tools

### Visual Studio:
Visit the Developer FAQ for which specific options to use when installing Visual Studio: https://gitlab.com/GlitchyScripts/xlivelessness/-/wikis/FAQ-for-Devs#how-do-i-install-visual-studio-2017-for-use-with-xlln-and-what-individual-components-do-i-use

### Debian based OS requires:
- `sudo apt install llvm-19 clang-19 make cmake`

- Optional:
  - `sudo apt install git wine codium`

### Windows Sysroot ([llwin](https://gitlab.com/superewald/llwin)):
Clone the llwin repository to `$HOME/.local/llwin` and use the `bin/llwin init` command to auto-configure llwin for the current user like this:
```sh
# clone repository to .local user folder
git clone https://gitlab.com/superewald/llwin $HOME/.local/llwin
# init llwin (creates symlinks for utils)
$HOME/.local/llwin/bin/llwin init --user --wine
```
To install the required Windows Sysroot run the following:
```sh
llwin install 16 --force --debug --archs x86,x86_64
llwin use 16
```
Run `llwin install` for more info on the options.

There is an issue currently with Opus v1.5.2 ([fixed](https://gitlab.xiph.org/xiph/opus/-/commit/1884ec0fe81065ba24eaaaaeb2b1718146246d2c)) where there is a letter case missmatch for an included file in case-sensitive file systems (typically on Unix operating systems). This command creates a symbolic link to the original so it will compile successfully.
```sh
ln -s $HOME/.local/llwin/win/current/sysroot/Windows\ Kits/10/include/10.0.22621/shared/intsafe.h $HOME/.local/llwin/win/current/sysroot/Windows\ Kits/10/include/10.0.22621/shared/Intsafe.h
```

## Configure and Build the Project

### Using VSCode / VSCodium:
Open the root folder of the project then run the `Configure Project` task and follow the prompts. Use the `Clean Project` task first if swapping to a different build architecture or need to do a complete reconfigure and rebuild. Once configured successfully, the `Build Project` task can be used to compile, link and output the completed binary in the `./bin/` directory.

### Using CMake / llwin's `wmake` command:

#### To configure:
The CMake GUI can be used to configure the project on Windows as an alternative to running the following command manually:
- If you are on **Windows** use: `cmake -B build`
- if you are on **Unix** use: `wmake -B build`
- Add the following options to the above command:
  - `-G '<generator>'` - To specify the generator.
    - `Visual Studio 15 2017` - The default on Windows if MSVC is installed.
    - `Makefile` - The default on Unix.
  - `-A <platform>` - To specify the plaform/architecture.
    - `Win32` - All officially released GFWL Titles are 32-bit.
    - `x64` - XLLN also supports compiling for 64 bit architectures.
  - `-DCMAKE_BUILD_TYPE=<configuration>` - required for the `Makefile` generator.
    - `Debug` - No optimisation, has build symbols.
    - `Release` - Has optimisation, no build symbols.
    - `RelWithDebInfo` - Has optimisation, has build symbols.
    - `MinSizeRel` - Has optimisation, no build symbols, smallest output binary file size.
  - `-DWINE_PREFIX_DIR="<path>"` - To specify the location of a particular WINE Prefix.
    - This path is used to detect the default install location of a DirectX SDK installation and is also used as the location to install the project's output binary to.
    - `~/.wine` - Default path if running on Unix OS.
    - ` ` - Default path if running on Windows OS.
  - `-DDXSDK_DIR="<path>"` - To explicitly specify the location of the DirectX SDK installation files.

Unix Example:
```sh
wmake -B build -A Win32 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWINE_PREFIX_DIR="~/.wine-32"
```
Windows Example:
```sh
cmake -B build -G 'Visual Studio 15 2017' -A Win32 -DDXSDK_DIR="C:/Microsoft DirectX SDK (June 2010)/"
```

#### To build:
Both on Windows or Unix run:
```sh
cmake --build build --parallel
```
Add any of the following options as necessary:
- `--config=<configuration>` - If using a Visual Studio generator.
  - `Debug` - No optimisation, has build symbols.
  - `Release` - Has optimisation, no build symbols.
  - `RelWithDebInfo` - Has optimisation, has build symbols.
  - `MinSizeRel` - Has optimisation, no build symbols, smallest output binary file size.

### Using Visual Studio:
CMake must be used to configure the project first (also possible via the VSCode/VSCodium Task) before Visual Studio can be used to open the generated project (in the new `./build/` folder) to then be able to compile it or debug it (in runtime on a GFWL Title).

## Launching and Debugging

Currently only **Visual Studio** is supported for compiling and debugging in runtime with symbols loading correctly. Possibly VSCode too with proprietary Microsoft C/C++ extension.

# Contributions
You may make contributions via pull requests. However it is best to check or add to the [issues board](https://gitlab.com/GlitchyScripts/xlivelessness/-/issues) before starting any work to ensure that the work isn't already being covered by someone else or is being planned out differently.

# Submitting New Issues
- Please do not submit feature requests (without any thought, design or planning put into it). This should generally only be done by active contributors.
- If you encounter a regression in Title compatibility between updates please open a new issue detailing what specific version it worked on and what (newer) version it does not work on with the steps and settings necessary to reproduce the issue. Additionally any logs or crash offsets will be very helpful for those who do look into the issue.
- If you wish to report a Title that does not function correctly (or one that now does!) then please add to this existing issue [GFWL Title/Game Compatibility List](https://gitlab.com/GlitchyScripts/xlivelessness/-/issues/1) in the comments. Also please specify if it is all working or partially working (single-player, offline/system-link multiplayer, online multiplayer).
- Bug reports of general XLiveLessNess functionality should contain as much information as possible such as reproduction steps, debug logs, crash logs, version(s) used, build commit(s) (or dates) and build options (debug/release).
- Alternatively, if your intended submission is not covered by the points above then we appreciate your interest in the project and look forward to your contribution.

# XLiveLessNess License

Copyright (C) 2026 [Glitchy Scripts](https://glitchyscripts.com/)

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

# Opus Codec License

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the names of specific contributors, may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Fantasy Name Generator License

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

# RapidXml License

Use of this software is granted under one of the following two licenses,
to be chosen freely by the user.

## 1. Boost Software License - Version 1.0 - August 17th, 2003

Copyright (c) 2006, 2007 Marcin Kalicinski

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

## 2. The MIT License

Copyright (c) 2006, 2007 Marcin Kalicinski

Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
IN THE SOFTWARE.

# FIPS-180-2 Compliant SHA-256 Implementation License
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

# CRC32 Hash License
Copyright (C) 1986 Gary S. Brown.  You may use this program, or code or tables extracted from it, as desired without restriction.
