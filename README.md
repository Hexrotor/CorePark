# CorePark

A simple tool to implement core parking for Linux

**WARNING: I'M NOT A PROFESSIONAL PROGRAMMER, THIS PROGRAM MAY DAMAGE YOUR SYSTEM, PLEASE BE CAREFUL.**

## For what

Recently I noticed that in Windows 11, the task manager shows that some CPU cores are parked. This is a feature implemented by Microsoft called Core Parking, which dynamically shuts down unused cores to save power.

My PC is a laptop with Intel i7-12700H installed, and I've installed dual-system for it, which are Windows 11 and Linux Mint 21.3(with Cinnamon, kernel 6.8.0-40-generic).

But what confuses me is that when I use Windows 11, it consumes much less power than Linux. People say that Linux kernel is more efficient than Windows, but I feel that Linux consumes much more power and heats up a lot more when I use it with battery.

At first I thought this was caused by the lack of a CoreParking mechanism on Linux, so I programmed this tool. But after testing it for a while, I realised that things don't seem to be that simple.

## Some guess

The tool controls CPU cores by writing 0/1 to '/sys/devices/system/cpu/cpu{ID}/online' to disable or enable cores numbered by ID. Honestly idk if this could stop the power supply to the physical core, because in my tests, the core temperature is still higher than Windows.

At low load, the tool will reduce the working cores to one or two, but the temperature of the cores is not very different from the full-on case, and the temperature is even higher than the full-on case, due to the increase in frequency. After my observations, I found that Windows basically doesn't enable E cores at low loads. Well, with some tweaking, the core count can be kept at a relatively normal amount and the average frequency seems to be able to be lower than Windows, but no matter how much I modify the parameters, I can't get to the power consumption level of Windows.

I think the problem may be related to the Linux GUI, which after all is not as mature as Windows and may cost more CPU. The Other side is software optimisation, when I test Microsoft Edge, the CPU power is higher than Windows. I just stopped on a web page and didn't move, the CPU temperature of Windows could even go down to 45℃, but Linux never went below 55℃. Another possible reason for this is that modifying "online" to control the core may not completely cut off the power. 

This tool doesn't read CPU information directly to make quick adjustments like the kernel governor does; when the load is increased, the governor up the CPU clock first and the load is concentrated in a small number of cores. After 500ms the tool turns on the additional cores, and in addition, migrating tasks on the CPU incurs additional overheads.
