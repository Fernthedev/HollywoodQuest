# Hollywood

The library behind making those magical Quest videos that look like PC

## The story behind this

The idea behind this project came up when we jokingly said we would make a mod that would record videos better than ~~Oculus'~~ Meta's own recording software (which is horrible) which looks and sounds like a video recorded on a phone. (I guess that's what happens when you use a mobile chip.)
Throughout the many months of researching, development and testing we are finally able to show the horrifying code behind it all.

Their recording software has gotten better since the original creation of this library, but so has this library so I think we're still winning.

## How to use

- Add hollywood to dependencies
- Create `CameraRecordingSettings` with desired settings
- Create a Unity camera to use
- Use `SetCameraCapture` and `Init` with the camera and settings to start recording
- Or use `SetAudioCapture` and `OpenFile` with a Unity audio source or listener instead
- Destroy components to finish recording
- Optionally use `MuxFilesSync` to combine a video and audio file after finished
- Hope it worked

## How to compile

- Clone repository
- `qpm restore`
- `python3 ./update_ffmpeg.py`
- `qpm s build`
- Profit

## Credits to

- All the people who made the Quest libraries
- FFMPEG and the libraries behind that
- ffmpeg-kit for compiling FFMPEG libraries on Android
- darknight1050 for the initial AsyncGPUReadbackPlugin code
- xyonico for his awesome help with UnityEngine.Time.captureDeltaTime, OpenGL knowledge and the RGB OpenGL shader
- danike for his advice
- fern and henwill8 for starting the project
