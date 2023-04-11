# Check Recorded Frequency

<!--* freshness: { owner: 'elow' reviewed: '2023-04-11' } *-->

Check Recorded Frequency compares the recorded audio file against a golden
 frequencies. Most useful in tests, the script will perform frequency analysis
 of the audio sink and compare against frequencies the source had played.

[TOC]

## Prerequisites
Make sure these packages are included for testing.
```shell
pip install numpy six
```
**NOTE:** ChromeOS has already had these packages.

This scripts depends on `audio_data.py`, `audio_analysis.py` and
`audio_quality_measurement.py`

## Build

### Build in chroot
```
(inside the cros chroot) emerge-${BOARD} audiotest
```
The scripts will be in `/build/${BOARD}/usr/bin/*.py`.

### Deploy onto DUT
```
cros deploy ${DUT_IP} audiotest
```
The scripts will be in `/usr/bin/*.py`.

## Usage
```bash
check_recorded_frequency.py [-h]
  -g GOLDEN_FREQUENCIES [GOLDEN_FREQUENCIES ...]
  -t TEST_FILE
  [-f {S32_LE,S16_LE}] [-c CHANNEL] [-r RATE]
  -m CHANNEL_MAP [CHANNEL_MAP ...]
  ...OTHER_FLAGS...
```

**NOTE:**  If the requirements are satistifed, this script can run on
workstation, DUT or Chameleon.

### Important Options
+ `-h`, `--help`
	+ show this help message and exit
+ `-g GOLDEN_FREQUENCIES [GOLDEN_FREQUENCIES ...]`, </br>
  `--golden_frequencies GOLDEN_FREQUENCIES [GOLDEN_FREQUENCIES ...]`
    + A list of float that describe the frequency of each
      channel. Currently only one frequency is supported in
      for each channel.
+ `-t TEST_FILE`, `--test_file TEST_FILE`
    + Path to test audio file, the recorded audio.
+ `-f {S32_LE,S16_LE}`, `--sample_format {S32_LE,S16_LE}`
    + Format of **test file**. (default: S16_LE)
+ `-c CHANNEL`, `--channel CHANNEL`
    + Number of channels in the **test file**. (default: 2)
+ `-r RATE`, `--rate RATE`
    + Rate of of the **test file**. Usually 48000 or 44100. (default: 48000)
+ `-m CHANNEL_MAP [CHANNEL_MAP ...]`, `--channel_map CHANNEL_MAP [CHANNEL_MAP ...]`
    + A list of float, how the test channel maps to the golden channels.
 ie. [1, 0, None, None] means the first channel of test file maps to second
 channel of golden frequency, and vice versa for second channel of test file.

\* for other arguments check the help message.

## Results
Returns the frequencies and magnitude of the frequencies detected. Return
non-zero error code when frequencies in test file mismatch against golden
frequencies.

## Example

Assuming the source of audio is playing [`2000`, `2000`] golden frequencies (`2000`
in first channel, and `2000` is second channel). The recorded audio at
`/tmp/recorded.raw`, with `8` channel, `48000` rate, and `S32_LE` audio format.

If the channel maps between source and sink:
```
source #0 -> sink #0         X -> sink #4
source #1 -> sink #1         X -> sink #5
        X -> sink #2         X -> sink #6
        X -> sink #3         X -> sink #7
```

\* where `X` means no source/sink

The command to call this script will be:
```
> check_recorded_frequency.py -g 2000 2000 -t /tmp/recorded.raw -r 48000 \
  -f S32_LE -c 8 -m 0 1
[(1999.9620982413583, 4.799658508679272e-06), (1999.9620982413583, 4.800869941991319e-06)]
```

The script conclude that first channel has one frequency: `1999.96Hz` at `4.7997`
magnitude; second channel has one frequency: `1999.96Hz` at `4.8009` magnitude.

Since there is no error, the golden frequency and the recorded audio matched.

### Notes on running on workstation
The script also depends on `audio_data.py`, `audio_analysis.py` and
`audio_quality_measurement.py`.

Make sure these scripts can be imported by `check_recorded_frequency.py`.

In particular, if you run outside of `platform/audiotest/script` directory, set `PYTHONPATH` with
`export PYTHONPATH="$PYTHONPATH:~/chromiumos/src/platform/audiotest/script"`

### Converting WAV audio file to RAW audio file
```
sox -t wav audio.wav -t raw -r 48000 -c 8 -e signed -b 32 audio.raw
```


## Source of scripts

The source of scripts are:
- `check_recorded_frequency.py` is modified from `third_party/autotest/files/client/cros/chameleon/audio_test_utils.py`
  - only the function `check_recorded_frequency` and `longest_common_subsequence` is kept.
- `audio_data.py` is unchanged from `third_party/autotest/files/client/cros/audio/audio_data.py`
- `audio_analysis.py` is unchanged from `third_party/autotest/files/client/cros/audio/audio_analysis.py`
- `audio_quality_measurement.py` is unchanged from `third_party/autotest/files/client/cros/audio/audio_quality_measurement.py`

Copied `audio_quality_measurement.py`, `audio_data.py`, and `audio_analysis.py` from `third_party/autotest/files/client/cros/audio/`
```bash
cd ~/chromiumos/src/third_party/autotest/files/client/cros/audio/
cp audio_quality_measurement.py audio_data.py audio_analysis.py  ~/chromiumos/src/platform/audiotest/script/
```

## Problems and Solution

#### `emerge-${BOARD} audiotest` with error: `Local branch for '/mnt/host/source/src/third_party/chromiumos-overlay' is out of sync.`
Sync the ebuild files.
```bash
cd ~/chromiumos/src/third_party/chromiumos-overlay/
git checkout main
```

#### `emerge-${BOARD} audiotest` with error: `/usr/bin/install: omitting directory '/mnt/host/source/src/platform/audiotest/script/__pycache__'`
You might have accidentally run some file on workstation.
```bash
cd ~/chromiumos/src/platform/audiotest/script/
rm -rf __pycache__/
```
