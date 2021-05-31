// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "include/binary_client.h"
#include "include/common.h"
#include "include/evaluator.h"
#include "include/generator_player.h"
#include "include/sample_format.h"
#include "include/tone_generators.h"

constexpr static const char* short_options =
    "a:m:d:n:o:w:p:P:f:R:F:r:t:c:C:T:l:g:i:x:hv";

constexpr static const struct option long_options[] = {
    {"active-speaker-channels", 1, NULL, 'a'},
    {"active-mic-channels", 1, NULL, 'm'},
    {"allowed-delay", 1, NULL, 'd'},
    {"fft-size", 1, NULL, 'n'},
    {"power-threshold", 1, NULL, 'p'},
    {"confidence-threshold", 1, NULL, 'o'},
    {"match-window-size", 1, NULL, 'w'},
    {"player-command", 1, NULL, 'P'},
    {"player-fifo", 1, NULL, 'f'},
    {"recorder-command", 1, NULL, 'R'},
    {"recorder-fifo", 1, NULL, 'F'},
    {"sample-rate", 1, NULL, 'r'},
    {"sample-format", 1, NULL, 't'},
    {"num-mic-channels", 1, NULL, 'c'},
    {"num-speaker-channels", 1, NULL, 'C'},
    {"test-rounds", 1, NULL, 'T'},
    {"tone-length", 1, NULL, 'l'},
    {"volume-gain", 1, NULL, 'g'},
    {"min-frequency", 1, NULL, 'i'},
    {"max-frequency", 1, NULL, 'x'},

    // Other helper args.
    {"help", 0, NULL, 'h'},
    {"verbose", 0, NULL, 'v'},
};

// Parse the sample format. The input should be one of the string in
// SampleFormat:Type.
SampleFormat ParseSampleFormat(const char *arg) {
  SampleFormat sample_format;
  for (int format = SampleFormat::kPcmU8;
       format != SampleFormat::kPcmInvalid;
       format++) {
    sample_format = SampleFormat(SampleFormat::Type(format));
    if (strcmp(sample_format.to_string(), optarg) == 0) {
      return sample_format;
    }
  }
  fprintf(stderr, "Unknown sample format %s, using S16 instead.", arg);
  return SampleFormat(SampleFormat::kPcmS16);
}

bool ParseOptions(int argc, char *const argv[], AudioFunTestConfig *config) {
  int opt = 0;
  int optindex = -1;

  while ((opt = getopt_long(argc, argv, short_options,
                            long_options,
                            &optindex)) != -1) {
    switch (opt) {
      case 'a':
        ParseActiveChannels(optarg, &(config->active_speaker_channels));
        break;
      case 'm':
        ParseActiveChannels(optarg, &(config->active_mic_channels));
        break;
      case 'd':
        config->allowed_delay_sec = atof(optarg);
        break;
      case 'n':
        config->fft_size = atoi(optarg);
        if ((config->fft_size == 0) ||
            (config->fft_size & (config->fft_size - 1))) {
          fprintf(stderr, "FFT size needs to be positive & power of 2\n");
          return false;
        }
        break;
      case 'o':
        config->confidence_threshold = atof(optarg);
        break;
      case 'w':
        config->match_window_size = atoi(optarg);
        if (config->match_window_size % 2 == 0) {
          fprintf(stderr, "Match window size must be an odd value.\n");
          return false;
        }
        break;
      case 'p':
        config->power_threshold = atof(optarg);
        break;
      case 'P':
        config->player_command = std::string(optarg);
        break;
      case 'f':
        config->player_fifo = std::string(optarg);
        break;
      case 'R':
        config->recorder_command = std::string(optarg);
        break;
      case 'F':
        config->recorder_fifo = std::string(optarg);
        break;
      case 'r':
        config->sample_rate = atoi(optarg);
        break;
      case 't':
        config->sample_format = ParseSampleFormat(optarg);
        break;
      case 'c':
        config->num_mic_channels = atoi(optarg);
        break;
      case 'C':
        config->num_speaker_channels = atoi(optarg);
        break;
      case 'T':
        config->test_rounds = atoi(optarg);
        break;
      case 'l':
        config->tone_length_sec = atof(optarg);
        // Avoid overly short tones.
        if (config->tone_length_sec < 0.01) {
          fprintf(stderr,
                  "Tone length too short. Must be 0.01s or greater.\n");
          return false;
        }
        break;
      case 'g':
        config->volume_gain = atoi(optarg);
        if (config->volume_gain < 0 || config->volume_gain > 100) {
          fprintf(stderr, "Value of volume_gain is out of range.\n");
          return false;
        }
        break;
      case 'i':
        config->min_frequency = atoi(optarg);
        break;
      case 'x':
        config->max_frequency = atoi(optarg);
        break;
      case 'v':
        config->verbose = true;
        break;
      case 'h':
        return false;
      default:
        fprintf(stderr, "Unknown arguments %c\n", opt);
        assert(false);
    }
  }

  if (config->player_command.empty()) {
    fprintf(stderr, "player-command is not set.\n");
    return false;
  }

  if (config->recorder_command.empty()) {
    fprintf(stderr, "recorder-command is not set.\n");
    return false;
  }

  if (config->active_speaker_channels.empty()) {
    for (int i = 0; i < config->num_speaker_channels; ++i) {
      config->active_speaker_channels.insert(i);
    }
  }

  if (config->active_mic_channels.empty()) {
    for (int i = 0; i < config->num_mic_channels; ++i) {
      config->active_mic_channels.insert(i);
    }
  }

  if (config->min_frequency > config->max_frequency) {
    fprintf(stderr, "Range error: min_frequency > max_frequency\n");
    return false;
  }

  if (config->min_frequency < 0) {
    fprintf(stderr, "Range error: min_frequency < 0\n");
    return false;
  }
  return true;
}

