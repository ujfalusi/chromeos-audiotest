# Alsa Conformance Test

<!--* freshness: { owner: 'yuhsuan' reviewed: '2019-08-19' } *-->

ALSA Conformance Test is a tool to verify the correctness and performance of
audio drivers. It can be used to verify the quality of audio bringup and prevent
regression. The tool has found many audio [issues](#bugs-we-found) on different
devices.

[TOC]

## Prerequisites
Make sure these packages are on the host to compile the tool.
```Shell
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install git pkg-config build-essential libasound2-dev
```

Make sure these packages are on the device under test(DUT) for testing.
```Shell
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install alsa-utils libasound2-dev
```
**NOTE:** ChromeOS has already had these packages.

## Build
The tool is a built-in binary in the ChromeOS test image. There are two ways to
build ALSA conformance test manually.

### Build in the ChromeOS chroot
```
(inside the cros chroot) emerge-${BOARD} audiotest
```
The binary will be in `/build/${BOARD}/usr/bin/alsa_conformance_test`.

The command `cros deploy ${DUT_IP} audiotest` can deploy this tool to a device.
The binary will be in /usr/bin/.

**NOTE:** The built-in binary is in `/usr/local/bin` where the developer root
is mounted at. The location is different from building by yourself. Make sure
that you use the correct binary before testing.

### Build in the git package
```
git clone https://chromium.googlesource.com/chromiumos/platform/audiotest
cd audiotest
make alsa_conformance_test [OUT=/path/to/builddir]
```
The default binary is in ./src. Set variable OUT to change the build directory.

## Run
```
alsa_conformance_test [OPTIONS]
```
Notice that some devices may not work before the correct UCM (Use Case Manager)
is set. In ChromeOS, the UCM should be set automatically when selecting audio
nodes from UI. Users can also find the UCM file for each supported sound card
in `/usr/share/alsa/ucm/` and set it manually by [alsaucm](https://www.systutorials.com/docs/linux/man/1-alsaucm/).

**Example:** To enable the headphone specified on `kbl_r5514_5663_max`.
```
alsaucm -c kbl_r5514_5663_max set _verb HiFi set _enadev "Headphone"
```

### Options
+ -h, --help
	+ Print this help and exit.
+ -P, --playback_dev <device>
	+ PCM device for playback. (default: NULL)
+ -C, --capture_dev <device>
	+ PCM device for capture. (default: NULL)
+ -c, --channels <channels>
	+ Set channels. (default: 2)
+ -f, --format <format>
	+ Set format. (default: S16_LE)
+ -r, --rate <rate>
	+ Set rate. (default: 48000)
+ -p, --period <period>
	+ Set period. If not set, the default value set in the driver will be used.
+ -d, --durations <duration>
	+ Set durations(second). (default: 1.0)
+ -B, --block_size <block_size>
	+ Set block size in frames of each read(for input devices) and
      write(for output devices). (default: 240)
    + For more details, please refer to the section about [Underrun and Overrun](#underrun-and-overrun).
+ --debug
	+ Enable debug mode. (Not support multi-streams in this version)
+ --strict
	+ Enable strict mode. It will set params to the fixed value.
    + ALSA conformance test sets the rate and the period size nearest to the
      target. The final value may not be the same as a user set in the params.
      With this flag, the test will fail if these values are unexpected.
+ --dev_info_only
	+ Show device information only without setting params and running I/O.
+ --iterations
	+ Number of times to run the tests specified. (default: 1)
+ --device_file:
	+ Device file path. It will load devices from the file. File format:
		```
	  	[name] [type] [channels] [format] [rate] [period] [block_size] [durations] # comment
      [type] could be either `PLAYBACK` or `CAPTURE` # comment
      	eg: hw:0,0 PLAYBACK 2 S16_LE 48000 240 240 10 # Example
		```
+ --merge_threshold
	+ Set merge_threshold_t. (default: 0.0001)
	+ If the value is not zero, there will be a dryrun to set the
	  merge_threshold_sz as the median of frame diff. Points with
	  TIME_DIFF less than merge_threshold_t and SAMPLES_DIFF less
	  than merge_threshold_sz will be merged.

## Results
These are the functions that ALSA conformance test covers.

### Correctness of params
Sometimes audio device claims that it can support some params but it actually
does not. In this case, snd_pcm_hw_params fails. Therefore, verifying all
params can be set correctly is necessary.
```
---------PRINT PARAMS---------
PCM name: hw:0,0
card: acpd7219m98357 [acpd7219m98357]
device: Playback da7219-hifi-0 []
stream: PLAYBACK
access type: MMAP_INTERLEAVED
format: S16_LE
channels: 2
rate: 48000 fps
period time: 5000 us
period size: 240 frames
buffer time: 160000 us
buffer size: 7680 frames
```

### Stability of rate
The number of samples consumed / time must be the same as the sampling rate.
For short-term, it can be checked every time when writing a block into the
device by calculating samples_diff / time_diff.
```
TIME_DIFF(s)    HW_LEVEL       READ               RATE
0.000780757           32          0       40985.863719
0.000791478           72          0       50538.359879
0.000834111          112          0       47955.248162
0.000838265          152          0       47717.607201
```

For long-term, the tool can record all data when hw_level changes. And then,
it will use linear regression to find out the measured rate. If measured rate
is not close to the sampling rate, or standard error from linear regression is
too large, the device doesn’t have enough stability of rate.

```
rate: 48001.729135
rate error: 7.050352
```

The [script](#script) only uses the long-term rate to determine pass or fail
because an unstable short-term rate may not affect the whole stability.


**Explaination of fields**

+ HW_LEVEL - Hardware level. The number of frames in a device buffer.
+ READ - The cumulative number of frames has been read since the beginning.

**Explaination of point merge**
```
TIME_DIFF(s)    HW_LEVEL              PLAYED           DIFF               RATE
0.001000772          376                1064             48        47962.972585
0.001000089          328                1112             48        47995.728380
0.001000082          280                1160             48        47996.064323
0.001016092          232                1208             48        47239.816867
0.000983772          424                1256             48        48791.793220
0.000998709          400                1280             24        24031.024052
0.000082259          376                1304             24        291761.387812 [Merged]
```

In the last two lines above, the device consumes 24 samples twice in a short time
instead of 48 samples in the fixed period and result in higher rate error.
To reduce the error, points with TIME_DIFF less than merge_threshold_t and SAMPLES_DIFF less than merge_threshold_sz will be merged. The merge_threshold_sz is determined automatically by the test and merge_threshold_t can be set by --merge_threshold option.
With the --merge_threshold set to 0.0001, the last two points will be merged, and only
the latter point is counted in linear regression. The result of point merge is the same as below:
```
TIME_DIFF(s)    HW_LEVEL              PLAYED           DIFF               RATE
0.001000772          376                1064             48        47962.972585
0.001000089          328                1112             48        47995.728380
0.001000082          280                1160             48        47996.064323
0.001016092          232                1208             48        47239.816867
0.000983772          424                1256             48        48791.793220
0.001080968          376                1304             48        44404.6447258
```


### Stability of step
If a device consumes too large a block at once, we cannot maintain the buffer
level at low. This affects continuous latency. To measure this, the tool can
monitor hw_ptr and see whether the largest step during monitoring is small
enough.

```
step min: 8
step max: 288
```

### Runtime of each ALSA API
The runtime of APIs is also important. If it needs a long time to open PCM
device, it might affect stream efficiency or cause long latency.
```
---------TIMER RESULT---------
                                 Total_time(s)               Counts          Averages(s)
snd_pcm_open                       0.003351390                    1             0.003351
snd_pcm_hw_params                  0.039304997                    1             0.039305
snd_pcm_hw_params_any              0.000055113                    1             0.000055
snd_pcm_sw_params                  0.000017586                    1             0.000018
snd_pcm_prepare                    0.000007613                    1             0.000008
snd_pcm_start                      0.000774880                    1             0.000775
snd_pcm_avail                      0.807074662               246177             0.000003
precision: 0.000000001
```

### Correctness of recorded samples
Without any help from users, we can verify the correctness of recorded samples
by checking whether all of them are zeros. The tool shows whether each channel
is a zero channel, which means all samples in this channel are zeros. Usually
the recorded samples should not be all zeros.
```
zero channels: 0 1 // Channel 0 is not a zero channel but channel 1 is.
```

### Underrun and Overrun
The basic request of the audio device is not causing any underrun or overrun.
+ Underrun happens when there are no frames to playback on a running device.
  It makes the device play obsolete samples in the buffer, which sounds like
  glitches.

+ Overrun happens when frames in a device buffer are more than buffer size.
  It makes the device have a long delay and the latest recorded samples will
  overlay the old ones.

The block size is the size ALSA conformance test puts data into or gets data
from a device buffer. For playback, it puts data into the device once the
remaining frames in the device buffer are smaller than the block size. For
capture, it gets data from a device once the remaining frames in the device
buffer are larger than the block size. Users can adjust the block size to
detect whether this size might cause underrun or overrun.

```
number of underrun: 0
number of overrun: 0
```

### Example
These are some examples to show how to use ALSA conformance test.

+ Get the PCM name
Before testing, to get the PCM name of the device is necessary. The name format
is `hw:{CARD},{DEVICE}`. Users can get it by the command `aplay -l` and
`arecord -l`.
```Shell
> aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: sofglkda7219max [sof-glkda7219max], device 0: Speakers (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: sofglkda7219max [sof-glkda7219max], device 1: Headset (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: sofglkda7219max [sof-glkda7219max], device 5: HDMI1 (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: sofglkda7219max [sof-glkda7219max], device 6: HDMI2 (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: sofglkda7219max [sof-glkda7219max], device 7: HDMI3 (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```
From this result, the PCM name for internal speaker is `hw:0,0` and for
headset is `hw:0,1`. We will use `hw:0,0` as a playback and `hw:0,1` as capture
device for the below examples.

+ Show the range of parameters of PCM device
```
alsa_conformance_test -P hw:0,0 --dev_info_only
```
+ Run playback and get the result
```
alsa_conformance_test -P hw:0,0
```
+ Run capture and get the result
```
alsa_conformance_test -C hw:0,1
```
+ Try to change the rate and format
```
alsa_conformance_test -P hw:0,0 -r 44100 -f S32_LE
```
+ Try to show detail of playback
```
alsa_conformance_test -P hw:0,0 -d 0.1 --debug
```
+ Playback and capture at the same time
```
alsa_conformance_test -P hw:0,0 -C hw:0,1
```
+ Test playback for more times
```
alsa_conformance_test -P hw:0,0 --iterations 10
```
+ Playback and capture at the same time with different params
```Shell
> cat ./device
hw:0,0 PLAYBACK 2 S16_LE 48000 240 240 1
hw:0,1 CAPTURE  2 S32_LE 44100 240 240 2
> alsa_conformance_test --device_file ./device
```

## Script
For more convenience, this is a script that can test the basic funtions of
ALSA PCM device automatically.
```
alsa_conformance_test.py [-h] [-C INPUT_DEVICE] [-P OUTPUT_DEVICE]
                         [--rate-criteria-diff-pct RATE_CRITERIA_DIFF_PCT]
                         [--rate-err-criteria RATE_ERR_CRITERIA]
                         [--json] [--log-file LOG_FILE]
                         [--test-suites  [...]]
```

### Options
+ -h, --help
	+ show this help message and exit
+ -C INPUT_DEVICE, --input_device INPUT_DEVICE
    + Alsa input device, such as hw:0,0
+ -P OUTPUT_DEVICE, --output_device OUTPUT_DEVICE
    + Alsa output device, such as hw:0,0
+ --rate_criteria RATE_CRITERIA
	+ The pass criteria of rate. The value is a percentage of rate. For example,
      0.01 means the pass range is [47995.2, 48004.8] for rate 48000.
      (default: 0.01)
+ --rate_err_criteria RATE_ERR_CRITERIA
	+ The pass criteria of rate error. (default: 10)
+ --json
	+ Print result in JSON format
+ --log-file LOG_FILE
	+ The file to save logs.
+ --test-suites
    + Customize which test suites should be run. If not set, all suites will
      be run. See next section for more information.

### Test Suites
+ test_params
    + Check whether all parameters can be set correctly.
+ test_rates
    + Check whether all estimated rates are the same as what it set.
+ test_all_pairs
    + Check whether the audio is still stable when mixing different params.
      The test will check if rates meet our expectation when testing all
      combinations of channels, sample rates and formats.

### Results
The result will show pass or fail.

### Example

```
> alsa_conformance_test.py -P hw:0,0
11 passed, 0 failed
Device Information
        Name: hw:0,0
        Card: sof-glkda7219max
        Stream: PLAYBACK
        Format: ['S16_LE', 'S32_LE']
        Channels: [2]
        Rate: [44100, 48000, 96000, 192000]
        Period_size range: [16, 8192]
        Buffer_size range: [32, 16384]
Test Params
        Set channels 2: pass
        Set format S16_LE: pass
        Set format S32_LE: pass
        Set rate 44100: pass
        Set rate 48000: pass
        Set rate 96000: pass
        Set rate 192000: pass
Test Rates
        Set rate 44100: pass
        Set rate 48000: pass
        Set rate 96000: pass
        Set rate 192000: pass
Test All Pairs
        Set channels 2, format S16_LE, rate 44100: pass
        Set channels 2, format S16_LE, rate 48000: pass
        Set channels 2, format S16_LE, rate 96000: pass
        Set channels 2, format S16_LE, rate 192000: pass
        Set channels 2, format S32_LE, rate 44100: pass
        Set channels 2, format S32_LE, rate 48000: pass
        Set channels 2, format S32_LE, rate 96000: pass
        Set channels 2, format S32_LE, rate 192000: pass
```

## Bugs we found
+ Unsupported channel number on Octopus. ([Issue](http://b/122867610), [Fix](http://crrev.com/c/1440921))
+ Incorrect sample rate and params on Octopus. ([Issue](http://b/119390555), [Fix](http://crrev.com/c/1391434))
+ Incorrect sample rate on Veyron HDMI. ([Issue](http://b/122017492), [Fix](http://crrev.com/c/1394465))
+ Incorrect sample rate after suspending on Grunt. ([Issue](http://b/117585955), [Fix](http://crrev.com/i/718908))
+ Incorrect sample rate with period size 256 on strago. ([Issue](http://b/127192730), [Fix](http://crrev.com/c/1505076))
+ Incorrect params supported on Veyron. ([Issue](http://b/123053423), [Fix](http://crrev.com/c/1680381))
+ Incorrect params supported on Peach. ([Issue](http://b/128960223), [Fix](http://crrev.com/c/1680381))
+ Incorrect params supported on Coral. ([Issue](http://b/128955894), [Fix](http://crrev.com/c/1617381))
+ Incorrect sample rate supported on Gru. ([Issue](http://b/128960612), [Fix](http://crrev.com/c/1592502))
+ Incorrect sample rate supported on Scarlet. ([Issue](http://b/128959797), [Fix](http://crrev.com/c/1592502))
+ Read a large block of frames once on Grunt. ([Issue](http://b/138076270), [Fix](http://crrev.com/c/1708817))
+ Abnormal consuming size on Kefka-kernelnext. ([Issue](http://b/136629398), [Fix](http://crrev.com/c/1688675))
+ Unstable sample rate after suspending on Kukui. ([Issue](http://b/130053411), [Fix](http://crrev.com/c/1614344))
+ Incorrect sample rate with period size 256 on Reks. ([Issue](http://b/35565625))
+ Capture is not stable in the beginning on Grunt. ([Issue](http://b/113768831), [Fix](http://crrev.com/c/1228453))
+ Unstable playback rate on Grunt. ([Issue](http://b/112404244), [Fix](http://crrev.com/c/1183010))
+ Incorrect sample rate supported on Grunt. ([Issue](http://b/112402329), [Fix](http://crrev.com/c/1183006))
+ Incorrect sample rate with 1 channel on veyron-minnie. ([Issue](http://crbug.com/987550), [Fix](http://crrev.com/c/1720373))
+ Incorrect sample rate with 3 and 4 channels on samus. ([Issue](http://b/138825612))
+ Incorrect sample rate with 3 channels on buddy. ([Issue](http://b/138825293))
