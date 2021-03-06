robocopy .\build32\rundir\RelWithDebInfo .\build\ /E /XF .gitignore
robocopy .\build64\rundir\RelWithDebInfo .\build\ /E /XC /XN /XO /XF .gitignore
7z a build.zip .\build\*