ASI TPX driver
==============

An `EPICS <https://epics-controls.org>`_
`areaDetector <https://github.com/areaDetector/areaDetector/blob/master/README.md>`_
driver for `Amsterdam Scientific Instruments <https://www.amscins.com>`_ hybrid pixel
detectors using the Serval HTTP API.


Trigger settings
----------------

.. cssclass:: table-bordered table-striped table-hover
.. flat-table::
  :header-rows: 1
  :widths: 20 10 70

  * - ASI TriggerMode
    - EPICS TriggerMode
    - EPICS ExposureMode
    - EPICS TriggerPolarity

  * - PEXSTART_NEXSTOP
    - External
    - Gated
    - Positive

  * - NEXSTART_PEXSTOP
    - External
    - Gated
    - Negative
  
  * - PEXSTART_TIMERSTOP
    - External
    - Timed
    - Positive

  * - NEXSTART_TIMERSTOP
    - External
    - Timed
    - Negative

  * - AUTOTRIGSTART_TIMERSTOP
    - Internal
    - Timed
    - -

  * - CONTINUOUS
    - Internal
    - Gated
    - -

  * - AUTOTRIGSTART_TIMERSTOP
    - Software
    - Timed
    - -

  * - SOFTWARESTART_SOFTWARESTOP
    - Software
    - Gated
    - -

