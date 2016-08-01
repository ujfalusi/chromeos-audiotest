// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/param_config.h"

#include <sstream>
#include <string>

namespace autotest_client {
namespace audio {

// Two function of Command class.
void Command::Print(std::ostream *out) {
  for (auto it : exe_) {
    *out << it << " ";
  }
  *out << endl;
}

void Command::Parse(const char *line) {
  char *tok;
  char *rest;
  unsigned len = strlen(line)+1;
  char* tmpstr = reinterpret_cast<char*>(calloc(len, sizeof(char)));
  strncpy(tmpstr, line, len);
  while ((tok = strtok_r(tmpstr, " ", &rest)) != NULL) {
    exe_.push_back(tok);
    tmpstr = rest;
  }
}

// Functions of ParamConfig class.
void ParamConfig::Print(std::ostream *out) {
  *out << "Config values." << endl;
  *out << "\tPlayer parameter: ";  player_command.Print(out);
  *out << "\tRecorder parameter: ";  recorder_command.Print(out);

  *out << "\tPlayer FIFO name: " << player_fifo << endl;
  *out << "\tRecorder FIFO name: " << recorder_fifo << endl;

  *out << "\tNumber of test rounds: " << test_rounds << endl;
  *out << "\tPass threshold: " << pass_threshold << endl;
  *out << "\tAllowed delay: " << allowed_delay_millisecs << "(ms)" << endl;

  *out << "\tSample rate: " << sample_rate << endl;
  *out << "\tFFT size: " << fft_size << endl;
  *out << "\tMicrophone channels: " << num_mic_channels << endl;
  *out << "\tSpeaker channels: " << num_speaker_channels << endl;
  *out << "\tMicrophone active channels: ";
  for (auto it : active_mic_channels) {
    *out << it << ", ";
  }
  *out << endl;
  *out << "\tSpeaker active channels: ";
  for (auto it : active_speaker_channels) {
    *out << it << ", ";
  }
  *out << endl;
  // Sets double precision to 2.
  *out << std::setprecision(2) << std::fixed;

  *out << "\tTone length (in second): " << tone_length << endl;
  *out << "\tVolume range: " << start_volume << " ~ " << end_volume << endl;

  if (verbose) *out << "\t** Debug mode **." << endl;
  if (is_logging) *out << "\t** Log mode **." << endl;

  out->flush();
}

void ParamConfig::ParseActiveChannels(const char* arg, bool is_mic) {
  unsigned len = strlen(arg);
  char *ptr = static_cast<char*>(calloc(len, sizeof(char)));
  strncpy(ptr, arg, len);
  char *rest;
  char *tok;
  while ((tok = strtok_r(ptr, ",", &rest)) != NULL) {
    if (is_mic) {
      active_mic_channels.insert(atoi(tok));
    } else {
      active_speaker_channels.insert(atoi(tok));
    }
    ptr = rest;
  }
}

// Declares here again to prevent 'undefined reference'.
constexpr const struct option ParamConfig::long_options_[];

bool ParamConfig::ParseOptions(int argc, char* const argv[]) {
  int opt = 0;
  int optindex = -1;
  char config_file[100] = "test.conf";

  while ((opt = getopt_long(argc, argv, short_options_,
                            long_options_,
                            &optindex)) != -1) {
    switch (opt) {
      case 'z':
        strncpy(config_file, optarg, sizeof(config_file)); break;

      case 'l':
        tone_length = atof(optarg);
        /* Avoid overly short tones. */
        if (tone_length < 0.01) {
          std::cerr << "Tone length too short. "
                    << "Must be 0.01s or greater." << endl;
          return false;
        }
        break;

      case 'r':
        sample_rate = atoi(optarg); break;

      case 's':
        start_volume = atof(optarg); break;

      case 'e':
        end_volume = atof(optarg); break;

      case 'c':
        num_mic_channels = atoi(optarg); break;

      case 'C':
        num_speaker_channels = atoi(optarg); break;

      case 'a':
        ParseActiveChannels(optarg, true); break;

      case 'A':
        ParseActiveChannels(optarg, false); break;

      case 't':
        ParseSampleFormat(optarg); break;

      case 'n':
        fft_size = atoi(optarg); break;

      case 'f':
        player_fifo = string(optarg); break;

      case 'F':
        recorder_fifo = string(optarg); break;

      case 'T':
        test_rounds = atoi(optarg); break;

      case 'p':
        pass_threshold = atof(optarg); break;

      case 'd':
        allowed_delay_millisecs = atoi(optarg); break;

      case 'v':
        verbose = true; break;

      case 'L':
        is_logging = true; break;

      case 'h':
        return false;

      default:
        std::cerr << "Unknown arguments " << opt << endl;
        assert(false);
    }
  }

  if (!ParseConfigFile(config_file)) {
    std::cerr << "Error parsing configuration file.\n" << endl;
  }

  if ((fft_size == 0) || (fft_size & (fft_size - 1))) {
    std::cerr << "FFT size needs to be positive & power of 2: " << endl;
    return false;
  }

  // Normalizes the active channel set to explicitly list all channels.
  if (active_mic_channels.empty()) {
    for (int i = 0; i < num_mic_channels; ++i) {
      active_mic_channels.insert(i);
    }
  }
  if (active_speaker_channels.empty()) {
    for (int i = 0; i < num_speaker_channels; ++i) {
      active_speaker_channels.insert(i);
    }
  }

  return true;
}

void ParamConfig::ParseSampleFormat(const char* arg) {
  SampleFormat sample_fmt;
  for (int fmt = SampleFormat::kPcmU8;
       fmt != SampleFormat::kPcmInvalid;
       fmt++) {
    sample_fmt = SampleFormat(static_cast<SampleFormat::Type>(fmt));
    if (strcmp(sample_fmt.to_string(), optarg) == 0) {
      format = sample_fmt;
      return;
    }
  }
  std::cerr << "Unknown sample format " << arg
            << ", using S16 instead." << endl;
}

bool ParamConfig::ParseConfigFile(const char *config_file) {
  std::ifstream conf(config_file, std::ifstream::in);
  if (!conf.is_open()) {
    return false;
  }

  ParamConfig default_config;

  string line;
  while (std::getline(conf, line)) {
    std::istringstream buf(line);
    string key;
    if (std::getline(buf, key, '=')) {  // get key
      // Trims the key.
      const auto key_begin = key.find_first_not_of(" \t");
      if (key_begin == std::string::npos) {  // no empty key!
        std::cerr << "Empty key: " << line << std::endl;
        continue;
      }
      const auto key_end = key.find_last_not_of(" \t");
      const auto range = key_end - key_begin + 1;
      key = key.substr(key_begin, range);

      buf >> std::ws;  // remove whitespace after '='

      string value;
      if (std::getline(buf, value)) {  // get value
        if (key.compare("player-proc") == 0) {
          player_command.Parse(value.c_str());
        } else if (key.compare("recorder-proc") == 0) {
          recorder_command.Parse(value.c_str());
        } else if (key.compare("sample-rate") == 0
            && sample_rate == default_config.sample_rate) {
          // TODO(amylin): use bit operation to check?
          sample_rate = atoi(value.c_str());
        } else if (key.compare("fftsize") == 0
            && fft_size == default_config.fft_size) {
          fft_size = atoi(value.c_str());
        } else if (key.compare("mic-channels") == 0
            && num_mic_channels == default_config.num_mic_channels) {
          num_mic_channels = atoi(value.c_str());
        } else if (key.compare("active-mic-channels") == 0
            && active_mic_channels == default_config.active_mic_channels) {
          ParseActiveChannels(value.c_str(), true);
        } else if (key.compare("speaker-channels") == 0
            && num_speaker_channels == default_config.num_speaker_channels) {
          num_speaker_channels = atoi(value.c_str());
        } else if (key.compare("active-speaker-channels") == 0 &&
                   active_speaker_channels ==
                   default_config.active_speaker_channels) {
          ParseActiveChannels(value.c_str(), false);
        } else if (key.compare("sample-format") == 0
            && format == default_config.format) {
          ParseSampleFormat(value.c_str());
        } else if (key.compare("tone-length") == 0
            && tone_length == default_config.tone_length) {
          tone_length = atof(value.c_str());
        } else if (key.compare("start-volume") == 0
            && start_volume == default_config.start_volume) {
          start_volume = atof(value.c_str());
        } else if (key.compare("end-volume") == 0
            && (end_volume - default_config.end_volume) < 1e-9) {
          end_volume = atof(value.c_str());
        } else if (key.compare("player-fifo") == 0
            && player_fifo.empty()) {
          player_fifo = string(value);
        } else if (key.compare("recorder-fifo") == 0
            && recorder_fifo.empty()) {
          recorder_fifo = string(value);
        } else if (key.compare("test-rounds") == 0
            && test_rounds == default_config.test_rounds) {
          test_rounds = atoi(value.c_str());
        } else if (key.compare("pass-threshold") == 0
            && pass_threshold == default_config.pass_threshold) {
          pass_threshold = atof(value.c_str());
        } else if (key.compare("allowed-delay") == 0
            && allowed_delay_millisecs
            == default_config.allowed_delay_millisecs) {
          allowed_delay_millisecs = atoi(value.c_str());
        } else if (key.compare("verbose") == 0
            && verbose == default_config.verbose) {
          verbose = ParseBool(value);
        } else if (key.compare("log") == 0
            && is_logging == default_config.is_logging) {
          is_logging = ParseBool(value);
        } else {
          std::cerr << "Unknown argument or already set: " << key << endl;
        }
      } else {  // did not get value
        std::cerr << "Missing value for argument: " << key << endl;
        return false;
      }
    } else {  // did not get key
      std::cerr << "Wrong format: " << line << endl;
      return false;
    }
  }

  conf.close();

  // Checks player & recorder command set.
  if (!player_command.IsSet()) {
    std::cerr << "Missing argument: player-proc." << endl;
    return false;
  }
  if (!recorder_command.IsSet()) {
    std::cerr << "Missing argument: recorder-proc." << endl;
    return false;
  }

  return true;
}

bool ParamConfig::ParseBool(const string& str) {
  if (str.compare("true") == 0
      || str.compare("True") == 0
      || str.compare("t") == 0) {
    return true;
  }
  return false;
}

void ParamConfig::PrintUsage(std::ostream *out, const char* name) {
  ParamConfig default_config;

  *out << "Usage: " << name << " [options]" << endl;
  *out << "\t-z, --config-file: "
        << "\t\tThe path for player and recorder configuration file "
        << "(def 'test.conf').\n";
  *out << "\t-r, --sample-rate: "
        << "\t\tSample rate of generated wave in HZ "
        << "(def " << default_config.sample_rate << ")\n";
  *out << "\t-n, --fftsize:   "
        << "\t\tLonger fftsize has more carriers but longer latency."
        << " Also, fftsize needs to be power of 2"
        << "(def " << default_config.fft_size << ")\n";
  *out << "\t-c, --mic-channels: "
        << "\t\tThe number of microphone channels "
        << "(def " << default_config.num_mic_channels << ")\n";
  *out << "\t-a, --active-mic-channels: "
        << "\tComma-separated list of microphone channels to play on. "
        << "(def all channels)\n";
  *out << "\t-C, --speaker-channels: "
        << "\tThe number of speaker channels "
        << "(def " << default_config.num_speaker_channels << ")\n";
  *out << "\t-A, --active-speaker-channels: "
        << "\tComma-separated list of speaker channels to play on. "
        << "(def all channels)\n";
  *out << "\t-t, --sample-format: "
        << "\t\tFormat of recording & playing samples. "
        << "(def " << default_config.format.to_string() << ").\n";
  *out << "\t-l, --tone-length: "
        << "\t\tDecimal value of tone length in secs "
        << "(def " << default_config.tone_length << ")\n";
  *out << "\t-s, --start-volume: "
        << "\t\tDecimal value of start volume "
        << "(def " << default_config.start_volume << ")\n";
  *out << "\t-e, --end-volume: "
        << "\t\tDecimal value of end volume "
        << "(def " << default_config.end_volume << ")\n";
  *out << "\t-f, --player-fifo: "
        << "\t\tSet fifo name for player if player takes fifo as input.\n";
  *out << "\t-F, --recorder-fifo: "
        << "\t\tSet fifo name for recorder if recorder takes fifo as input.\n";
  *out << "\t-T, --test-rounds: "
        << "\t\tNumber of test rounds "
        << "(def " << default_config.test_rounds << ").\n";
  *out << "\t-p, --pass-threshold"
        << "\t\tThreshold of accumulated confidence to pass evaluation "
        << "(def " << default_config.pass_threshold << ").\n";
  *out << "\t-d, --allowed-delay"
        << "\t\tAllowed latency between player & recorder "
        << "(def " << default_config.allowed_delay_millisecs << ").\n";
  *out << "\t-v, --verbose:   "
        << "\t\tShow debugging information.\n";
  *out << "\t-L, --log:       "
        << "\t\tLog recorder & player outputs to file.\n";
  *out << "\t-h, --help:      "
        << "\t\tShow this page." << endl;
}

}  // namespace audio
}  // namespace autotest_client
