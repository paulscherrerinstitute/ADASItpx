ADASItpx Releases
=================

The latest untagged master branch can be obtained at https://github.com/paulscherrerinstitute/ADASItpx.


Release Notes
=============

2.0.0 (September 9, 2025)
-------------------------

* Replace record PreviedEnable with DataSource. Choice can be None, Preview or Image.
  IOC runs a TCP server configured by the :ref:`configFile`. If Preview or Image is selected,
  the detector server will be configured to send jsonimage format data to this server.
  The data is received into NDArray for areaDetector pipeline.
* Add setting of image integration mode and size.

1.2.0 (July 4, 2025)
--------------------

* Fix Raw and Image outputs with tcp scheme.

1.1.0 (July 1, 2025)
-------------------

* Add TDC control.

1.0.0 (May 9, 2025)
-------------------

* First release.
