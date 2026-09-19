#!/bin/sh
set -e;

PROJECT_BUILD_PATH="./build/";

# If argument is "true" (case-insensitive).
if [ "$(echo "${1}" | tr '[:upper:]' '[:lower:]')" = "true" ]; then
	rm -rf "${PROJECT_BUILD_PATH}"*;
	find "$PROJECT_BUILD_PATH" -mindepth 1 -maxdepth 1 -exec rm -rf -- {} +;
	echo "Deleted \"./build/*\".";
else
	# Find all files/directories to delete (not matching any preserve pattern).
	find "${PROJECT_BUILD_PATH}" \
		! -path "${PROJECT_BUILD_PATH}" \
		! -path "${PROJECT_BUILD_PATH}cpm-package-lock.cmake" \
		! -path "${PROJECT_BUILD_PATH}_deps" \
		! -path "${PROJECT_BUILD_PATH}_deps/*-src" \
		! -path "${PROJECT_BUILD_PATH}_deps/*-src/*" \
		! -path "${PROJECT_BUILD_PATH}_deps/*-subbuild" \
		! -path "${PROJECT_BUILD_PATH}_deps/*-subbuild/*-populate-prefix" \
		! -path "${PROJECT_BUILD_PATH}_deps/*-subbuild/*-populate-prefix/*" \
		! -path "${PROJECT_BUILD_PATH}_deps/*-subbuild/CMakeLists.txt" \
		! -path "${PROJECT_BUILD_PATH}cmake" \
		! -path "${PROJECT_BUILD_PATH}cmake/*" \
		! -path "${PROJECT_BUILD_PATH}CPM_modules" \
		! -path "${PROJECT_BUILD_PATH}CPM_modules/*" \
		-print \
	| grep -v "^.$" \
	| xargs -I{} rm -rf "{}";
	
	echo "Deleted \"./build/*\" except for CPM project files.";
fi
