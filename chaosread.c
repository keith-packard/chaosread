/*
 * Copyright © 2016 Keith Packard <keithp@keithp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>

#define CHAOS_SIZE	64

#define CHAOS_VENDOR	0x1d50
#define CHAOS_PRODUCT	0x60c6

struct chaoskey {
	libusb_context		*ctx;
	libusb_device_handle	*handle;
	int			kernel_active;
};

libusb_device_handle *
chaoskey_match(libusb_device *dev, char *match_serial)
{
	struct libusb_device_descriptor desc;
	int ret;
	int match_len;
	char *device_serial = NULL;
	libusb_device_handle *handle = NULL;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor: %s\n", libusb_strerror(ret));
		return 0;
	}

	if (desc.idVendor != CHAOS_VENDOR)
		return NULL;
	if (desc.idProduct != CHAOS_PRODUCT)
		return NULL;

	ret = libusb_open(dev, &handle);

	if (match_serial == NULL)
		return handle;

	if (ret < 0) {
		fprintf(stderr, "failed to open device: %s\n", libusb_strerror(ret));
		return NULL;
	}

	match_len = strlen(match_serial);
	device_serial = malloc(match_len + 2);

	if (!device_serial) {
		fprintf(stderr, "malloc failed\n");
		goto out;
	}

	ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char *) device_serial, match_len + 1);

	if (ret < 0) {
		fprintf(stderr, "failed to get serial number: %s\n", libusb_strerror(ret));
		goto out;
	}

	device_serial[ret] = '\0';

	ret = strcmp(device_serial, match_serial);
	free(device_serial);
	if (ret)
		goto out;

	return handle;

out:
	if (handle)
		libusb_close(handle);
	return 0;
}

struct chaoskey *
chaoskey_open(char *serial)
{
	struct chaoskey	*ck;
	int		ret;
	ssize_t		num;
	libusb_device	**list;
	int		d;

	ck = calloc(sizeof (struct chaoskey), 1);
	if (!ck)
		goto out;
	ret = libusb_init(&ck->ctx);
	if (ret ) {
		fprintf(stderr, "libusb_init failed: %s\n", libusb_strerror(ret));
		goto out;
	}

	num = libusb_get_device_list(ck->ctx, &list);
	if (num < 0) {
		fprintf(stderr, "libusb_get_device_list failed: %s\n", libusb_strerror(num));
		goto out;
	}

	for (d = 0; d < num; d++) {
		libusb_device_handle *handle;

		handle = chaoskey_match(list[d], serial);
		if (handle) {
			ck->handle = handle;
			break;
		}
	}

	libusb_free_device_list(list, 1);

	if (!ck->handle) {
		if (serial)
			fprintf (stderr, "No chaoskey matching %s\n", serial);
		else
			fprintf (stderr, "No chaoskey\n");
		goto out;
	}

	ck->kernel_active = libusb_kernel_driver_active(ck->handle, 0);
	if (ck->kernel_active) {
		ret = libusb_detach_kernel_driver(ck->handle, 0);
		if (ret)
			goto out;
	}

	ret = libusb_claim_interface(ck->handle, 0);
	if (ret)
		goto out;

	return ck;
out:
	if (ck->kernel_active)
		libusb_attach_kernel_driver(ck->handle, 0);
	if (ck->ctx)
		libusb_exit(ck->ctx);
	free(ck);
	return NULL;
}

void
chaoskey_close(struct chaoskey *ck)
{
	libusb_release_interface(ck->handle, 0);
	if (ck->kernel_active)
		libusb_attach_kernel_driver(ck->handle, 0);
	libusb_close(ck->handle);
	libusb_exit(ck->ctx);
	free(ck);
}

#define ENDPOINT	0x86

int
chaoskey_read(struct chaoskey *ck, void *buffer, int len)
{
	uint8_t	*buf = buffer;
	int	total = 0;

	while (len) {
		int	ret;
		int	transferred;

		ret = libusb_bulk_transfer(ck->handle, ENDPOINT, buf, len, &transferred, 10000);
		if (ret) {
			if (total)
				return total;
			else {
				errno = EIO;
				return -1;
			}
		}
		len -= transferred;
		buf += transferred;
	}
	return total;
}

static const struct option options[] = {
	{ .name = "serial", .has_arg = 1, .val = 's' },
	{ .name = "length", .has_arg = 1, .val = 'l' },
	{ .name = "infinite", .has_arg = 0, .val = 'i' },
	{ .name = "bytes", .has_arg = 0, .val = 'b' },
	{ 0, 0, 0, 0},
};

static void usage(char *program)
{
	fprintf(stderr, "usage: %s [--serial=<serial>] [--length=<length>[kMG]] [--infinite] [--bytes]\n", program);
	exit(1);
}

int
main (int argc, char **argv)
{
	struct chaoskey	*ck;
	uint16_t	buf[512];
	int	got;
	int	c;
	char	*serial = NULL;
	char	*length_string;
	char	*length_end;
	unsigned long	length = sizeof(buf);
	int	this_time;
	int	infinite = 0;
	int	bytes = 0;

	while ((c = getopt_long(argc, argv, "s:l:ib", options, NULL)) != -1) {
		switch (c) {
		case 's':
			serial = optarg;
			break;
		case 'l':
			length_string = optarg;
			length = strtoul(length_string, &length_end, 10);
			if (!strcasecmp(length_end, "k"))
				length *= 1024;
			else if (!strcasecmp(length_end, "m"))
				length *= 1024 * 1024;
			else if (!strcasecmp(length_end, "g"))
				length *= 1024 * 1024 * 1024;
			else if (strlen(length_end))
				 usage(argv[0]);
			break;
		case 'i':
			infinite = 1;
			break;
		case 'b':
			bytes = 1;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	ck = chaoskey_open(serial);
	if (!ck)
		exit(1);

	if (bytes)
		length *= 2;

	while (length || infinite) {
		this_time = sizeof(buf);
		if (!infinite && length < sizeof(buf))
			this_time = (int) length;
		got = chaoskey_read(ck, buf, this_time);
		if (got < 0) {
			perror("read");
			exit(1);
		}
		if (bytes) {
			int i;
			for (i = 0; i < got / 2; i++)
				putchar((buf[i] >> 1 & 0xff));
		} else
			write(1, buf, got);
		length -= got;
	}
	exit(0);
}
