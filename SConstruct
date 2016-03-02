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
import Utility
    
GatewayName = 'Gateway'

vars = Variables()

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
    
# In case that AJ_SRC_PATH is not specified, use the OS environment variable to 
# generate the default AllJoyn core source path.
alljoynCoreSrcPathEnvName = 'ALLJOYN_SRC_' + alljoynVersion + '_HOME'
defaultAlljoynCoreSrcPath = Utility.GetDefaultPathFromEnvironVar(
                             'AllJoyn core source', 
                             alljoynCoreSrcPathEnvName)

alljoynCoreSrcPath = ARGUMENTS.get('AJ_SRC_PATH', defaultAlljoynCoreSrcPath)
if not alljoynCoreSrcPath:
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'The AllJoyn core source path is not found.')
    Exit(1)

Utility.PrintOneLineLog(Utility.EventLevel.info, GatewayName,
'The AllJoyn core source path is {0}.'.format(alljoynCoreSrcPath))

vars.Add(PathVariable('AJ_SRC_PATH', 
                      'Directory containing the AllJoyn core source', 
                      alljoynCoreSrcPath))                   

# In case that SOFIA_SIP_HOME is not specified, use the OS environment variable to 
# generate the default Sofia SIP home path.    
sofiaSipHomeEnvName = 'SOFIA_SIP_HOME'
defaultSofiaSipHome = Utility.GetDefaultPathFromEnvironVar(
                         'Sofia SIP home', 
                         sofiaSipHomeEnvName)

sofiaSipHome = ARGUMENTS.get('SOFIA_SIP_HOME', defaultSofiaSipHome)
if not sofiaSipHome:
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'The Sofia SIP home path is not found.')
    Exit(1)

Utility.PrintOneLineLog(Utility.EventLevel.info, GatewayName,
    'The Sofia SIP home path is {0}.'.format(sofiaSipHome))      

vars.Add(PathVariable('SOFIA_SIP_HOME', 
                      'Directory containing Sofia SIP', 
                      sofiaSipHome))        

# In case that GLIB_HOME is not specified, use the OS environment variable to 
# generate the default Glib home path.  
glibHomeEnvName = 'GLIB_HOME'
defaultGlibHome = Utility.GetDefaultPathFromEnvironVar(
                         'Glib home', 
                         glibHomeEnvName)
                
glibHome = ARGUMENTS.get('GLIB_HOME', defaultGlibHome) 
if not glibHome:
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'The Glib home path is not found.')
    Exit(1)

Utility.PrintOneLineLog(Utility.EventLevel.info, GatewayName,
        'The Glib home path is {0}.'.format(glibHome))

vars.Add(PathVariable('GLIB_HOME', 
                      'Directory containing Glib', 
                      glibHome))

# In case that ICONV_HOME is not specified, use the OS environment variable to 
# generate the default iconv home path.
iconvHomeEnvName = 'ICONV_HOME'
defaultIconvHome = Utility.GetDefaultPathFromEnvironVar(
                         'iconv home', 
                         iconvHomeEnvName)

iconvHome = ARGUMENTS.get('ICONV_HOME', defaultIconvHome)
if not iconvHome:
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'The iconv home path is not found.')
    Exit(1)

Utility.PrintOneLineLog(Utility.EventLevel.info, GatewayName,
    'The iconv home path is {0}.'.format(iconvHome))

vars.Add(PathVariable('ICONV_HOME', 
                      'Directory containing iconv', 
                      iconvHome))   
                      
# In case that INTL_HOME is not specified, use the OS environment variable to 
# generate the default intl home path.
intlHomeEnvName = 'INTL_HOME'
defaultIntlHome = Utility.GetDefaultPathFromEnvironVar(
                         'intl home', 
                         intlHomeEnvName)

intlHome = ARGUMENTS.get('INTL_HOME', defaultIntlHome)
if not intlHome:
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'The intl home path is not found.')
    Exit(1)

Utility.PrintOneLineLog(Utility.EventLevel.info, GatewayName,
    'The intl home path is {0}.'.format(intlHome))

vars.Add(PathVariable('INTL_HOME', 
                      'Directory containing intl', 
                      intlHome))   
                         
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
        
# Check if the specified build arguments are supported. Currently only Win7 and 
# x86 are supported.
if Utility.IsBuildSupported(alljoynEnv):
    # Invoke the SConscript for CloudCommEngine to build the executable.
    env.SConscript(['./Src/CloudCommEngine/SConscript'], variant_dir = '$OBJDIR/cpp/CloudCommEngine', 
        duplicate = 0, exports = ['env'])   
    
    # Copy five dlls which are required for running the executable.
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
    Utility.PrintOneLineLog(Utility.EventLevel.error, GatewayName, 
    'Only the combination of win7 and x86 is supported.')
    Exit(1)