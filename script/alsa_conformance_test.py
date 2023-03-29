#!/usr/bin/env python3

# Copyright 2018 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to run alsa_conformance_test automatically."""

import argparse
import collections
import json
import logging
import re
import subprocess
import sys

TEST_BINARY = 'alsa_conformance_test'

Range = collections.namedtuple('Range', ['lower', 'upper'])

DataDevInfo = collections.namedtuple('DataDevInfo', [
    'name', 'card', 'device', 'stream', 'valid_formats', 'valid_rates',
    'valid_channels', 'period_size_range', 'buffer_size_range', 'mixers'
])

DataParams = collections.namedtuple('DataParams', [
    'name', 'stream', 'access', 'format', 'channels', 'rate', 'period_size',
    'buffer_size'
])

DataResult = collections.namedtuple('DataResult', [
    'points', 'step_average', 'step_min', 'step_max', 'step_sd', 'rate',
    'rate_error', 'underrun_nums', 'overrun_nums'
])

DEFAULT_PARAMS = DataParams(
    name=None,
    stream=None,
    access='MMAP_INTERLEAVED',
    format='S16_LE',
    channels=2,
    rate=48000,
    period_size=240,
    buffer_size=None)

Criteria = collections.namedtuple('PassCriteria', [
    'rate_diff', 'rate_err'
])

DESCRIPTION = """
Test basic funtion of alsa pcm device automatically.
It is a script for alsa_conformance_test.
"""

TEST_SUITES = ['test_card_name', 'test_params', 'test_rates', 'test_all_pairs', 'test_usb_mixer']

TEST_SUITES_DESCRIPTION = """
test suites list:
  test_card_name        Check whether card name is in the block list.
  test_params           Check whether all parameters can be set correctly.
  test_rates            Check whether all estimated rates are the same as what
                        it set.
  test_all_pairs        Check whether the audio is still stable when mixing
                        different params.
  test_usb_mixer        Check whether USB mixer set correctly. This will run if the device is USB audio.
"""

class Output(object):
  """The output from alsa_conformance_test.

  Attributes:
    rc: The return value.
    out: The output from stdout.
    err: The output from stderr.
  """

  def __init__(self, rc, out, err):
    """Inits Output object."""
    self.rc = rc
    self.out = out
    self.err = err


class Parser(object):
  """Object which can parse result from alsa_conformance_test.

  Attributes:
    _context: The output result from alsa_conformance_test.
  """

  def parse(self, context):
    """Parses alsa_conformance_test result.

    Args:
      context: The output result from alsa_conformance_test.
    """
    raise NotImplementedError

  def _get_value(self, key, unit=''):
    """Finds the key in context and returns its content.

    Args:
      key: String representing the key.
      unit: String representing the unit.

    Returns:
      The content following the key. For example:

      _context = '''
          format: S16_LE
          channels: 4
          rate: 48000 fps
          period size: 240 frames
      '''
      _get_value('format') = 'S16_LE'
      _get_value('channels') = '4'
      _get_value('rate', 'fps') = '48000'
      _get_value('period size', 'frames') = '240'

    Raises:
      ValueError: Can not find the key in context or finds an
                  unmatched unit.
    """
    pattern = key + ': (.*)' + unit + '\n'
    search = re.search(pattern, self._context)
    if search is None:
      msg = 'Can not find keyword {}'.format(key)
      if not unit:
        msg += ' with unit {}'.format(unit)
      raise ValueError(msg)
    return search.group(1).strip()
  def _get_pair(self, key):
    """Finds the key in context and returns its content as a pair.

    Args:
      key: String representing the key.

    Returns:
      The pair following the key. For example:

      _context = '''
          Card: Wired [Zone Wired]
          Device: USB Audio [USB Audio]
      '''
      _get_pair('Card') = ('Wired', 'Zone Wired')
      _get_pair('Device') = ('USB Audio', 'USB Audio')

    Raises:
      ValueError: Can not find the key in context.
      ValueError: Can find the key in context, but format is incorrect.
    """
    content = self._get_value(key)
    match = re.match("(?P<_0>.*) \[(?P<_1>.*)\]", content)
    if not match:
      msg = 'Wrong format of content:{}'.format(content)
      raise ValueError(msg)
    return match.group(1), match.group(2)
  def _get_list(self, key):
    """Finds the key in context and returns its content as a list.

    Args:
      key: String representing the key.

    Returns:
      The list following the key. For example:

      _context = '''
          available channels: 1, 2
          available formats: S16_LE S32_LE
          available rates: 44100 48000 96000
      '''
      _get_list('available channels') = ['1', '2']
      _get_list('available formats') = ['S16_LE', 'S32_LE']
      _get_list('available rates') = ['44100', '48000', '96000']

    Raises:
      ValueError: Can not find the key in context.
    """
    content = self._get_value(key)
    content = content.strip('[]')
    content = content.replace(',', ' ')
    return content.split()

  def _get_range(self, key):
    """Finds the key in context and returns its content as a range.

    Args:
      key: String representing the key.

    Returns:
      The range following the key. For example:

      context = '''
          period size range: [16, 262144]
      '''
      _get_range('period size range') = [16, 262144]

    Raises:
      ValueError: Can not find the key in context or wrong format.
    """
    content_list = self._get_list(key)
    if len(content_list) != 2:
      raise ValueError('Wrong range format.')

    return Range(*map(int, content_list))


