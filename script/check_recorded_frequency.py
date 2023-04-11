#!/usr/bin/env python3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compare test and golden audio file with spectral frequency analysis."""

import argparse
from collections import namedtuple
import logging
from pathlib import Path
import pprint
import sys
from typing import List


sys.path.append(Path(__file__).parent)

import audio_analysis
import audio_data
import audio_quality_measurement


# data structures
DataFormat = namedtuple("DataFormat", ["sample_format", "channel", "rate"])

# The second dominant frequency should have energy less than -26dB of the
# first dominant frequency in the spectrum.
DEFAULT_SECOND_PEAK_RATIO = 0.05

# maximum tolerant noise level
DEFAULT_TOLERANT_NOISE_LEVEL = 0.01

# If relative error of two durations is less than 0.2,
# they will be considered equivalent.
DEFAULT_EQUIVALENT_THRESHOLD = 0.2

# The frequency at lower than _DC_FREQ_THRESHOLD should have coefficient
# smaller than _DC_COEFF_THRESHOLD.
_DC_FREQ_THRESHOLD = 0.001
_DC_COEFF_THRESHOLD = 0.01

# The deviation of estimated dominant frequency from golden frequency.
DEFAULT_FREQUENCY_DIFF_THRESHOLD = 5


DESCRIPTION = """Use spectral analysis to compare test and golden audio file."""

CHANNEL_MAP_DESCRIPTION = """
A list of float, how the test channel maps to the golden channels.
 ie. [1, 0, None, None] means the first channel of test file maps to second
 channel of golden frequency, and vice versa for second channel of test file.
"""

SECOND_PEAK_RATIO_DESCRIPTION = """
A float between 0 and 1. The test fails when the second dominant frequency has
 coefficient larger than this ratio of the coefficient of first dominant
 frequency.
"""

FREQUENCY_DIFF_THRESHOLD_DESCRIPTION = """
The deviation of estimated dominant frequency from golden frequency. The maximum
 difference between estimated frequency of test signal and golden frequency.
 This value should be small for signal passed through line.
"""

IGNORE_FREQUENCIES_DESCRIPTION = """
A list of frequencies to be ignored. The component in the spectral with
 frequency too close to the frequency in the list will be ignored. The
 comparison of frequencies uses frequency_diff_threshold as well.
"""


def longest_common_subsequence(list1, list2, equivalent_threshold):
    """Finds longest common subsequence of list1 and list2

    Such as list1: [0.3, 0.4],
            list2: [0.001, 0.299, 0.002, 0.401, 0.001]
            equivalent_threshold: 0.001
    it will return matched1: [True, True],
                   matched2: [False, True, False, True, False]

    Args:
        list1: a list of integer or float value
        list2: a list of integer or float value
        equivalent_threshold: two values are considered equivalent if their
                              relative error is less than equivalent_threshold.

    Returns:
        a tuple of list (matched_1, matched_2) indicating each item of list1 and
        list2 are matched or not.
    """
    length1, length2 = len(list1), len(list2)
    matching = [[0] * (length2 + 1)] * (length1 + 1)
    # matching[i][j] is the maximum number of matched pairs for first i items
    # in list1 and first j items in list2.
    for i in range(length1):
        for j in range(length2):
            # Maximum matched pairs may be obtained without
            # i-th item in list1 or without j-th item in list2
            matching[i + 1][j + 1] = max(matching[i + 1][j], matching[i][j + 1])
            diff = abs(list1[i] - list2[j])
            relative_error = diff / list1[i]
            # If i-th item in list1 can be matched to j-th item in list2
            if relative_error < equivalent_threshold:
                matching[i + 1][j + 1] = matching[i][j] + 1

    # Backtracking which item in list1 and list2 are matched
    matched1 = [False] * length1
    matched2 = [False] * length2
    i, j = length1, length2
    while i > 0 and j > 0:
        # Maximum number is obtained by matching i-th item in list1
        # and j-th one in list2.
        if matching[i][j] == matching[i - 1][j - 1] + 1:
            matched1[i - 1] = True
            matched2[j - 1] = True
            i, j = i - 1, j - 1
        elif matching[i][j] == matching[i - 1][j]:
            i -= 1
        else:
            j -= 1
    return (matched1, matched2)


