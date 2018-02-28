# vadumpcaps

This is a utility to show all capabilities of a VAAPI device/driver.

## Building

```
$ make
```

## Installing

```
$ make install
```
installs to `/usr/local`, or
```
$ make PREFIX=/somewhere/else install
```
installs to `/somewhere/else`.

## Running

```
$ vadumpcaps
```
will dump capabilities of the first DRM device with its default driver.
The output format is JSON, but is (intended to be) human-readable.

If you have multiple devices you can select which one to use with the `-d`
option:
```
$ vadumpcaps -d /dev/dri/renderD129
```

If you have multiple drivers you can select which one to use with the normal
libva environment variable:
```
$ LIBVA_DRIVER_NAME=iHD vadumpcaps -d /dev/dri/renderD128
```

Other options:
* `-i`, `--indent`:  Set indent level for pretty-printing (defaults to four
                     spaces).
* `-u`, `--ugly`:  Ugly-print (do not include any whitespace in the JSON).
