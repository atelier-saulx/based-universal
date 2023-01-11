{
  'targets': [
    {
      'target_name': 'hello-native',
      'sources': [ 'src/hello.cc' ],
      'include_dirs': ["<(module_root_dir)", "<!@(node -p \"require('node-addon-api').include\")"],
      'libraries': [ "-Wl,-rpath,<(module_root_dir)", '-lbased', '-L<(module_root_dir)'],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'CLANG_CXX_LIBRARY': 'libc++',
        'MACOSX_DEPLOYMENT_TARGET': '10.7',
        'OTHER_CFLAGS': [
          "-std=c++17",
        ]
      },
      'msvs_settings': {
        'VCCLCompilerTool': { 'ExceptionHandling': 1 },
      }
    }
  ]
}