def check_recorded_frequency(
    golden_frequencies: List[float],
    test_file: str,
    test_data_format: DataFormat,
    channel_map: List[int],
    second_peak_ratio: float = DEFAULT_SECOND_PEAK_RATIO,
    frequency_diff_threshold=DEFAULT_FREQUENCY_DIFF_THRESHOLD,
    ignore_frequencies=None,
    check_anomaly=False,
    check_artifacts=False,
    mute_durations=None,
    volume_changes=None,
    tolerant_noise_level=DEFAULT_TOLERANT_NOISE_LEVEL,
):
    """Checks if the recorded data contains sine tone of golden frequency.

    Args:
        golden_frequencies: A list of float that describe the frequency of each
                            channel. Currently only one frequency is supported
                            for each channel.
        test_file: A raw audio file, used for testing.
        test_data_format: A DataFormat to describe test_file's data type.
        channel_map: A list of float, check CHANNEL_MAP_DESCRIPTION for more.
        second_peak_ratio: A float between 0 and 1, check
                           SECOND_PEAK_RATIO_DESCRIPTION for more.
        frequency_diff_threshold: A float, the deviation of estimated dominant
                                  frequency from golden frequency. Check
                                  FREQUENCY_DIFF_THRESHOLD_DESCRIPTION for more.
        ignore_frequencies: A list of frequencies to be ignored, check
                            IGNORE_FREQUENCIES_DESCRIPTION for more.
        check_anomaly: True to check anomaly in the signal.
        check_artifacts: True to check artifacts in the signal.
        mute_durations: Each duration of mute in seconds in the signal.
        volume_changes: A list containing alternative -1 for decreasing
                        volume and +1 for increasing volume.
        tolerant_noise_level: The maximum noise level can be tolerated

    Returns:
        A list containing tuples of (dominant_frequency, coefficient) for
        valid channels. Coefficient can be a measure of signal magnitude
        on that dominant frequency. Invalid channels where golden_channel
        is None are ignored.

    Raises:
        error.TestFail if the recorded data does not contain sine tone of
            golden frequency.
    """
    if not ignore_frequencies:
        ignore_frequencies = []

    # Also ignore harmonics of ignore frequencies.
    ignore_frequencies_harmonics = []
    for ignore_freq in ignore_frequencies:
        ignore_frequencies_harmonics += [ignore_freq * n for n in range(1, 4)]

    data_format = test_data_format._asdict()

    with open(test_file, "rb") as f:
        test_data_binary = f.read()

    recorded_data = audio_data.AudioRawData(
        binary=test_data_binary,
        channel=data_format["channel"],
        sample_format=data_format["sample_format"],
    )

    errors = []
    dominant_spectrals = []

    for test_channel, golden_channel in enumerate(channel_map):
        if golden_channel is None:
            logging.info("Skipped channel %d", test_channel)
            continue

        signal = recorded_data.channel_data[test_channel]
        saturate_value = audio_data.get_maximum_value_from_sample_format(
            data_format["sample_format"]
        )
        logging.debug("Channel %d max signal: %f", test_channel, max(signal))
        normalized_signal = audio_analysis.normalize_signal(
            signal, saturate_value
        )
        logging.debug("saturate_value: %f", saturate_value)
        logging.debug("max signal after normalized: %f", max(normalized_signal))
        spectral = audio_analysis.spectral_analysis(
            normalized_signal, data_format["rate"]
        )
        logging.debug("spectral: %s", spectral)

        if not spectral:
            errors.append(
                "Channel %d: Can not find dominant frequency." % test_channel
            )

        golden_frequency = golden_frequencies[golden_channel]
        logging.debug(
            "Checking channel %s spectral %s against frequency %s",
            test_channel,
            spectral,
            golden_frequency,
        )

        def should_be_ignored(frequency, ignore_frequencies):
            """Checks if frequency is close to any frequency in ignore list.

            The ignore list is harmonics of frequency to be ignored
            (like power noise), plus harmonics of dominant frequencies,
            plus DC.

            Args:
                frequency: The frequency to be tested.
                ignore_frequencies: The frequency that needs to be ignored.

            Returns:
                True if the frequency should be ignored. False otherwise.
            """
            for ignore_frequency in ignore_frequencies:
                if abs(frequency - ignore_frequency) < frequency_diff_threshold:
                    logging.debug("Ignore frequency: %s", frequency)
                    return True

        # Filter out the frequencies to be ignored.
        spectral_post_ignore = [
            x
            for x in spectral
            if not should_be_ignored(x[0], ignore_frequencies_harmonics + [0.0])
        ]

        if not spectral_post_ignore:
            errors.append(
                "Channel %d: No frequency left after removing unwanted "
                "frequencies. Spectral: %s; After removing unwanted "
                "frequencies: %s"
                % (test_channel, spectral, spectral_post_ignore)
            )
            continue

        dominant_frequency = spectral_post_ignore[0][0]

        if (
            abs(dominant_frequency - golden_frequency)
            > frequency_diff_threshold
        ):
            errors.append(
                "Channel %d: Dominant frequency %s is away from golden %s"
                % (test_channel, dominant_frequency, golden_frequency)
            )

        if check_anomaly:
            detected_anomaly = audio_analysis.anomaly_detection(
                signal=normalized_signal,
                rate=data_format["rate"],
                freq=golden_frequency,
            )
            if detected_anomaly:
                errors.append(
                    "Channel %d: Detect anomaly near these time: %s"
                    % (test_channel, detected_anomaly)
                )
            else:
                logging.info(
                    "Channel %d: Quality is good as there is no anomaly",
                    test_channel,
                )

        if check_artifacts or mute_durations or volume_changes:
            result = audio_quality_measurement.quality_measurement(
                normalized_signal,
                data_format["rate"],
                dominant_frequency=dominant_frequency,
            )
            logging.debug(
                "Quality measurement result:\n%s", pprint.pformat(result)
            )
            if check_artifacts:
                if len(result["artifacts"]["noise_before_playback"]) > 0:
                    errors.append(
                        "Channel %d: Detects artifacts before playing near"
                        " these time and duration: %s"
                        % (
                            test_channel,
                            str(result["artifacts"]["noise_before_playback"]),
                        )
                    )

                if len(result["artifacts"]["noise_after_playback"]) > 0:
                    errors.append(
                        "Channel %d: Detects artifacts after playing near"
                        " these time and duration: %s"
                        % (
                            test_channel,
                            str(result["artifacts"]["noise_after_playback"]),
                        )
                    )

            if mute_durations:
                delays = result["artifacts"]["delay_during_playback"]
                delay_durations = []
                for x in delays:
                    delay_durations.append(x[1])
                mute_matched, delay_matched = longest_common_subsequence(
                    mute_durations,
                    delay_durations,
                    DEFAULT_EQUIVALENT_THRESHOLD,
                )

                # updated delay list
                new_delays = [
                    delays[i] for i in delay_matched if not delay_matched[i]
                ]

                result["artifacts"]["delay_during_playback"] = new_delays

                unmatched_mutes = [
                    mute_durations[i]
                    for i in mute_matched
                    if not mute_matched[i]
                ]

                if len(unmatched_mutes) > 0:
                    errors.append(
                        "Channel %d: Unmatched mute duration: %s"
                        % (test_channel, unmatched_mutes)
                    )

            if check_artifacts:
                if len(result["artifacts"]["delay_during_playback"]) > 0:
                    errors.append(
                        "Channel %d: Detects delay during playing near"
                        " these time and duration: %s"
                        % (
                            test_channel,
                            result["artifacts"]["delay_during_playback"],
                        )
                    )

                if len(result["artifacts"]["burst_during_playback"]) > 0:
                    errors.append(
                        "Channel %d: Detects burst/pop near these time: %s"
                        % (
                            test_channel,
                            result["artifacts"]["burst_during_playback"],
                        )
                    )

                if result["equivalent_noise_level"] > tolerant_noise_level:
                    errors.append(
                        "Channel %d: noise level is higher than tolerant"
                        " noise level: %f > %f"
                        % (
                            test_channel,
                            result["equivalent_noise_level"],
                            tolerant_noise_level,
                        )
                    )

            if volume_changes:
                matched = True
                volume_changing = result["volume_changes"]
                if len(volume_changing) != len(volume_changes):
                    matched = False
                else:
                    for i in range(len(volume_changing)):
                        if volume_changing[i][1] != volume_changes[i]:
                            matched = False
                            break
                if not matched:
                    errors.append(
                        "Channel %d: volume changing is not as expected, "
                        "found changing time and events are: %s while "
                        "expected changing events are %s"
                        % (test_channel, volume_changing, volume_changes)
                    )

        # Checks DC is small enough.
        for freq, coeff in spectral_post_ignore:
            if freq < _DC_FREQ_THRESHOLD and coeff > _DC_COEFF_THRESHOLD:
                errors.append(
                    "Channel %d: Found large DC coefficient: "
                    "(%f Hz, %f)" % (test_channel, freq, coeff)
                )

        # Filter out the harmonics resulted from imperfect sin wave.
        # This list is different for different channels.
        harmonics = [dominant_frequency * n for n in range(2, 10)]

        spectral_post_ignore = [
            x
            for x in spectral_post_ignore
            if not should_be_ignored(x[0], harmonics)
        ]

        if len(spectral_post_ignore) > 1:
            first_coeff = spectral_post_ignore[0][1]
            second_coeff = spectral_post_ignore[1][1]
            if second_coeff > first_coeff * second_peak_ratio:
                errors.append(
                    "Channel %d: Found large second dominant frequencies: "
                    "%s" % (test_channel, spectral_post_ignore)
                )

        if not spectral_post_ignore:
            errors.append(
                "Channel %d: No frequency left after removing unwanted "
                "frequencies. Spectral: %s; After removing unwanted "
                "frequencies: %s"
                % (test_channel, spectral, spectral_post_ignore)
            )

        else:
            dominant_spectrals.append(spectral_post_ignore[0])

    if errors:
        raise ValueError(", ".join(errors))

    return dominant_spectrals