class DeviceInfoParser(Parser):
  """Object which can parse device info from alsa_conformance_test."""

  def __init__(self, allow_rates=None):
    """
    Args:
      allow_rates: Restrict the sample rates to be tested if specified.
    """
    self.allow_rates = set(allow_rates) if allow_rates else None

  def _parse_mixer(self):
    """
    Parse multi mixers into a list in json format.
    context = '''
      ------DEVICE INFORMATION------
      PCM handle name: hw:1,0
      PCM type: HW
      card: Wired [Zone Wired]
      device: USB Audio [USB Audio]
      stream: PLAYBACK
      available channels: 2
      available formats: S16_LE S24_3LE
      rate range: [44100, 96000]
      available rates: 44100 48000 96000
      period size range: [45, 131072]
      buffer size range: [90, 262144]
      mixer: name:PCM index:0 has_volume:1 db_range:[-6562, 0] volume_range:[0, 175]
      mixer: name:Headset index:0 has_volume:1 db_range:[-1000, 0] volume_range:[0, 15]
      ------------------------------
    '''
    Result
      mixers=[{
                'name': 'PCM',
                'index': 0,
                'has_volume': 1,
                'db_range': Range(lower=-6562, upper=0),
                'volume_range': Range(lower=0, upper=175),
              },
              {
                'name': 'Headset',
                'index': 0,
                'has_volume': 1,
                'db_range': Range(lower=-1000, upper=0),
                'volume_range': Range(lower=0, upper=15),
              }]
    """
    mixers = []
    for raw in re.finditer("mixer: (.*)", self._context):
      mixer = {}
      result = re.match(
          'name:(?P<_0>.+) index:(?P<_1>.+) has_volume:(?P<_2>.+) db_range:\[(?P<_3>.+), (?P<_4>.+)\] volume_range:\[(?P<_5>.+), (?P<_6>.+)\]',
          raw.group(1),
      )
      mixer['name'] = result.group(1)
      mixer['index'] = int(result.group(2))
      mixer['has_volume'] = int(result.group(3))
      mixer['db_range'] = Range(int(result.group(4)), int(result.group(5)))
      mixer['volume_range'] = Range(int(result.group(6)), int(result.group(7)))
      mixers.append(mixer)
    return mixers

  def parse(self, context):
    """Parses device information.

    Args:
      context: The output result from alsa_conformance_test
               with --dev_info_only flag.

    Returns:
      The DataDevInfo object which includes device information. For example:

      context = '''
          ------DEVICE INFORMATION------
          PCM handle name: hw:0,0
          PCM type: HW
          card: CardID [CardName]
          device: DeviceID [DeviceName]
          stream: PLAYBACK
          available range: 1, 2
          available formats: S16_LE S32_LE
          rate range: [44100, 192000]
          available rates: 44100 48000 96000 192000
          period size range: [16, 262144]
          buffer size range: [32, 524288]
          mixer: name:PCM index:0 has_volume:1 db_range:[-6562, 0] volume_range:[0, 175]
          mixer: name:Headset index:0 has_volume:1 db_range:[-1000, 0] volume_range:[0, 15]
          ------------------------------
      '''
      Result
          DataDevInfo(
              name='hw:0,0',
              card='soundcard',
              stream='PLAYBACK',
              valid_formats=['S16_LE', 'S32_LE'],
              valid_channels=['1', '2'],
              valid_rates=[44100, 48000, 96000, 192000],
              period_size_range=Range(lower=16, upper=262144),
              buffer_size_range=Range(lower=32, upper=524288),
              mixers=[{
                'name': 'PCM',
                'index': 0,
                'has_volume': 1,
                'db_range': Range(lower=-6562, upper=0),
                'volume_range': Range(lower=0, upper=175),
              },{
                'name': 'Headset',
                'index': 0,
                'has_volume': 1,
                'db_range': Range(lower=-1000, upper=0),
                'volume_range': Range(lower=0, upper=15),
              }]
          )

    Raises:
      ValueError: Can not get device information.
    """
    if 'DEVICE INFORMATION' not in context:
      raise ValueError('Can not get device information.')

    self._context = context

    valid_rates = list(map(int, self._get_list('available rates')))
    if self.allow_rates:
      valid_rates = [r for r in valid_rates if r in self.allow_rates]

    return DataDevInfo(
        self._get_value('PCM handle name'),
        self._get_pair('card'),
        self._get_pair('device'),
        self._get_value('stream'),
        self._get_list('available formats'),
        valid_rates,
        list(map(int, self._get_list('available channels'))),
        self._get_range('period size range'),
        self._get_range('buffer size range'),
        self._parse_mixer(),
        )