void PrintUsage(const char *name, FILE *fd = stderr) {
  AudioFunTestConfig default_config;

  fprintf(fd,
          "Usage %s -P <player_command> -R <recorder_command> [options]\n",
          name);
  fprintf(fd,
          "\t-a, --active-speaker-channels:\n"
          "\t\tComma-separated list of speaker channels to play on. "
          "(def all channels)\n");
  fprintf(fd,
          "\t-m, --active-mic-channels:\n"
          "\t\tComma-separated list of mic channels to test. "
          "(def all channels)\n");
  fprintf(fd,
          "\t-d, --allowed-delay:\n"
          "\t\tAllowed latency between player & recorder "
          "(def %.4f).\n", default_config.allowed_delay_sec);
  fprintf(fd,
          "\t-n, --fftsize:\n"
          "\t\tLonger fftsize has more carriers but longer latency."
          " Also, fftsize needs to be power of 2"
          "(def %d)\n", default_config.fft_size);
  fprintf(fd,
          "\t-p, --power-threshold:\n"
          "\t\tThreshold of RMS value to pass evaluation "
          "(def %.4f)\n",
          default_config.power_threshold);
  fprintf(fd,
          "\t-o, --confidence-threshold:\n"
          "\t\tThreshold of accumulated confidence to pass evaluation "
          "(def %.4f)\n", default_config.confidence_threshold);
  fprintf(fd,
          "\t-w, --match-window-size:\n"
          "\t\tNumber of bin to be used for calculating matching confidence. "
          "Should be an odd number."
          "(def %d)\n", default_config.match_window_size);
  fprintf(fd,
          "\t-P, --player-command:\n"
          "\t\tThe command used to play sound.\n");
  fprintf(fd,
          "\t-f, --player-fifo:\n"
          "\t\tThe named pipe used to send wave to the player. If not set, "
          "wave is send to player program via its stdin.\n");
  fprintf(fd,
          "\t-R, --recorder-command:\n"
          "\t\tThe command used to record sound.\n");
  fprintf(fd,
          "\t-F, --recorder-fifo:\n"
          "\t\tThe named pipe used to read recorded wave from the recorder "
          "program. If not set, wave is read from recorder program via "
          "its stdout.\n");
  fprintf(fd,
          "\t-r, --sample-rate:\n"
          "\t\tSample rate of generated wave in HZ "
          "(def %d)\n", default_config.sample_rate);
  fprintf(fd,
          "\t-t, --sample-format:\n"
          "\t\tFormat of recording & playing samples, should be one of u8, "
          "s16, s24, s32."
          "(def %s).\n", default_config.sample_format.to_string());
  fprintf(fd,
          "\t-c, --num-mic-channels:\n"
          "\t\tThe number of microphone channels "
          "(def %d)\n", default_config.num_mic_channels);
  fprintf(fd,
          "\t-C, --num-speaker-channels:\n"
          "\t\tThe number of speaker channels "
          "(def %d)\n", default_config.num_speaker_channels);
  fprintf(fd,
          "\t-T, --test-rounds:\n"
          "\t\tNumber of test rounds "
          "(def %d)\n", default_config.test_rounds);
  fprintf(fd,
          "\t-l, --tone-length:\n"
          "\t\tDecimal value of tone length in secs "
          "(def %.4f)\n", default_config.tone_length_sec);
  fprintf(fd,
          "\t-g, --volume-gain\n"
          "\t\tControl the volume of generated audio frames. The range is from"
          " 0 to 100.\n");
  fprintf(fd,
          "\t-i, --min-frequency:\n"
          "\t\tThe minimum frequency of generated audio frames."
          "(def %d)\n", default_config.min_frequency);
  fprintf(fd,
          "\t-x, --max-frequency\n"
          "\t\tThe maximum frequency of generated audio frames."
          "(def %d)\n", default_config.max_frequency);

  fprintf(fd,
          "\t-v, --verbose: Show debugging information.\n");
  fprintf(fd,
          "\t-h, --help: Show this page.\n");
}

