cd ..
:Sign the EXE
call app_info_setup.bat

SET ZIP_FILE_NAME=RTSlideShow_Win64
SET APP_BUILD_DIR=dist
SET TEMP_DIR=SlideShow

REM Delete previous files
del bin\%ZIP_FILE_NAME%.zip
del %APP_BUILD_DIR%\RTSlideShow.exe
del %APP_BUILD_DIR%\%ZIP_FILE_NAME%.zip
del %APP_BUILD_DIR%\save.dat
del %APP_BUILD_DIR%\log.txt

REM Copy the executable into the dist directory
copy "bin\RTSlideShow_Release GL_x64.exe" %APP_BUILD_DIR%\RTSlideShow.exe

echo Signing time
call %RT_PROJECTS%\Signing\sign.bat %APP_BUILD_DIR%\RTSlideShow.exe "RTSlideShow"

echo Waiting 4 seconds because sometimes stuff doesn't work if I don't
timeout 4


REM Remove the temporary directory before zipping.  For safety, I don't use %TEMP_DIR% with rmdir
rmdir /S /Q SlideShow

echo Zipping time
REM Create a temporary directory with the desired name inside the zip
mkdir %TEMP_DIR%
REM Copy the contents of the dist directory to the temporary directory
xcopy /E /I %APP_BUILD_DIR%\* %TEMP_DIR%\

REM Zip the temporary directory
%PROTON_DIR%\shared\win\utils\7za.exe a -r -tzip %ZIP_FILE_NAME%.zip %TEMP_DIR%


cd script
pause