#!/usr/bin/env python3

# Copyright 2018 The Chromium OS Authors. All rights reserved.
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
    'name', 'stream', 'valid_formats', 'valid_rates', 'valid_channels',
    'period_size_range', 'buffer_size_range'
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

TEST_SUITES = ['test_params', 'test_rates', 'test_all_pairs']

TEST_SUITES_DESCRIPTION = """
test suites list:
  test_params           Check whether all parameters can be set correctly.
  test_rates            Check whether all estimated rates are the same as what
                        it set.
  test_all_pairs        Check whether the audio is still stable when mixing
                        different params.
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
          stream: PLAYBACK
          available range: 1, 2
          available formats: S16_LE S32_LE
          rate range: [44100, 192000]
          available rates: 44100 48000 96000 192000
          period size range: [16, 262144]
          buffer size range: [32, 524288]
          ------------------------------
      '''
      Result
          DataDevInfo(
              name='hw:0,0',
              stream='PLAYBACK',
              valid_formats=['S16_LE', 'S32_LE'],
              valid_channels=['1', '2'],
              valid_rates=[44100, 48000, 96000, 192000],
              period_size_range=Range(lower=16, upper=262144),
              buffer_size_range=Range(lower=32, upper=524288)
          )

    Raises:
      ValueError: Can not get device information.
    """
    if 'DEVICE INFORMATION' not in context:
      raise ValueError('Can not get device information.')

    self._context = context

    return DataDevInfo(
        self._get_value('PCM handle name'),
        self._get_value('stream'),
        self._get_list('available formats'),
        list(map(int, self._get_list('available rates'))),
        list(map(int, self._get_list('available channels'))),
        self._get_range('period size range'),
        self._get_range('buffer size range'))


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


class AlsaConformanceTester(object):
  """Object which can set params and run alsa_conformance_test."""

  def __init__(self, name, stream, criteria):
    """Initializes an AlsaConformanceTester.

    Args:
      name: PCM device for playback or capture.
      stream: The stream type. (PLAYBACK or CAPTURE)
      criteria: A Criteria object for pass criteria.
    """
    self.name = name
    self.stream = stream
    self.format = None
    self.channels = None
    self.rate = None
    self.period_size = None
    self.criteria = criteria

    output = self.run(['--dev_info_only'])
    if output.rc != 0:
      print('Fail - {}'.format(output.err))
      exit()

    self.dev_info = DeviceInfoParser().parse(output.out)

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
    print('\tStream:', self.dev_info.stream)
    print('\tFormat:', self.dev_info.valid_formats)
    print('\tChannels:', self.dev_info.valid_channels)
    print('\tRate:', self.dev_info.valid_rates)
    print('\tPeriod_size range:', list(self.dev_info.period_size_range))
    print('\tBuffer_size range:', list(self.dev_info.buffer_size_range))

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
    logging.info('Execute command: %s', ' '.join(cmd))
    p = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding='utf8')
    rc = p.wait()
    out, err = p.communicate()
    return Output(rc, out, err[:-1])

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
    data = {}
    data['name'] = test_name
    logging.info(test_name)
    output = self.run(test_args)
    result, error = check_function(output)
    data['result'] = result
    data['error'] = error

    logging_msg = result
    if result == 'fail':
      logging_msg += ' - ' + error
    logging.info(logging_msg)

    return data

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

  def test(self, test_suites, use_json):
    """Does testing.

    Args:
      test_suites: Indicate which tests will be run.
      use_json: If true, print result with json format.
    """
    result = {}
    result['testSuites'] = []
    if 'test_params' in test_suites:
      result['testSuites'].append(self.test_params())
    if 'test_rates' in test_suites:
      result['testSuites'].append(self.test_rates())
    if 'test_all_pairs' in test_suites:
      result['testSuites'].append(self.test_all_pairs())
    result = self.summarize(result)

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

  def summarize(self, result):
    """Summarizes the test results.

    Args:
      result: A result from tester.

    Returns:
      The result with counts of pass and fail. For example:
      {
          "pass": 4,
          "fail": 1,
          "testSuites": [
              {
                  "name": "Test Params",
                  "pass": 4,
                  "fail": 1,
                  "tests": [
                      {
                          "name": "Set channels 2",
                          "result": "pass",
                          "error": ""
                      },
                      {
                          "name": "Set rate 48000",
                          "result": "fail",
                          "error": "Set rate 48000 but got 44100"
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
      '--json', action='store_true', help='Print result in JSON format')
  parser.add_argument('--log-file', help='The file to save logs.')
  parser.add_argument(
      '--test-suites', nargs='+',
      help='Customize which test suites should be run. If not set, all suites '
           'will be run. See the test suites list for more information.',
      choices=TEST_SUITES, default=TEST_SUITES, metavar='TEST_SUITE')

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
    tester = AlsaConformanceTester(args.input_device, 'CAPTURE', criteria)

  if args.output_device:
    tester = AlsaConformanceTester(args.output_device, 'PLAYBACK', criteria)

  tester.test(args.test_suites, args.json)

if __name__ == '__main__':
  main()
