/******************************************************************************
 *  bwm-ng                                                                    *
 *                                                                            *
 *  Copyright (C) 2004 Volker Gropp (vgropp@pefra.de)                         *
 *                                                                            *
 *  for more info read README.                                                *
 *                                                                            *
 *  This program is free software; you can redistribute it and/or modify      *
 *  it under the terms of the GNU General Public License as published by      *
 *  the Free Software Foundation; either version 2 of the License, or         *
 *  (at your option) any later version.                                       *
 *                                                                            *
 *  This program is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 *  GNU General Public License for more details.                              *
 *                                                                            *
 *  You should have received a copy of the GNU General Public License         *
 *  along with this program; if not, write to the Free Software               *
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 *                                                                            *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>

#include "../config.h"

/* ugly defines to handle different compile time options */

#if HAVE_STRUCT_IF_MSGHDR_IFM_DATA
#define SYSCTL
#endif

#if NETSTAT_LINUX || NETSTAT_BSD || NETSTAT_BSD_BYTES || NETSTAT_SOLARIS || NETSTAT_NETBSD
#define NETSTAT 1
#endif

#if HAVE_LIBSTATGRAB
#define LIBSTATGRAB
#endif

#if HAVE_LIBCURSES || HAVE_LIBNCURSES
#define HAVE_CURSES
#endif

#if HAVE_STRUCT_IF_DATA_IFI_IBYTES
#define GETIFADDRS
#endif

#if HAVE_GETOPT_LONG
#define LONG_OPTIONS
#endif

#if HAVE_IOCTL
#include <sys/ioctl.h>
#ifdef SIOCGIFFLAGS
#define IOCTL
#endif
#endif

#ifdef HAVE_CURSES
#include <curses.h>
#endif

#ifdef LONG_OPTIONS
#include <getopt.h>
#endif

#if HAVE_GETTIMEOFDAY
#include <sys/time.h>
#include <time.h>
#endif

#ifdef NETSTAT
#if HAVE_NETSTAT_PATH
#define NETSTAT_PATH HAVE_NETSTAT_PATH
#else
#define NETSTAT_PATH "netstat"
#endif
#endif

#if HAVE__PROC_NET_DEV
#ifdef PROC_NET_DEV_PATH
#define PROC_NET_DEV PROC_NET_DEV_PATH
#else
#define PROC_NET_DEV "/proc/net/dev"
#endif
#endif

/* prepare input methods */
#ifdef NETSTAT
#define NETSTAT_IN 1
#define INPUT_METHODS_NETSTAT " netstat"
#else
#define NETSTAT_IN 0
#define INPUT_METHODS_NETSTAT
#endif

#ifdef PROC_NET_DEV
#define PROC_IN 2
#define INPUT_METHODS_PROC " proc"
#else
#define PROC_IN 0
#define INPUT_METHODS_PROC
#endif

#ifdef GETIFADDRS
#define GETIFADDRS_IN 4
#define INPUT_METHODS_GETIFADDRS " getifaddrs"
#else
#define GETIFADDRS_IN 0
#define INPUT_METHODS_GETIFADDRS
#endif

#ifdef LIBSTATGRAB
#define LIBSTAT_IN 8
#define INPUT_METHODS_LIBSTATGRAB " libstatgrab"
#else
#define LIBSTAT_IN 0
#define INPUT_METHODS_LIBSTATGRAB
#endif

#ifdef SYSCTL
#define SYSCTL_IN 16
#define INPUT_METHODS_SYSCTL " sysctl"
#else
#define SYSCTL_IN 0
#define INPUT_METHODS_SYSCTL
#endif

#if HAVE_LIBKSTAT
#define KSTAT_IN 32
#define INPUT_METHODS_KSTAT " kstat"
#else 
#define KSTAT_IN 0
#define INPUT_METHODS_KSTAT
#endif

#define INPUT_MASK (NETSTAT_IN | PROC_IN | GETIFADDRS_IN | LIBSTAT_IN | SYSCTL_IN | KSTAT_IN)
#define INPUT_METHODS INPUT_METHODS_PROC INPUT_METHODS_GETIFADDRS INPUT_METHODS_SYSCTL INPUT_METHODS_KSTAT INPUT_METHODS_NETSTAT INPUT_METHODS_LIBSTATGRAB 

/* used for this nice spinning wheel */
#define IDLE_CHARS "-\\|/"


/* build output methods string: curses, plain, csv, html */
#ifdef HAVE_CURSES 
#define CURSES_OUTPUT_METHODS ", curses"
#define CURSES_OUT 0
#else
#define CURSES_OUTPUT_METHODS
#endif

#ifdef CSV
#define CSV_OUTPUT_METHODS ", csv"
#define CSV_OUT 2
#else
#define CSV_OUTPUT_METHODS
#endif

#ifdef HTML
#define HTML_OUTPUT_METHODS ", html"
#define HTML_OUT 3
#else
#define HTML_OUTPUT_METHODS
#endif

#define OUTPUT_METHODS "plain" CURSES_OUTPUT_METHODS CSV_OUTPUT_METHODS HTML_OUTPUT_METHODS
#define PLAIN_OUT 1
#define PLAIN_OUT_ONCE 4

/* build short options */
#ifdef PROC_NET_DEV
#define PROC_SHORT_OPT "f:"
#else 
#define PROC_SHORT_OPT
#endif

#ifdef HTML
#define HTML_SHORT_OPT "HR:"
#else
#define HTML_SHORT_OPT
#endif

#if NETSTAT && NETSTAT_OPTION
#define NETSTAT_SHORT_OPT "n:"
#else 
#define NETSTAT_SHORT_OPT
#endif

#ifdef CSV
#define CSV_SHORT_OPT "C:F:"
#else 
#define CSV_SHORT_OPT
#endif

#define SHORT_OPTIONS ":ht:dVa:u:I:i:o:c:DST:" PROC_SHORT_OPT HTML_SHORT_OPT NETSTAT_SHORT_OPT CSV_SHORT_OPT

#define BYTES_OUT 1
#define BITS_OUT 2
#define PACKETS_OUT 3
#define ERRORS_OUT 4

#define RATE_OUT 1
#define MAX_OUT 2
#define SUM_OUT 3
#define AVG_OUT 4

/* default length of avg in 1/1000sec */
#define AVG_LENGTH 30000

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define WRAP_AROUND ULONG_MAX
#define WRAP_32BIT 4294967295ul

#ifdef SYS_64BIT
#undef WRAP_AROUND
#define WRAP_AROUND 18446744073709551615ull
#endif

#define MAJOR 0
#define MINOR 5
#define EXTRA "-pre2-cvs"

#define print_version printf("Bandwidth Monitor NG (bmw-ng) v%i.%i%s\nCopyright (C) 2004,2005 Volker Gropp <bwmng@gropp.org>\n",MAJOR,MINOR,EXTRA); 