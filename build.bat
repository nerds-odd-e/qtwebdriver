rem @ECHO OFF

python generate_wdversion.py

rem gyp --depth . -G output_dir=. -D platform=desktop -D mode=release --generator-output=out/ wd.gyp
rd /S /Q build out
gyp --depth . -f msvs -G msvs_version=2015 -G output_dir=. -D platform=desktop -D mode=debug --generator-output=out/ wd.gyp
devenv .\out\wd.sln /rebuild "Default|x64"

rd /S /Q build out-rel
gyp --depth . -f msvs -G msvs_version=2015 -G output_dir=. -D platform=desktop -D mode=release --generator-output=out-rel/ wd.gyp
devenv .\out-rel\wd.sln /rebuild "Default|x64"