void PrintSet(const std::set<int> &numbers, FILE *fd = stdout) {
  bool first = true;
  for (auto it : numbers) {
    if (!first)
      fprintf(fd, ", ");
    fprintf(fd, "%d", it);
    first = false;
  }
}

void PrintConfig(const AudioFunTestConfig &config, FILE *fd = stdout) {
  fprintf(fd, "Config values.\n");

  fprintf(fd, "\tSpeaker active channels: ");
  PrintSet(config.active_speaker_channels, fd);
  fprintf(fd, "\n");
  fprintf(fd, "\tMic active channels: ");
  PrintSet(config.active_mic_channels, fd);
  fprintf(fd, "\n");
  fprintf(fd, "\tAllowed delay: %.4f(s)\n", config.allowed_delay_sec);
  fprintf(fd, "\tFFT size: %d\n", config.fft_size);
  fprintf(fd, "\tConfidence threshold: %.4f\n", config.confidence_threshold);
  fprintf(fd, "\tMatch window size: %d\n", config.match_window_size);
  fprintf(fd, "\tPlayer parameter: %s\n", config.player_command.c_str());
  fprintf(fd, "\tPlayer FIFO name: %s\n", config.player_fifo.c_str());
  fprintf(fd, "\tRecorder parameter: %s\n", config.recorder_command.c_str());
  fprintf(fd, "\tRecorder FIFO name: %s\n", config.recorder_fifo.c_str());
  fprintf(fd, "\tSample format: %s\n", config.sample_format.to_string());
  fprintf(fd, "\tSample rate: %d\n", config.sample_rate);
  fprintf(fd,
          "\tNumber of Microphone channels: %d\n", config.num_mic_channels);
  fprintf(fd, "\tNumber of Speaker channels: %d\n",
          config.num_speaker_channels);
  fprintf(fd, "\tNumber of test rounds: %d\n", config.test_rounds);
  fprintf(fd, "\tTone length: %.4f(s)\n", config.tone_length_sec);
  fprintf(fd, "\tVolume gain: %d\n", config.volume_gain);
  fprintf(fd, "\tMinimum frequency: %d\n", config.min_frequency);
  fprintf(fd, "\tMaximum frequency: %d\n", config.max_frequency);

  if (config.verbose)
    fprintf(fd, "\t** Verbose **.\n");
}

// Randomly picks an integer from the given range [min, max],
// including both end points.
inline int RandomPick(int min, int max) {
  if (min > max) {
    fprintf(stderr, "Range error: min > max\n");
    assert(false);
  }

  static unsigned int seed = time(NULL) + getpid();
  return (rand_r(&seed) % (max - min + 1)) + min;
}

// Controls the main process of audiofuntest.
void ControlLoop(const AudioFunTestConfig &config,
                 Evaluator *evaluator,
                 PlayClient *player,
                 RecordClient *recorder) {
  const double frequency_resolution =
      static_cast<double>(config.sample_rate) / config.fft_size;
  const int min_bin = config.min_frequency / frequency_resolution;
  const int max_bin = config.max_frequency / frequency_resolution;

  std::vector<int> passes(config.num_mic_channels);
  std::vector<bool> single_round_pass(config.num_mic_channels);

  size_t buf_size = config.fft_size * config.num_speaker_channels *
      config.sample_format.bytes();
  SineWaveGenerator generator(
      config.sample_rate,
      config.tone_length_sec,
      config.volume_gain);
  GeneratorPlayer generatorPlayer(
      buf_size,
      config.num_speaker_channels,
      config.active_speaker_channels,
      config.sample_format,
      player);

  for (int round = 1; round <= config.test_rounds; ++round) {
    std::fill(single_round_pass.begin(), single_round_pass.end(), false);
    int bin = RandomPick(min_bin, max_bin);
    double frequency = bin * frequency_resolution;

    generator.Reset(frequency);
    generatorPlayer.Play(&generator);

    evaluator->Evaluate(bin, recorder, &single_round_pass);
    for (int chn = 0; chn < config.num_mic_channels; ++chn) {
      if (single_round_pass[chn]) {
        ++passes[chn];
      }
    }
    generatorPlayer.Stop();

    printf("carrier = %d\n", bin);
    for (auto c : config.active_mic_channels) {
      const char *res = single_round_pass[c] ? "O" : "X";
      printf("%s: channel = %d, success = %d, fail = %d, rate = %.4f\n",
             res, c, passes[c], round - passes[c], 100.0 * passes[c] / round);
    }
  }
}

int main(int argc, char *argv[]) {
  // Parses configuration.ParamConfig
  AudioFunTestConfig config;
  if (!ParseOptions(argc, argv, &config)) {
    PrintUsage(argv[0]);
    return 1;
  }

  PrintConfig(config);

  PlayClient player(config);
  player.Start();

  RecordClient recorder(config);
  recorder.Start();

  Evaluator evaluator(config);

  // Starts evaluation.
  ControlLoop(config, &evaluator, &player, &recorder);

  // Terminates and cleans up.
  recorder.Terminate();
  player.Terminate();

  return 0;
}