class ParamsParser(Parser):
  """Object which can parse params from alsa_conformance_test."""

  def parse(self, context):
    """Parses device params.

    Args:
      context: The output result from alsa_conformance_test.

    Returns:
      The DataParams object which includes device information. For example:

      context = '''
          ---------PRINT PARAMS---------
          PCM name: hw:0,0
          stream: PLAYBACK
          access type: MMAP_INTERLEAVED
          format: S16_LE
          channels: 2
          rate: 48000 fps
          period time: 5000 us
          period size: 240 frames
          buffer time: 160000 us
          buffer size: 7680 frames
          ------------------------------
      '''
      Result
          DataParams(
              name='hw:0,0',
              stream='PLAYBACK',
              access='MMAP_INTERLEAVED',
              format='S16_LE',
              channels=2,
              rate=48000,
              period_size=240,
              buffer_size=7680
          )

    Raises:
      ValueError: Can not get params information or wrong format.
    """
    if 'PRINT PARAMS' not in context:
      raise ValueError('Can not get params information.')

    self._context = context

    rate = self._get_value('rate', unit='fps')
    period_size = self._get_value('period size', unit='frames')
    buffer_size = self._get_value('buffer size', unit='frames')

    return DataParams(
        self._get_value('PCM name'),
        self._get_value('stream'),
        self._get_value('access type'),
        self._get_value('format'),
        int(self._get_value('channels')),
        float(rate),
        int(period_size),
        int(buffer_size))


