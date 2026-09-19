# Third-party notices & credits

The mod builds on GlitchyScripts' **XLiveLessNess** and **XLLN Modules**. Their original notices and LGPL 2.1 license texts are retained. Modified source and build files are included in `source/xlivelessness` and `source/xlln-modules-fable3`; `source/PROVENANCE.json` records the upstream base commits. These snapshots include local modifications, so the base commits alone do not reproduce this beta.

Upstream project addresses:

- XLiveLessNess: https://gitlab.com/GlitchyScripts/xlivelessness
- XLLN Modules: https://gitlab.com/GlitchyScripts/xlln-modules

The modifications include Fable III Patch 2 integration, the peer friends/invite protocol and Sanctuary-style menu, explicit friend requests, accepted-invite delivery, the native Fable kick bridge, and system-random virtual network identities.

The xlive DLL includes **Opus 1.5.2** (LICENSE-Opus.txt) and uses **RapidXML 1.13** (LICENSE-RapidXML.txt). Other original source notices, including fantasyname and utilities, remain with their files. The upstream module source tree is retained for context; this package installs only the Fable III module, not other title modules.

No Fable game executable, game data, DLC, save, activation file, or Microsoft font is distributed. The renderer selects Palatino Linotype from installed Windows fonts. The launcher script opens the recipient's existing FableLauncher.exe. No hosted backend or credentials are needed to build the runtime.

Standalone packaging and installation scripts use the MIT license in LICENSE-Installer.txt.
