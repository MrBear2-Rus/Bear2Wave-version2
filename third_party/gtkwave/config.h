/*
 * Minimal config.h for embedding GTKWave libvzt / liblxt in Bear2Wave (MSVC).
 * Full gtkwave meson build generates this automatically; we vendor a subset.
 */
#ifndef BEAR2WAVE_GTKWAVE_CONFIG_H
#define BEAR2WAVE_GTKWAVE_CONFIG_H

#define STDC_HEADERS 1
#define PACKAGE_VERSION "gtkwave-embedded"
#define PACKAGE_BUGREPORT "bybell@rocketmail.com"

/* Headers (MSVC / vcpkg) */
#define HAVE_INTTYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GETOPT_H 1

/* Not used on Windows reader build */
/* #define HAVE_RPC_XDR_H */
/* #define HAVE_ALLOCA_H */

/* fseeko/ftello: vzt_read.h falls back to fseek/ftell when undefined */
#ifndef _MSC_VER
#define HAVE_FSEEKO 1
#endif

#endif /* BEAR2WAVE_GTKWAVE_CONFIG_H */