def main():
    parser = argparse.ArgumentParser(
        description=DESCRIPTION,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-g",
        "--golden_frequencies",
        type=float,
        nargs="+",
        required=True,
        help="A list of float that describe the frequency of each channel. "
        "Currently only one frequency is supported for each channel.",
    )

    parser.add_argument(
        "-t",
        "--test_file",
        type=str,
        required=True,
        help="Path to test audio file only raw format is accepted, the recorded"
        " audio.",
    )
    parser.add_argument(
        "-f",
        "--sample_format",
        type=str,
        default="S16_LE",
        choices=list(audio_data.SAMPLE_FORMATS.keys()),
        help="Format of test file. (default: %(default)s)",
    )
    parser.add_argument(
        "-c",
        "--channel",
        type=int,
        default=2,
        help="Number of channels in the test file. (default: %(default)s)",
    )
    parser.add_argument(
        "-r",
        "--rate",
        type=float,
        default=48000,
        help="Rate of of the test file. Usually 48000 or 44100. (default: %(default)s)",
    )

    parser.add_argument(
        "-m",
        "--channel_map",
        type=int,
        nargs="+",
        required=True,
        help=CHANNEL_MAP_DESCRIPTION,
    )

    parser.add_argument(
        "--second_peak_ratio",
        type=float,
        default=DEFAULT_SECOND_PEAK_RATIO,
        help=SECOND_PEAK_RATIO_DESCRIPTION,
    )

    parser.add_argument(
        "--frequency_diff_threshold",
        type=float,
        default=DEFAULT_FREQUENCY_DIFF_THRESHOLD,
        help=FREQUENCY_DIFF_THRESHOLD_DESCRIPTION,
    )

    parser.add_argument(
        "--ignore_frequencies",
        type=float,
        nargs="+",
        help=IGNORE_FREQUENCIES_DESCRIPTION,
    )

    parser.add_argument(
        "--check_anomaly",
        type=bool,
        default=False,
        help="True to check anomaly in the signal.",
    )

    parser.add_argument(
        "--check_artifacts",
        type=bool,
        default=False,
        help="True to check artifacts in the signal.",
    )

    parser.add_argument(
        "--mute_durations",
        type=float,
        nargs="+",
        default=None,
        help="Each duration of mute in seconds in the signal.",
    )

    parser.add_argument(
        "--volume_changes",
        type=int,
        nargs="+",
        default=None,
        help="A list containing alternative -1 for decreasing volume and +1"
        "for increasing volume.",
    )

    parser.add_argument(
        "--tolerant_noise_level",
        type=float,
        default=DEFAULT_TOLERANT_NOISE_LEVEL,
        help="The maximum noise level can be tolerated",
    )

    args = parser.parse_args()

    print(
        check_recorded_frequency(
            args.golden_frequencies,
            args.test_file,
            DataFormat(
                sample_format=args.sample_format,
                channel=args.channel,
                rate=args.rate,
            ),
            args.channel_map,
            second_peak_ratio=args.second_peak_ratio,
            frequency_diff_threshold=args.frequency_diff_threshold,
            ignore_frequencies=args.ignore_frequencies,
            check_anomaly=args.check_anomaly,
            check_artifacts=args.check_artifacts,
            mute_durations=args.mute_durations,
            volume_changes=args.volume_changes,
            tolerant_noise_level=args.tolerant_noise_level,
        )
    )


if __name__ == "__main__":
    main()
