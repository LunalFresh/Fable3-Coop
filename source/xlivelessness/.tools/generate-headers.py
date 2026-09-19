#!/usr/bin/python3
import os
import re

# The closest thing I could get working in bash (the regex also works in VSCode):
#pcregrep --recursive --multiline --no-filename '(\/\/ #[0-9]+[^\n]*(\n[^\n]+[^;])+(?=\n\{))' ./

def GetListOfFiles(directory_pathname):
	if (not os.path.isdir(directory_pathname)):
		return list()
	
	listOfFile = os.listdir(directory_pathname)
	allFiles = list()
	for entry in listOfFile:
		fullPath = os.path.join(directory_pathname, entry)
		# If entry is a directory then get the list of files in that directory.
		if os.path.isdir(fullPath):
			allFiles = allFiles + GetListOfFiles(fullPath)
			pass
		else:
			allFiles.append(fullPath)
			pass
		pass
	
	return allFiles

def RunMain():
	
	headerFileContents = "\
#pragma once\n\
#include <stdint.h>\n\
#include \"xdefs.hpp\"\n\
\n"
	
	xliveFileList = [filepathname for filepathname in GetListOfFiles("../xlivelessness/xlive/") if filepathname.endswith(".cpp")]
	
	functionDeclarationsList = []
	
	for filepathname in xliveFileList:
		
		with open(filepathname, 'r') as fileHandle:
			fileContents = fileHandle.read()
			
			functionDeclarationsList += [ (int(i[1]), i[0]+";") for i in re.findall("(// #([0-9]+)[^\\n]*(\\n[^\\n]*[^;])+(?=\\n\\{))", fileContents)]
			
			pass
		
		pass
	
	functionDeclarationsList.sort(key=lambda i: i[0])
	
	headerFileContents += "\n".join([i[1] for i in functionDeclarationsList])
	headerFileContents += "\n"
	
	headerFileHandle = open("../xlivelessness/xlive/xfuncs.hpp", "w")
	headerFileHandle.write(headerFileContents)
	headerFileHandle.close()
	
	pass

if __name__ == '__main__':
	RunMain()
	pass
