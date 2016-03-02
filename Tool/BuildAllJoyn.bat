@echo off

set alljoynBuildPath=%CD%
set VsVersion=11.0
set wsFlag=on
set wsFlagInScons=

if not -%1-==-- (
    set alljoynBuildPath=%1
)

if not -%2-==-- (
    set VsVersion=%2
)

if not -%3-==-- (
    set wsFlag=%3
    set wsFlagInScons=WS=off
)

echo Build path is %alljoynBuildPath%.
echo Visual Studio version is %VsVersion%.
echo wsFlag is %wsFlag%.

echo Change to the specified path.
pushd %alljoynBuildPath%

echo ==============================================================================================
echo Build starts.
echo ==============================================================================================

echo ==============================================================================================
echo Build x86 chk.
echo ==============================================================================================
call scons OS=win7 CPU=x86 MSVC_VERSION=%VsVersion% BINDINGS=cpp %wsFlagInScons%

echo ==============================================================================================
echo Build x86_64 chk.
echo ==============================================================================================
call scons OS=win7 CPU=x86_64 MSVC_VERSION=%VsVersion% BINDINGS=cpp %wsFlagInScons%

echo ==============================================================================================
echo Build x86 rel.
echo ==============================================================================================
call scons OS=win7 CPU=x86 VARIANT=release MSVC_VERSION=%VsVersion% BINDINGS=cpp %wsFlagInScons%

echo ==============================================================================================
echo Build x86_64 rel.
echo ==============================================================================================
call scons OS=win7 CPU=x86_64 VARIANT=release MSVC_VERSION=%VsVersion% BINDINGS=cpp %wsFlagInScons%

echo ==============================================================================================
echo Build ends.
echo ==============================================================================================

echo Restore to the original path.
popd