Converts WAV files into the audio format used for Ship of Harkinian and 2Ship2Harkinian.

## Usage

You just set your output folder. Click Add WAVs and add every WAV you want to convert, then click Convert. That's it.

You still need to make sure the sample rate of your audio matches the sample rate of the audio you are replacing or else your audio will be either slowed down or sped up ingame.
Also the file names obviously need to be the same as the file names of the audio you are replacing. You can either edit the WAV's file name or the output file it doesn't matter.

You can view every sample name and its sample rate on this document here: https://docs.google.com/spreadsheets/u/0/d/1Yf_1Juzj06RZNmuZsWBSSX5ZD7wTRwjf8WxE25-2pJI/htmlview

## Building

Windows: You need Visual Studio 2022 with `Desktop development with C++`
	1. Clone the repo
	2. Clone submodules `git submodule update --init --recursive`
	3. In powershell run msvc patch `scripts/apply-vadpcm-patch.ps1`
	4. In command prompt run `"C:\Program Files\CMake\bin\cmake.exe" -S . -B build -G "Visual Studio 17 2022" -A x64`
	5. Then run `"C:\Program Files\CMake\bin\cmake.exe" --build build --config Release`
	6. Exe will be in `\build\Release`

Tested on Windows and Linux, haven't tested Mac yet since I don't have a Mac to test on but report any issues or crashes if you try it please.
