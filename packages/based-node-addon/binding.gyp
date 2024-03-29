{
  'targets': [
    {
      'target_name': 'based-node-addon',
      'sources': [ 'src/based.cc' ],
      'include_dirs': [
          "<(module_root_dir)/based-lib",
          "<!@(node -p \"require('node-addon-api').include\")"
      ],
      'libraries': [
          "-Wl,-rpath,<(module_root_dir)/based-lib",
          '-lbased',
          '-L<(module_root_dir)/based-lib'
      ],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'CLANG_CXX_LIBRARY': 'libc++',
        'MACOSX_DEPLOYMENT_TARGET': '12.6',
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