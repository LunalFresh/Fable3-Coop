@echo off
setlocal enabledelayedexpansion

REM Change to build directory. Abort if it does not exist otherwise everything below would delete from the wrong directory.
cd /d build || exit /b 1

REM If Clean everything.
IF /I "%~1"=="true" (
	
	REM If argument is true, delete all files and folders (including hidden and those beginning with .) in current directory.
	
	REM Delete all files (including hidden and dotfiles).
	del /a /q *.* >nul 2>nul
	del /a /q .* >nul 2>nul
	
	REM Remove all directories (including hidden and dot-prefixed).
	FOR /d %%G IN (*) DO rd /s /q "%%G"
	FOR /d %%G IN (.*) DO rd /s /q "%%G"
	
) ELSE (
	
	REM Loop through all files (including hidden/dot-prefixed).
	FOR /F "delims=" %%F IN ('dir /A:-D /B') DO (
		IF /I NOT "%%F"=="cpm-package-lock.cmake" (
			REM Remove attributes from files except exclusion.
			attrib -H -S "%%F"
			REM Delete files except the exclusion, including hidden/system files.
			del /Q "%%F"
		)
	)
	
	REM Loop through all directories (including hidden/dot-prefixed).
	FOR /F "delims=" %%F IN ('dir /A:D /B') DO (
		IF /I NOT "%%F"=="_deps" IF /I NOT "%%F"=="cmake" IF /I NOT "%%F"=="CPM_modules" (
			REM Remove attributes from folder.
			attrib -H -S "%%F"
			REM Delete folder, including hidden/system folders.
			rd /S /Q "%%F"
		)
	)
	
	cd /d "_deps" 2>nul
	IF NOT errorlevel 1 (
		REM Loop through all directories (including hidden/dot-prefixed).
		FOR /F "delims=" %%F IN ('dir /A:D /B') DO (
			set "folder=%%F"
			IF NOT "!folder:~-4!"=="-src" IF NOT "!folder:~-9!"=="-subbuild" (
				REM Remove attributes from folder.
				attrib -H -S "%%F"
				REM Delete folder, including hidden/system folders.
				rd /S /Q "%%F"
			)
			IF "!folder:~-9!"=="-subbuild" (
				cd "%%F"
				
				REM Loop through all files (including hidden/dot-prefixed)
				FOR /F "delims=" %%F IN ('dir /A:-D /B') DO (
					IF /I NOT "%%F"=="CMakeLists.txt" (
						REM Remove attributes from files except exclusion.
						attrib -H -S "%%F"
						REM Delete files except the exclusion, including hidden/system files.
						del /Q "%%F"
					)
				)
				
				FOR /F "delims=" %%F IN ('dir /A:D /B') DO (
					set "folder=%%F"
					IF NOT "!folder:~-16!"=="-populate-prefix" (
						REM Remove attributes from folder.
						attrib -H -S "%%F"
						REM Delete folder, including hidden/system folders.
						rd /S /Q "%%F"
					)
				)
				
				cd ..
			)
		)
		
		cd ..
	)
	
)

cd ..
