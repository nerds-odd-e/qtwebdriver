rem @ECHO OFF

rd /S /Q out

python generate_wdversion.py

rem gyp --depth . -G output_dir=. -D platform=desktop -D mode=release --generator-output=out/ wd.gyp
gyp --depth . -f msvs -G msvs_version=2015 -G output_dir=. -D platform=desktop -D mode=debug --generator-output=out/ wd.gyp

devenv .\out\wd.sln /rebuild "Default|x64"
