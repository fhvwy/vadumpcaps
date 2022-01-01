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

If you have multiple drivers you can select which one to use with the `-r`
option:
```
$ vadumpcaps -d /dev/dri/renderD128 -r i965
```

If you want to test a driver without installing it you can set the libva
`LIBVA_DRIVERS_PATH` environment variable to point at the directory
containing the `name_drv_video.so` file:
```
$ LIBVA_DRIVERS_PATH=/path/to/ihd/build/media_driver/ vadumpcaps -r iHD
```

General options:
* `-h`, `--help`: Show help.
* `-i`, `--indent`:  Set indent level for pretty-printing (defaults to four
                     spaces).
* `-u`, `--ugly`:  Ugly-print (do not include any whitespace in the JSON).
* `-d`, `--device`: Set device to use (defaults to `/dev/dri/renderD128`).
* `-r`, `--driver`: Set name of driver to load.

Output selection options:
* `-a`, `--all`: Dump all capabilities.
* `-p`, `--profiles`: Dump profiles.
* `-e`, `--entrypoints`: Dump entrypoints.
* `-t`, `--attributes`: Dump attributes.
* `-s`, `--surface-formats`: Dump surface formats.
* `-f`, `--filters`: Dump filters.
* `-c`, `--filter-caps`: Dump filter capabilities.
* `-l`, `--pipeline-caps`: Dump pipeline capabilities.
* `-m`, `--image-formats`: Dump image formats.
* `-b`, `--subpicture-formats`: Dump subpicture formats.
