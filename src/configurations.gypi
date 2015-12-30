# By default gyp/msbuild build for 32-bit Windows.  To build 64-bit Windows,
# use this file.
#
#     C:\proj\winpty\src>gyp -I configurations.gypi
#
# This command generates Visual Studio project files with a Release
# configuration and two Platforms--Win32 and x64.  Both can be built:
#
#     C:\proj\winpty\src>msbuild winpty.sln /p:Platform=Win32
#     C:\proj\winpty\src>msbuild winpty.sln /p:Platform=x64
#
# The output is placed in:
#
#     C:\proj\winpty\src\Release\Win32
#     C:\proj\winpty\src\Release\x64
#
# This file is not included by default, because I suspect it would interfere
# with node-gyp, which has a different system for building 32-vs-64-bit
# binaries.  It uses a common.gypi, and the project files it generates can only
# build a single architecture, the output paths are not differentiated by
# architecture.

{
    'target_defaults': {
        'default_configuration': 'Release_Win32',
        'configurations': {
            'Release_Win32': {
                'msvs_configuration_platform': 'Win32',
            },
            'Release_x64': {
                'msvs_configuration_platform': 'x64',
            },
        },
        'msvs_configuration_attributes': {
            'OutputDirectory': '$(SolutionDir)$(ConfigurationName)\\$(Platform)',
            'IntermediateDirectory': '$(ConfigurationName)\\$(Platform)\\obj\\$(ProjectName)',
        }
    }
}