class ResultParser(Parser):
  """Object which can parse run result from alsa_conformance_test."""

  def parse(self, context):
    """Parses run result.

    Args:
      context: The output result from alsa_conformance_test.

    Returns:
      The DataResult object which includes run result. For example:

      context = '''
          ----------RUN RESULT----------
          number of recorders: 1
          number of points: 6142
          step average: 7.769419
          step min: 1
          step max: 41
          step standard deviation: 1.245727
          rate: 48000.042167
          rate error: 0.349262
          number of underrun: 0
          number of overrun: 0
      '''
      Result
          DataResult(
              points=6162,
              step_average=7.769419,
              step_min=1,
              step_max=41,
              step_sd=1.245727,
              rate=48000.042167,
              rate_error=0.349262,
              underrun_nums=0,
              overrun_nums=0
          )

    Raises:
      ValueError: Can not get run result or wrong format.
    """
    if 'RUN RESULT' not in context:
      raise ValueError('Can not get run result.')

    self._context = context[context.find('RUN RESULT'):]

    return DataResult(
        int(self._get_value('number of points')),
        float(self._get_value('step average')),
        int(self._get_value('step min')),
        int(self._get_value('step max')),
        float(self._get_value('step standard deviation')),
        float(self._get_value('rate')),
        float(self._get_value('rate error')),
        int(self._get_value('number of underrun')),
        int(self._get_value('number of overrun')))


class USBMixerChecker:
  """Object which can check usb device mixer info from alsa_conformance_test."""

  @staticmethod
  def check_mixer_num(mixers):
    """
    check number of mixer == 1
    """
    result = 'pass'
    error = ''
    if len(mixers) != 1:
      result = 'fail'
      error = 'incorrect number of mixer[{}] should be [{}]'.format(len(mixers), 1)
    return result, error

  @staticmethod
  def check_has_volume_control(mixer):
    """
    check has volume == 1
    """
    result = 'pass'
    error = ''
    if(mixer['has_volume'] != 1):
      result = 'fail'
      error = 'error: mixer name[{}] has_volume[{}] can only be {}'.format(mixer['name'], mixer['has_volume'], 1)
    return result, error

  @staticmethod
  def check_playback_db_range(mixer):
    """
    check 500 <= max_db - min_db <= 20000
    """
    result = 'pass'
    error = ''
    if not (500 <= mixer['db_range'].upper - mixer['db_range'].lower <= 20000):
      result = 'fail'
      error = 'error: mixer name[{}] db_range[{}, {}] ' \
        'incorrect. Should be {} <= db_range_max - db_range_min <= {}'.format(mixer['name'], mixer['db_range'].lower, mixer['db_range'].upper, 500, 20000)
    return result, error

  @staticmethod
  def check_playback_volume_range(mixer):
    """
    playback volume_range_max - volume_range_min >= 10
    """
    result = 'pass'
    error = ''
    if not (10 <= mixer['db_range'].upper - mixer['db_range'].lower):
      result = 'fail'
      error = 'error: mixer name[{}] playback volume_range[{}, {}] ' \
        'incorrect. Should be {} <= db_range_max - db_range_min'.format(mixer['name'], mixer['volume_range'].lower, mixer['volume_range'].upper, 10)
    return result, error


