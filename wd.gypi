{
  'variables': {
    'QT5': '1',
    'WD_CONFIG_QWIDGET_BASE': '1',
# If Qt >= 5.6 set 'WD_CONFIG_WEBKIT' : '0'
    'WD_CONFIG_WEBKIT': '0',
    'WD_CONFIG_QUICK': '1',
    'WD_CONFIG_PLAYER': '0',
    'WD_CONFIG_ONE_KEYRELEASE': '0',
    'QT_INC_PATH': 'C:/Qt/Qt5.8.0/5.8/msvc2015_64/include',
    'QT_BIN_PATH': 'C:/Qt/Qt5.8.0/5.8/msvc2015_64/bin',
    'QT_LIB_PATH': 'C:/Qt/Qt5.8.0/5.8/msvc2015_64/lib',
  },
  'target_defaults': {
      'configurations':
      {
          'Default': {
          },
          'Default_x64': {
              'inherit_from': ['Default'],
              'msvs_configuration_platform': 'x64',
              'defines': [ 'DEBUG=1' ],
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'RuntimeLibrary': 3,    # Use /MTd for static debug
                  'Optimization': 0,
                  'BasicRuntimeChecks': 0,
                  'ProgramDataBaseFileName': '$(OutDir)\\$(ProjectName).pdb'
                },
                'VCLinkerTool': {
                  'ProgramDatabaseFile': '$(OutDir)\\$(ProjectName).pdb'
                }
              }
          },
      },
  },
}
