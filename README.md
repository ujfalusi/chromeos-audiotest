# ChromeOS Audio Test

This package intends to be a fast and powerful automated audio test tool.
It performs polyphonic tone synthesis while simultaneously capturing
the incoming sound in some form of external audio loopback. The
external Mic/Headphone jack can easily be tested but the built-in
speakers and microphone can also be tested by placing the
ChromeBook/Laptop in a box and arranging for the built-in speaker
sound to bounce off a surface and be directed to the built-in
microphone. USB/audio dongles can also be tested and the audio output
present in a HDMI port can likewise be tested with an appropriate
dongle while looping back to an incoming microphone port.

The incoming sound is convolved through a Fourier Transform and the
resulting energy peaks are filtered, sorted and compared to the known
expected tones. Any missing or unexpected tones are reported.
This test is fast- usually running in 1/2 second per port combination
tested. During that time up to 7 sinsusoidal tones are polyphonically
played, captured, Fourier analyzed, a report given and an optional
spectrogram created. More than 7 tones can be used but peak
discrimination of the convolved signal becomes more difficult.
This could be handled by using a larger transform space and taking
a little more time in processing.

It is a command line tool which lends itself to being
script driven. Scripts would be different for different
laptop/chromebook devices as the configuration of audio inputs
and outputs can be very different across different platforms.
This tool can be run on any Linux machine, not just a ChromeBook,
providied the ALSA audio subsystem is available, and it usually will be.

Additional features in the tool:
- Generation of a captured audio spectrogram file in 'gnuplot' format.
- Playing the captured sound buffer to a monitor port. Useful when
  DUT is in a chamber and the monitor port is a USB dongle.
- Generation of sound without capture; capture without generation.
  Useful for external characterization of test tool itself.


Additional features planned for inclusion very shortly:
- Automatic test sequencing of multiple channels in a port.
- Remote control, so a host can test ports in a Device Under Test.
- Higher resolution sound capture with new hardware.

## Packages included

- [ALSA Conformance Test](alsa_conformance_test.md) - A tool to verify audio
drivers.

- [ALSA API Test](src/alsa_api_test.c) - Test basic ALSA API function.

- [ALSA Helper](src/alsa_helpers.c) - Get basic information for PCM devices.

- [AudioFunTest](src/audiofuntest.cc) - A tool to test loopback, comparing
output streaming and input streaming with a special designed algorithm.

- [CRAS API Test](src/cras_api_test.c) - Test basic CRAS API function.

- [Loop Test](src/looptest.c) - Test loopback function for PCM devices.

- [Loopback Latency](loopback_latency/main.c) - Test loopback latency for PCM devices.

- [Test Tones](src/test_tones.cc) - A tool to play tone from PCM devices.

## Miscellaneous Notes

Properties of the DFT
DFT stands for 'Discrete Fourier Transform'. A pure Fourier
Transform is continuous; DFTs were invented so Fourier Transforms
could be used in the real world.
The reverse of the DFT is calculated simply by applying the
Fourier transform again, and reversing the resulting buffer
(to satisfy normalization, also divide resulting samples by N).

The components of the DFT are complex numbers. They have a modulus
and a phase:
The modulus of V[k], sqrt(V[k]*V*[k]), describes the intensity of
the particular frequency corresponding to k (because of normalization,
divide modulus by sqrt(N))

The phase of a given component describes the phase shift of this
component--at what angle it starts its oscillations.

What is the relation between k, the index of the Fourier component,
and its frequency? The function describing the k-th basis vector
is e-2πikt/N. Sample it at t= 0,1,2,..,N-1. Its real part is
cos(2πkt/N) (cosine is symmetric, so sign of argument doesn't matter).
Cosine starts repeating after 2π, which corresponds to
kt/N = 1 or t = N/k. So how many such repetitions fit in a segment
of length N? Exactly k. In other words, the k-th basis vector has
k periods per buffer. What time interval corresponds to one buffer?
That depends on sampling speed. If sps is number of samples/sec,
then sps/N is number of buffers/second. Therefore k*sps/N is number
of periods/second, or frequency.
