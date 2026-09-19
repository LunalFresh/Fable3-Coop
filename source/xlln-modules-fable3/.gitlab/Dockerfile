# escape=`

FROM mcr.microsoft.com/windows/servercore:ltsc2022

SHELL ["cmd", "/S", "/C"]

RUN curl.exe --location --create-dirs -o C:\TEMP\VisualStudio.chman https://aka.ms/vs/15/release/channel
RUN curl.exe --location --create-dirs -o C:\TEMP\vs_buildtools.exe https://aka.ms/vs/15/release/vs_buildtools.exe
RUN C:\TEMP\vs_buildtools.exe --quiet --wait --norestart --nocache `
    --channelUri C:\TEMP\VisualStudio.chman                        `
    --installChannelUri C:\TEMP\VisualStudio.chman                 `
    --add Microsoft.VisualStudio.Workload.VCTools                  `
    --add Microsoft.VisualStudio.Component.VC.140                  `
    --add Microsoft.VisualStudio.Component.Windows81SDK            `
    --add Microsoft.Component.VC.Runtime.UCRTSDK                   `
    --installPath C:\BuildTools

ENTRYPOINT C:\BuildTools\Common7\Tools\VsDevCmd.bat && cmd /c
