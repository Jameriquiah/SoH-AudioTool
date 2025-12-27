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

This is for Windows only right now. I don't have a mac or linux machine to build and test on.

For Linux users you can run the tool through a VM and use it that way. For Mac users idk.
I will try to get working Linux and Mac builds soon.