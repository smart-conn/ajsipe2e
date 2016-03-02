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

# This python file contains all the utility functions used by SConstruct and SConscript.

import os

def enum(**enums):
    return type('Enum', (), enums)

EventLevel = enum(error = 'ERROR', 
                  warning = 'WARNING', 
                  info = 'INFO', 
                  verbose = 'VERBOSE')
                  
utilityName = 'Utility'
    
def PrintOneLineLog(eventLevel, sconComponent, logStr):
    print '[' + eventLevel + ']', sconComponent + ':', logStr

def GetDefaultPathFromEnvironVar(component, envName):
    defaultPath = os.environ.get(envName)
    if defaultPath:
        PrintOneLineLog(EventLevel.info, utilityName, 
        'The default {0}'.format(component) + ' path is given by ' + envName \
        + ': ' + defaultPath + '.')
    else:
        PrintOneLineLog(EventLevel.Error, utilityName, 
        'The default {0}'.format(component) + ' path is not found because the \
        environment variable ' + envName + ' is not defined.')
    return defaultPath

def PrintBuildArguments(component, argumentType, argumentValueList):
    PrintOneLineLog(EventLevel.info, utilityName, 
    'The ' + argumentType + ' for ' + component + ' are:')
    for argumentValue in argumentValueList:
        PrintOneLineLog(EventLevel.info, utilityName, '\t' + str(argumentValue))

def ReplacePathSeparators(path):
    return path.replace('/', os.sep)
                
def PrintAlljoynEnv(env):
    PrintOneLineLog(EventLevel.info, utilityName, 'Related AllJoyn core build options:')
    PrintOneLineLog(EventLevel.info, utilityName, 'OS = ' + str(env['OS']))
    PrintOneLineLog(EventLevel.info, utilityName, 'CPU = ' + str(env['CPU']))
    PrintOneLineLog(EventLevel.info, utilityName, 'TARGET_ARCH = ' + str(env['TARGET_ARCH']))
    if env['OS'] == 'win7':
        PrintOneLineLog(EventLevel.info, utilityName, 'MSVC_VERSION = ' + str(env['MSVC_VERSION']))
    PrintOneLineLog(EventLevel.info, utilityName, 'VARIANT = ' + str(env['VARIANT']))
    PrintOneLineLog(EventLevel.info, utilityName, 'WS = ' + str(env['WS']))
    PrintOneLineLog(EventLevel.info, utilityName, 'CPPDEFINES = ')
    for defineMacro in env['CPPDEFINES']:
        PrintOneLineLog(EventLevel.info, utilityName, '\t' + str(defineMacro))
    
def IsBuildSupported(env):
    if (env['OS'] == 'win7') and (env['CPU'] == 'x86'):
        return True
    else:
        return False