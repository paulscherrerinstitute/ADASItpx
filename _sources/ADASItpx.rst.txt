ASI TPX driver
==============

:author: Xiaoqiang Wang

.. contents: Contents

.. _EPICS: https://epics-controls.org
.. _areaDetector: https://areadetector.github.io/master/index.html
.. _ADDriver: https://areadetector.github.io/master/ADCore/ADDriver.html
.. _Amsterdam Scientific Instruments: https://www.amscins.com

Introduction
------------

This is an `EPICS`_ `areaDetector`_ driver for `Amsterdam Scientific Instruments`_ hybrid pixel detectors
using the Serval HTTP API.

ADASItpx specific parameters
-----------------------------

The ADASItpx driver implements the following parameters in addition
to those in `ADDriver`_. The records are in asiTpx.template.

Data output settings
^^^^^^^^^^^^^^^^^^^^

The server supports three output channels,

* Raw - Unprocessed data straight from the detector in tpx3 format.
* Image - Two-dimensional images at detector acquisition framerate in TIFF format.
* Preview - Two-dimensional images at preview framerate.

The output file path of each channel can be one of the following:

* A physical file path on the server host, e.g. "/tmp".
* An HTTP host URL, e.g. "\http://localhost:8080". In such a case the image is accessible via URL "\http://localhost:8080/measurement/image".
* A TCP address,

  * "tcp://listen@localhost:1234". Server opens a socket at the specified port at the start of a measurement.
    The client can connect to this TCP address and start listening for data.
  * "tcp://connect@localhost:1234". The client opens a TCP socket at the specified address and port.
    On starting the measurement the server will connect to the client socket.

.. note:: In this driver, the preview image output is fixed to the server host URL. The driver polls this URL and read into NDArrays.


.. cssclass:: table-bordered table-striped table-hover
.. flat-table::
  :header-rows: 1
  :widths: 20 10 70

  * - EPICS record name
    - EPICS record type
    - Description
  * - $(P)$(R)RawEnable, $(P)$(R)RawEnable_RBV
    - mbbo, mbbi
    - Enable raw data output
  * - $(P)$(R)RawFilePath, $(P)$(R)RawFilePath_RBV
    - waveform
    - raw data output path
  * - $(P)$(R)RawFileTemplate, $(P)$(R)RawFileTemplate_RBV
    - waveform
    - raw data output file name prefix
  * - $(P)$(R)ImageEnable, $(P)$(R)ImageEnable_RBV
    - mbbo, mbbi
    - Enable image output
  * - $(P)$(R)PixelMode, $(P)$(R)PixelMode_RBV
    - mbbo, mbbi
    - Pixel mode for image output
  * - $(P)$(R)ImageFilePath, $(P)$(R)ImageFilePath_RBV
    - waveform
    - image output path
  * - $(P)$(R)ImageFileTemplate, $(P)$(R)ImageFileTemplate_RBV
    - waveform
    - image output file name prefix
  * - $(P)$(R)PreviewEnable, $(P)$(R)PreviewEnable_RBV
    - mbbo, mbbi
    - Enable preview image output.


Trigger settings
^^^^^^^^^^^^^^^^

The detector supports 8 trigger modes, which are configured by a combination of 3 EPICS records.

.. cssclass:: table-bordered table-striped table-hover
.. flat-table::
  :header-rows: 1
  :widths: 2 1 1 1 5

  * - Native TriggerMode
    - $(P)$(R)TriggerMode
    - $(P)$(R)ExposureMode
    - $(P)$(R)TriggerPolarity
    - Description

  * - PEXSTART_NEXSTOP
    - External
    - Gated
    - Positive
    - Acq. is started by positive edge external trigger input, stopped by negative edge

  * - NEXSTART_PEXSTOP
    - External
    - Gated
    - Negative
    - Acq. is started by negative edge external trigger input, stopped by positive edge

  * - PEXSTART_TIMERSTOP
    - External
    - Timed
    - Positive
    - Acq. is started by positive edge external trigger input, stopped by HW timer

  * - NEXSTART_TIMERSTOP
    - External
    - Timed
    - Negative
    - Acq. is started by negative edge external trigger input, stopped by HW timer

  * - AUTOTRIGSTART_TIMERSTOP
    - Internal
    - Timed
    - \-
    - Acq. is started by trigger from HW, stopped by HW timer

  * - CONTINUOUS
    - Internal
    - Gated
    - \-
    - Acq. is started by software, stopped by software

  * - AUTOTRIGSTART_TIMERSTOP
    - Software
    - Timed
    - \-
    - Acq. is started by writting 1 to $(P)$(R)TriggerSoftware, stopped by HW timer

  * - SOFTWARESTART_SOFTWARESTOP
    - Software
    - Gated
    - \-
    - Acq. is started by writting 1 to $(P)$(R)TriggerSoftware, stopped by writting 0
      to $(P)$(R)TriggerSoftware


Configuration
-------------

The command to configure an ADVimba camera in the startup script is:
::

  asiTpxConfig(const char *portName, const char *configFile,
                   int maxBuffers, int maxMemory, int priority, int stackSize)

The *configFile* is a json file, which specifies the server address and detector DACS and BPC file path, e.g. ::

  {
      "Server": {
          "Address": "http://localhost:8080"
      },
      "Detector": {
          "Config": {
              "PixelConfig": "/home/scratch/asi_tpx3/asi-server-300-tpx3/examples/tpx3/tpx3-demo.bpc",
              "DACS": "/home/scratch/asi_tpx3/asi-server-300-tpx3/examples/tpx3/tpx3-demo.dacs"
          }
      }
  }


MEDM screen
-----------

.. figure:: _static/ADASItpx.png
    :align: center