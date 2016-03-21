# Copyright AllSeen Alliance. All rights reserved.
#
#    Permission to use, copy, modify, and/or distribute this software for any
#    purpose with or without fee is hereby granted, provided that the above
#    copyright notice and this permission notice appear in all copies.
#
#    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# 

import os
import platform
import Utility
    
GatewayName = 'Gateway'

vars = Variables()

def GetPathFromEnvironVarOrArgument(pathEnvName, description, argumentName):
    defaultPath = Utility.GetDefaultPathFromEnvironVar(description, pathEnvName)
    path = ARGUMENTS.get(argumentName, defaultPath)
    if not path:
        Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
            'The {0} path is not found.'.format(description))
        Exit(1)
    
    Utility.PrintOneLineLog(Utility.EventLevel.info, GatewayName,
        'The AllJoyn core source path is {0}.'.format(path))
    
    vars.Add(PathVariable(argumentName, 
        'Directory containing the {0}'.format(description), 
        path))  
    
    return path

# Get the version of AllJoyn core from scons command line arguments and check 
# if it is supported. If AJ_VER is not specified, it will be set to the default 
# value.
defaultAlljoynVersion = '1504'
alljoynVersion = ARGUMENTS.get('AJ_VER', defaultAlljoynVersion)

supportedAlljoynVersions = ['1504']
if alljoynVersion not in supportedAlljoynVersions:
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'Building with this alljoyn core version {0} is not supported.'.format(alljoynVersion))
    Exit(1)

vars.Add(EnumVariable('AJ_VER',
                      'AllJoyn core version',
                      alljoynVersion,
                      allowed_values=('1504',)))
    
# If AJ_SRC_PATH is not specified, use the OS environment variable to generate 
# the default AllJoyn core source path.
alljoynCoreSrcPathEnvName = 'ALLJOYN_SRC_' + alljoynVersion + '_HOME'
alljoynCoreSrcPath = GetPathFromEnvironVarOrArgument(
                        alljoynCoreSrcPathEnvName, 
                        'AllJoyn core source', 
                        'AJ_SRC_PATH')

# Need to obtain a few more paths. Some of them are for Windows only and others may 
# be different between Windows and Linux.
if 'Windows' in platform.system():
    defaultOs = 'win7'
else:
    defaultOs = 'linux'
isWindows = (ARGUMENTS.get('OS', defaultOs) == 'win7')

# For Windows, if any argument of SOFIA_SIP_HOME, GLIB_HOME, ICONV_HOME, and INTL_HOME 
# is not specified, use the corresponding OS environment variable to generate the path. 
# For Linux, if SOFIA_SIP_INCLUDE is not specified, use the OS environment variable to 
# generate the Sofia SIP include path.    
if isWindows:
    sofiaSipHomeEnvName = 'SOFIA_SIP_HOME'    
    sofiaSipHome = GetPathFromEnvironVarOrArgument(
                    sofiaSipHomeEnvName, 
                    'Sofia SIP home', 
                    sofiaSipHomeEnvName)
    
    glibHomeEnvName = 'GLIB_HOME'
    glibHome = GetPathFromEnvironVarOrArgument(
                glibHomeEnvName, 
                'Glib', 
                glibHomeEnvName)  
    
    iconvHomeEnvName = 'ICONV_HOME' 
    iconvHome = GetPathFromEnvironVarOrArgument(
                    iconvHomeEnvName, 
                    'iconv', 
                    iconvHomeEnvName) 
    
    intlHomeEnvName = 'INTL_HOME'
    intlHome = GetPathFromEnvironVarOrArgument(
                intlHomeEnvName, 
                'intl', 
                intlHomeEnvName)  
else:
    sofiaSipIncludeEnvName = 'SOFIA_SIP_INCLUDE'
    sofiaSipInclude = GetPathFromEnvironVarOrArgument(
                    sofiaSipIncludeEnvName, 
                    'Sofia SIP headers', 
                    sofiaSipIncludeEnvName)        
               
# Get the global environment from AllJoyn build_core SConscript.
alljoynBuildCorePath = alljoynCoreSrcPath + '/core/alljoyn/build_core/SConscript'
alljoynBuildCorePath = alljoynBuildCorePath.replace('/', os.sep)
alljoynEnv = SConscript(alljoynBuildCorePath)
Utility.PrintAlljoynEnv(alljoynEnv)

# Clone a copy of AllJoyn core environment since we may need to change some 
# construction variables and want to keep the AllJoyn core environment intact.
env = alljoynEnv.Clone()

# Set additional construction variables which are not included in the AllJoyn 
# core environment.
vars.Update(env)
Help(vars.GenerateHelpText(env))
        
# Check if the specified build arguments are supported. Currently only win7 + x86 and 
# linux + x86/x86_64 are supported.
if Utility.IsBuildSupported(alljoynEnv):
    # Invoke the SConscript for CloudCommEngine to build the executable.
    env.SConscript(['./Src/CloudCommEngine/SConscript'], variant_dir = '$OBJDIR/cpp/CloudCommEngine', 
        duplicate = 0, exports = ['env'])   
    
    if isWindows:
        # For Windows, copy the five dlls which are required for running the executable.
        env.Command(target = "$DISTDIR/cpp/bin/libsofia_sip_ua.dll",
            source = "$SOFIA_SIP_HOME/win32/libsofia-sip-ua/$VARIANT/libsofia_sip_ua.dll",
            action = Copy("$TARGET", "$SOURCE"))
        env.Command(target = "$DISTDIR/cpp/bin/pthreadVC2.dll",
            source = "$SOFIA_SIP_HOME/win32/pthread/pthreadVC2.dll",
            action = Copy("$TARGET", "$SOURCE"))
        env.Command(target = "$DISTDIR/cpp/bin/libglib-2.0-0.dll",
            source = "$GLIB_HOME/bin/libglib-2.0-0.dll",
            action = Copy("$TARGET", "$SOURCE"))
        env.Command(target = "$DISTDIR/cpp/bin/iconv.dll",
            source = "$ICONV_HOME/bin/iconv.dll",
            action = Copy("$TARGET", "$SOURCE"))
        env.Command(target = "$DISTDIR/cpp/bin/intl.dll",
            source = "$INTL_HOME/bin/intl.dll",
            action = Copy("$TARGET", "$SOURCE"))
    else:
        # For Linux, install the two AllJoyn shared libraries which are required for  
        # running the executable.
        alljoynLibPath = alljoynCoreSrcPath + '/core/alljoyn/build/$OS/$CPU/$VARIANT/dist/cpp/lib/'
        alljoynLibPath = alljoynLibPath.replace('/', os.sep)
        env.Command(target = "$DISTDIR/cpp/bin/liballjoyn.so",
            source = alljoynLibPath + 'liballjoyn.so',
            action = Copy("$TARGET", "$SOURCE"))
        env.Command(target = "$DISTDIR/cpp/bin/liballjoyn_about.so",
            source = alljoynLibPath + 'liballjoyn_about.so',
            action = Copy("$TARGET", "$SOURCE"))
else:
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'Only the combination of win7 and x86 is supported.')
    Exit(1)