class AlsaConformanceTester(object):
  """Object which can set params and run alsa_conformance_test."""

  def __init__(self, name, stream, criteria, threshold, allow_rates):
    """Initializes an AlsaConformanceTester.

    Args:
      name: PCM device for playback or capture.
      stream: The stream type. (PLAYBACK or CAPTURE)
      criteria: A Criteria object for pass criteria.
      allow_rates: Restrict the sample rates to be tested if specified.
    """
    self.name = name
    self.stream = stream
    self.format = None
    self.channels = None
    self.rate = None
    self.period_size = None
    self.merge_thld_size = threshold
    self.criteria = criteria

    output = self.run(['--dev_info_only'])
    if output.rc != 0:
      print('Fail - {}'.format(output.err))
      exit()

    self.dev_info = DeviceInfoParser(allow_rates).parse(output.out)

  def init_params(self):
    """Sets the device params to the default values.

    If the default value is not supported, choose the first supported one
    instead.
    """
    in_range = lambda x, Range: Range.lower <= x <= Range.upper

    if DEFAULT_PARAMS.format in self.dev_info.valid_formats:
      self.format = DEFAULT_PARAMS.format
    else:
      self.format = self.dev_info.valid_formats[0]
    if DEFAULT_PARAMS.channels in self.dev_info.valid_channels:
      self.channels = DEFAULT_PARAMS.channels
    else:
      self.channels = self.dev_info.valid_channels[0]
    if DEFAULT_PARAMS.rate in self.dev_info.valid_rates:
      self.rate = DEFAULT_PARAMS.rate
    else:
      self.rate = self.dev_info.valid_rates[0]
    if in_range(DEFAULT_PARAMS.period_size, self.dev_info.period_size_range):
      self.period_size = DEFAULT_PARAMS.period_size
    else:
      self.period_size = self.dev_info.period_size_range.lower

  def show_dev_info(self):
    """Prints device information."""
    print('Device Information')
    print('\tName:', self.dev_info.name)
    print('\tCard: {}[{}]'.format(self.dev_info.card[0], self.dev_info.card[1]))
    print('\tDevice: {}[{}]'.format(self.dev_info.device[0], self.dev_info.device[1]))
    print('\tStream:', self.dev_info.stream)
    print('\tFormat:', self.dev_info.valid_formats)
    print('\tChannels:', self.dev_info.valid_channels)
    print('\tRate:', self.dev_info.valid_rates)
    print('\tPeriod_size range:', list(self.dev_info.period_size_range))
    print('\tBuffer_size range:', list(self.dev_info.buffer_size_range))
    for mixer in self.dev_info.mixers:
        print('\tMixer:')
        print('\t\tMixer name:', mixer['name'])
        print('\t\tMixer index:', mixer['index'])
        print('\t\tMixer has_volume:', mixer['has_volume'])
        print('\t\tMixer db_range:', list(mixer['db_range']))
        print('\t\tMixer volume_range:', list(mixer['volume_range']))

  def run(self, arg):
    """Runs alsa_conformance_test.

    Args:
      arg: An array of strings for extra arguments.

    Returns:
      The Output object from alsa_conformance_test.
    """
    if self.stream == 'PLAYBACK':
      stream_arg = '-P'
    elif self.stream == 'CAPTURE':
      stream_arg = '-C'
    cmd = [TEST_BINARY, stream_arg, self.name] + arg
    if self.rate is not None:
      cmd += ['-r', str(self.rate)]
    if self.channels is not None:
      cmd += ['-c', str(self.channels)]
    if self.format is not None:
      cmd += ['-f', str(self.format)]
    if self.period_size is not None:
      cmd += ['-p', str(self.period_size)]
    if self.merge_thld_size is not None:
      cmd += ['--merge_threshold_sz', str(self.merge_thld_size)]

    logging.info('Execute command: %s', ' '.join(cmd))
    # Replace stdout/stderr with capture_output=True when Python 3.7 is
    # available
    p = subprocess.run(
      cmd,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      encoding='utf8')
    return Output(p.returncode, p.stdout, p.stderr[:-1])

  def do_check(self, test_name, check_function, *args):
    """Check result with check_function.

      Args:
        test_name: The name of test.
        check_function: The function to check the result.
        args: arguments for check function.

      Returns:
        The data or result. For example:

        {'name': The name of the test.
        'result': The first return value from check_function.
                  It should be 'pass' or 'fail'.
        'error': The second return value from check_function.}
    """
    data = {}
    data['name'] = test_name
    logging.info(test_name)
    result, error = check_function(*args)
    data['result'] = result
    data['error'] = error

    logging_msg = result
    if result == 'fail':
      logging_msg += ' - ' + error
    logging.info(logging_msg)

    return data

  def run_and_check(self, test_name, test_args, check_function):
    """Runs alsa_conformance_test and checks result.

    Args:
      test_name: The name of test.
      test_args: An array of strings for extra arguments of test.
      check_function: The function to check the result from
                      alsa_conformance_test. Refer to _default_check_function
                      for default implementation.

    Returns:
      The data or result. For example:

      {'name': The name of the test.
       'result': The first return value from check_function.
                 It should be 'pass' or 'fail'.
       'error': The second return value from check_function.}
    """
    output = self.run(test_args)
    return self.do_check(test_name, check_function, output)

  @staticmethod
  def _default_check_function(output):
    """It is the default check function of test.

    Args:
      output: The Output object from alsa_conformance_test.

    Returns:
      result: pass or fail.
      err: The error message.
    """
    if output.rc != 0:
      result = 'fail'
      error = output.err
    else:
      result = 'pass'
      error = ''
    return result, error

  def test(self, test_suites, use_json, json_file):
    """Does testing.

    Args:
      test_suites: Indicate which tests will be run.
      use_json: If true, print result with json format.
      json_file: If non empty, dump result in json format to the file
    """
    result = {}
    result['testSuites'] = []
    if 'test_card_name' in test_suites:
      result['testSuites'].append(self.test_card_name())
    if 'test_params' in test_suites:
      result['testSuites'].append(self.test_params())
    if 'test_rates' in test_suites:
      result['testSuites'].append(self.test_rates())
    if 'test_all_pairs' in test_suites:
      result['testSuites'].append(self.test_all_pairs())
    if 'test_usb_mixer' in test_suites:
      result['testSuites'].append(self.test_usb_mixer())
    result = self.summarize(result)

    if json_file:
      with open(json_file, 'w') as f:
        json.dump(result, f, indent=4, sort_keys=True)
    if use_json:
      print(json.dumps(result, indent=4, sort_keys=True))
    else:
      self.print_result(result)

  def test_params(self):
    """Checks if we can set params correctly on device."""
    result = {}
    result['name'] = 'Test Params'
    result['tests'] = []

    result['tests'] += self.test_params_channels()
    result['tests'] += self.test_params_formats()
    result['tests'] += self.test_params_rates()

    return result

  def test_params_channels(self):
    """Checks if channels can be set correctly."""
    self.init_params()
    result = []
    for self.channels in self.dev_info.valid_channels:
      test_name = 'Set channels {}'.format(self.channels)
      test_args = ['-d', '0.1']
      data = self.run_and_check(test_name, test_args,
                                self._default_check_function)
      result.append(data)
    return result

  def test_params_formats(self):
    """Checks if formats can be set correctly."""
    self.init_params()
    result = []
    for self.format in self.dev_info.valid_formats:
      test_name = 'Set format {}'.format(self.format)
      test_args = ['-d', '0.1']
      data = self.run_and_check(test_name, test_args,
                                self._default_check_function)
      result.append(data)
    return result

  def test_params_rates(self):
    """Checks if rates can be set correctly."""
    def check_function(output):
      """Checks if rate in params is the same as rate being set."""
      result = 'pass'
      error = ''
      if output.rc != 0:
        result = 'fail'
        error = output.err
      else:
        params = ParamsParser().parse(output.out)
        if params.rate != self.rate:
          result = 'fail'
          error = 'Set rate {} but got {}'.format(self.rate, params.rate)
      return result, error

    self.init_params()
    result = []
    for self.rate in self.dev_info.valid_rates:
      test_name = 'Set rate {}'.format(self.rate)
      test_args = ['-d', '0.1']
      data = self.run_and_check(test_name, test_args, check_function)
      result.append(data)
    return result

  def _check_rate(self, output):
    """Checks if rate being set meets rate calculated by the test."""
    result = 'pass'
    error = ''
    if output.rc != 0:
      result = 'fail'
      error = output.err
    else:
      run_result = ResultParser().parse(output.out)
      rate_threshold = self.rate * self.criteria.rate_diff / 100.0
      if abs(run_result.rate - self.rate) > rate_threshold:
        result = 'fail'
        error = ('Expected rate is {}, measure {}, '
                 'difference {} > threshold {}')
        error = error.format(
            self.rate, run_result.rate,
            abs(run_result.rate - self.rate),
            rate_threshold)
      elif run_result.rate_error > self.criteria.rate_err:
        result = 'fail'
        error = 'Rate error {} > threshold {}'.format(
            run_result.rate_error, self.criteria.rate_err)
    return result, error

  def test_rates(self):
    """Checks if rates meet our prediction."""
    result = {}
    result['name'] = 'Test Rates'
    result['tests'] = []

    self.init_params()
    for self.rate in self.dev_info.valid_rates:
      test_name = 'Set rate {}'.format(self.rate)
      test_args = ['-d', '1']
      data = self.run_and_check(test_name, test_args, self._check_rate)
      result['tests'].append(data)

    return result

  def test_all_pairs(self):
    """Checks if the audio is still stable when mixing different params.

    The test will check if rates meet our prediction when testing all
    combinations of channels, sample rates and formats.
    """
    result = {}
    result['name'] = 'Test All Pairs'
    result['tests'] = []

    self.init_params()
    for self.channels in self.dev_info.valid_channels:
      for self.format in self.dev_info.valid_formats:
        for self.rate in self.dev_info.valid_rates:
          test_name = 'Set channels {}, format {}, rate {}'.format(
              self.channels, self.format, self.rate)
          test_args = ['-d', '1']
          data = self.run_and_check(test_name, test_args, self._check_rate)
          result['tests'].append(data)

    return result

  def _check_card_name(self):
      """
      check card name cannot be in block list.

      A specific UCM configuration file is sometimes required for a USB audio device.
      If the card name is the default USB card name, like 'USB Audio Device'
      it will not be possible to add a UCM configuration file because UCM files are matched by card name.
      """
      result = 'pass'
      error = ''
      block_list = ['USB Audio Device']
      card_id, card_name = self.dev_info.card
      for name in block_list:
        if name == card_name:
          result = 'fail'
          error = 'error: card name[{}] cannot be [{}]'.format(name, card_name)
          break
      return result, error

  def test_card_name(self):
    """Checks card name whether in the block list
    """
    result = {}
    result['name'] = 'Test card name'
    result['tests'] = []
    test_name = 'Test card name is not in the block list'
    data = self.do_check(test_name, self._check_card_name)
    result['tests'].append(data)
    return result

  def test_usb_mixer(self):
    """Checks whether USB mixer expose it's interface correctly.
    This test will run if the device is USB audio.
    """
    result = {}
    result['name'] = 'Test USB mixer'
    result['tests'] = []

    usb_device_id, usb_device_name = self.dev_info.device

    if usb_device_name != 'USB Audio':
      result['name'] += ' - skip'
      return result

    self.init_params()

    mixers = self.dev_info.mixers

    usb_device_id, usb_device_name = self.dev_info.device

    if(usb_device_name != 'USB Audio'):
      result['name'] += ' - skip'
      return result

    self.init_params()

    mixers = self.dev_info.mixers
    is_input = self.dev_info.stream == 'CAPTURE'
    test_name = 'Test usb mixer number correctness'
    data = self.do_check(test_name, USBMixerChecker.check_mixer_num, mixers)
    result['tests'].append(data)
    for mixer in mixers:
      test_name = 'Test usb mixer has_volume correctness'
      data = self.do_check(test_name, USBMixerChecker.check_has_volume_control, mixer)
      result['tests'].append(data)
      if not is_input:
        test_name = 'Test usb mixer playback db range correctness'
        data = self.do_check(test_name, USBMixerChecker.check_playback_db_range, mixer)
        result['tests'].append(data)
        test_name = 'Test usb mixer playback volume range correctness'
        data = self.do_check(test_name, USBMixerChecker.check_playback_volume_range, mixer)
        result['tests'].append(data)
    return result

  def summarize(self, result):
    """Summarizes the test results.

    Args:
      result: A result from tester.

    Returns:
      The result with counts of pass and fail. For example:
      {
          'pass': 4,
          'fail': 1,
          'testSuites': [
              {
                  'name': 'Test Params',
                  'pass': 4,
                  'fail': 1,
                  'tests': [
                      {
                          'name': 'Set channels 2',
                          'result': 'pass',
                          'error': ''
                      },
                      {
                          'name': 'Set rate 48000',
                          'result': 'fail',
                          'error': 'Set rate 48000 but got 44100'
                      }
                  ]
              }
          ]
      }
    """
    result['pass'] = 0
    result['fail'] = 0
    for suite in result['testSuites']:
      suite['pass'] = 0
      suite['fail'] = 0
      for test in suite['tests']:
        suite[test['result']] += 1
      result['pass'] += suite['pass']
      result['fail'] += suite['fail']

    return result

  def print_result(self, result):
    """Prints the test results.

    Args:
      result: A result from summarize.
    """
    print('{} passed, {} failed'.format(result['pass'], result['fail']))

    self.show_dev_info()

    for suite in result['testSuites']:
      print(suite['name'])
      for test in suite['tests']:
        msg = test['name'] + ': ' + test['result']
        if test['result'] == 'fail':
          msg += ' - ' + test['error']
        print('\t' + msg)


