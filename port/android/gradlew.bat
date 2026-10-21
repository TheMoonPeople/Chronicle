@echo off
setlocal EnableDelayedExpansion
set DIR=%~dp0
set GRADLE_USER_HOME=%DIR%.gradle-home
set GRADLE_DIST=%LOCALAPPDATA%\Chronicle-Android\gradle\gradle-8.13
if exist "%GRADLE_DIST%\bin\gradle.bat" (
    call "%GRADLE_DIST%\bin\gradle.bat" %*
    exit /b %ERRORLEVEL%
)
set CLASSPATH=%DIR%gradle\wrapper\gradle-wrapper-main-8.13.jar;%DIR%gradle\wrapper\gradle-wrapper-shared-8.13.jar
for %%J in ("%DIR%gradle\wrapper\*.jar") do set CLASSPATH=!CLASSPATH!;%%~fJ
if not exist "%DIR%gradle\wrapper\gradle-wrapper-main-8.13.jar" (
    echo Missing gradle-wrapper.jar in %DIR%gradle\wrapper 1>&2
    exit /b 1
)
"%JAVA_HOME%\bin\java.exe" -classpath "!CLASSPATH!" org.gradle.wrapper.GradleWrapperMain %*
