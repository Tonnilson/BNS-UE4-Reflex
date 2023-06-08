# BNS-UE4-Reflex
 NVIDIA Reflex implementation into Blade & Soul running on UE4 4.25.3

This repo is more of an example of how you could go about modding reflex into a game on Unreal Engine 4.25+

### Other Notes
You can look at the Unreal Engine source code on Github for reference points in how the plugin implements it like I did. Alternatives things you can do is use the GFrameCounter instead of using your own internal frame counter for frameId's and so on. Some things may change depending on the game but I tried to keep the implementation as 1:1 as possible but Blade & Soul is a very unique title when it comes to UE4 games and essentially has its own layer built on over of UE4, using UE4 as mostly just renderer with other base systems where as other games build upon it or extend.

Be mindful of the device, device context and swapchain. When switching window modes (full screen / borderless) things are released and recreated.

If you are attempting this on a different game engine it should at least give you an idea.

### Credits
[Doodlum](https://github.com/doodlum "Doodlum") - His source for Skyrims implementation

mambda on GH for his alternative method for getting the swapchain virtual table