def main():
  parser = argparse.ArgumentParser(
      description=DESCRIPTION,
      formatter_class=argparse.RawDescriptionHelpFormatter,
      epilog=TEST_SUITES_DESCRIPTION)
  parser.add_argument(
      '-C', '--input_device',
      help='Alsa input device, such as hw:0,0')
  parser.add_argument(
      '-P', '--output_device',
      help='Alsa output device, such as hw:0,0')
  parser.add_argument(
      '--rate-criteria-diff-pct',
      help=('The pass criteria of rate. The value is a percentage of rate. '
            'For example, 0.01 means the pass range is [47995.2, 48004.8] '
            'for rate 48000. (default: 0.01)'),
      type=float, default=0.01)
  parser.add_argument(
      '--rate-err-criteria',
      help='The pass criteria of rate error. (default: 10)',
      type=float, default=10)
  parser.add_argument(
      '--merge-thld-size',
      help=('Override the auto computed merge_threshold_sz. '
            'See the Explaination of point merge in the doc for details.'),
      type=int)
  parser.add_argument(
      '--json', action='store_true', help='Print result in JSON format')
  parser.add_argument(
      '--json-file', help='Dump result in JSON format to a file', type=str,
      default="")
  parser.add_argument('--log-file', help='The file to save logs.')
  parser.add_argument(
      '--test-suites', nargs='+',
      help='Customize which test suites should be run. If not set, all suites '
           'will be run. See the test suites list for more information.',
      choices=TEST_SUITES, default=TEST_SUITES, metavar='TEST_SUITE')
  parser.add_argument(
    '--allow-rates', nargs='+', type=int,
    help='Restrict the sample rates to be tested if specified. This can be '
         'used to reduce test run time when there are too many sample rates '
         'supported by the device.'
  )

  args = parser.parse_args()

  criteria = Criteria(args.rate_criteria_diff_pct, args.rate_err_criteria)

  if args.log_file is not None:
    logging.basicConfig(
        level=logging.DEBUG, filename=args.log_file, filemode='w')

  if not args.input_device and not args.output_device:
    print('Require an input or output device to test.', file=sys.stderr)
    exit(1)

  if args.input_device and args.output_device:
    print('Not support testing multiple devices yet.', file=sys.stderr)
    exit(1)

  if args.input_device:
    tester = AlsaConformanceTester(args.input_device, 'CAPTURE', criteria,
                                   args.merge_thld_size, args.allow_rates)

  if args.output_device:
    tester = AlsaConformanceTester(args.output_device, 'PLAYBACK', criteria,
                                   args.merge_thld_size, args.allow_rates)

  tester.test(args.test_suites, args.json, args.json_file)

if __name__ == '__main__':
  main()
