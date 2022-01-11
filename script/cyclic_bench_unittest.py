#!/usr/bin/env python3

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittest for utility functions in cyclic_bench.py"""

import unittest

from cyclic_bench import CyclicTestRunner
from cyclic_bench import CyclicTestStat


class CyclicTestRunnerTest(unittest.TestCase):
  """Unit tests for utility functions in CyclicTestRunner.

  @var _LOGS: test logs from `cyclictest --verbose`.
  @var _EXPECTED_LATENCIES: expected latencies list parsed from the
  corresponding log.
  @var _EXPECTED_STATS: expected stats calculated from the latencies.
  """

  def setUp(self):
    # TODO(eddyhsu): create test configs when needed in test.
    self.runner = CyclicTestRunner(None)

  _LOGS = [("""Max CPUs = 8
Online CPUs = 8
# /dev/cpu_dma_latency set to 0us
Thread 0 Interval: 1000
       0:       0:       9
       0:       1:      18
       0:       2:      15
       0:       3:      14
       0:       4:      14
       0:       5:      14
       0:       6:      24
       0:       7:      16
       0:       8:      15
       0:       9:      14""", 1),
           ("""Max CPUs = 8
Online CPUs = 8
# /dev/cpu_dma_latency set to 0us
Thread 0 Interval: 1000
Thread 1 Interval: 1000
       0:       0:      64
       0:       1:      66
       0:       2:      66
       0:       3:      65
       0:       4:      65
       1:       0:      58
       1:       1:      55
       1:       2:      65
       1:       3:      55
       1:       4:      55""", 2)]
  _EXPECTED_LATENCIES = [[[9, 18, 15, 14, 14, 14, 24, 16, 15, 14]],
                         [[64, 66, 66, 65, 65], [58, 55, 65, 55, 55]]]
  _EXPECTED_STATS = [[CyclicTestStat(9, 15, 24, 24)],
                     [
                         CyclicTestStat(64, 65, 66, 66),
                         CyclicTestStat(55, 55, 65, 65)
                     ]]

  def testParseLatency(self):
    self.assertEqual(len(self._LOGS), len(self._EXPECTED_LATENCIES))

    for test_no in range(len(self._LOGS)):
      latencies = self.runner.parse_latency(self._LOGS[test_no][0],
                                            self._LOGS[test_no][1])
      self.assertEqual(latencies, self._EXPECTED_LATENCIES[test_no])

  def testCalculateStats(self):
    self.assertEqual(len(self._EXPECTED_LATENCIES), len(self._EXPECTED_STATS))

    for test_no in range(len(self._EXPECTED_LATENCIES)):
      stats = self.runner.calculate_stats(self._EXPECTED_LATENCIES[test_no])
      self.assertEqual(stats, self._EXPECTED_STATS[test_no])


if __name__ == '__main__':
  unittest.main()
