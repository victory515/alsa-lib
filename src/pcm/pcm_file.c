/*
 *  PCM - File plugin
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
  
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

typedef struct {
	snd_pcm_t *slave;
	int close_slave;
	char *fname;
	int fd;
} snd_pcm_file_t;

static int snd_pcm_file_close(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	int err = 0;
	if (file->close_slave)
		err = snd_pcm_close(file->slave);
	if (file->fname) {
		free(file->fname);
		close(file->fd);
	}
	free(file);
	return 0;
}

static int snd_pcm_file_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_nonblock(file->slave, nonblock);
}

static int snd_pcm_file_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_info(file->slave, info);
}

static int snd_pcm_file_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_channel_info(file->slave, info);
}

static int snd_pcm_file_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t * params)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_channel_params(file->slave, params);
}

static int snd_pcm_file_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t * setup)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_channel_setup(file->slave, setup);
}

static int snd_pcm_file_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_status(file->slave, status);
}

static int snd_pcm_file_state(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_state(file->slave);
}

static int snd_pcm_file_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_delay(file->slave, delayp);
}

static int snd_pcm_file_prepare(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_prepare(file->slave);
}

static int snd_pcm_file_start(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_start(file->slave);
}

static int snd_pcm_file_stop(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_stop(file->slave);
}

static int snd_pcm_file_drain(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_drain(file->slave);
}

static int snd_pcm_file_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_pause(file->slave, enable);
}

static ssize_t snd_pcm_file_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_file_t *file = pcm->private;
	ssize_t f = snd_pcm_rewind(file->slave, frames);
	off_t err;
	if (f > 0) {
		err = lseek(file->fd, -snd_pcm_frames_to_bytes(pcm, f), SEEK_CUR);
		if (err < 0)
			return err;
	}
	return f;
}

static void snd_pcm_file_write_areas(snd_pcm_t *pcm, 
				     snd_pcm_channel_area_t *areas,
				     size_t offset, size_t frames)
{
	snd_pcm_file_t *file = pcm->private;
	size_t bytes = snd_pcm_frames_to_bytes(pcm, frames);
	char buf[bytes];
	size_t channels = pcm->setup.format.channels;
	snd_pcm_channel_area_t buf_areas[channels];
	size_t channel;
	ssize_t r;
	for (channel = 0; channel < channels; ++channel) {
		snd_pcm_channel_area_t *a = &buf_areas[channel];
		a->addr = buf;
		a->first = pcm->bits_per_sample * channel;
		a->step = pcm->bits_per_frame;
	}
	snd_pcm_areas_copy(areas, offset, buf_areas, 0, 
			   channels, frames, pcm->setup.format.sfmt);
	r = write(file->fd, buf, bytes);
	assert(r == (ssize_t)bytes);
}

static ssize_t snd_pcm_file_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	ssize_t n = snd_pcm_writei(file->slave, buffer, size);
	if (n > 0) {
		size_t bytes = snd_pcm_frames_to_bytes(pcm, n);
		ssize_t r = write(file->fd, buffer, bytes);
		assert(r == (ssize_t)bytes);
	}
	return n;
}

static ssize_t snd_pcm_file_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	ssize_t n = snd_pcm_writen(file->slave, bufs, size);
	if (n > 0) {
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		snd_pcm_file_write_areas(pcm, areas, 0, n);
	}
	return n;
}

static ssize_t snd_pcm_file_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	ssize_t n = snd_pcm_readi(file->slave, buffer, size);
	if (n > 0) {
		size_t bytes = snd_pcm_frames_to_bytes(pcm, n);
		ssize_t r = write(file->fd, buffer, bytes);
		assert(r == (ssize_t)bytes);
	}
	return n;
}

static ssize_t snd_pcm_file_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	ssize_t n = snd_pcm_writen(file->slave, bufs, size);
	if (n > 0) {
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		snd_pcm_file_write_areas(pcm, areas, 0, n);
	}
	return n;
}

static ssize_t snd_pcm_file_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	size_t ofs = pcm->mmap_control->appl_ptr % pcm->setup.buffer_size;
	ssize_t n = snd_pcm_mmap_forward(file->slave, size);
	size_t xfer = 0;
	if (n <= 0)
		return n;
	while (xfer < (size_t)n) {
		size_t frames = size - xfer;
		size_t cont = pcm->setup.buffer_size - ofs;
		if (cont < frames)
			frames = cont;
		snd_pcm_file_write_areas(pcm, pcm->mmap_areas, ofs, frames);
		ofs += frames;
		if (ofs == pcm->setup.buffer_size)
			ofs = 0;
		xfer += frames;
	}
	return n;
}

static ssize_t snd_pcm_file_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_avail_update(file->slave);
}

static int snd_pcm_file_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_mmap_status(file->slave, &pcm->mmap_status);
}

static int snd_pcm_file_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_mmap_control(file->slave, &pcm->mmap_control);
}

static int snd_pcm_file_mmap_data(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_mmap_data(file->slave, &pcm->mmap_data);
}

static int snd_pcm_file_munmap_status(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_munmap_status(file->slave);
}

static int snd_pcm_file_munmap_control(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_munmap_control(file->slave);
}

static int snd_pcm_file_munmap_data(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_munmap_data(file->slave);
}

static int snd_pcm_file_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_poll_descriptor(file->slave);
}

static int snd_pcm_file_channels_mask(snd_pcm_t *pcm, bitset_t *cmask)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_channels_mask(file->slave, cmask);
}

static int snd_pcm_file_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_params_info(file->slave, info);
}

static int snd_pcm_file_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_params(file->slave, params);
}

static int snd_pcm_file_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_setup(file->slave, setup);
}

static void snd_pcm_file_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_file_t *file = pcm->private;
	if (file->fname)
		fprintf(fp, "File PCM (file=%s)\n", file->fname);
	else
		fprintf(fp, "File PCM (fd=%d)\n", file->fd);
	if (pcm->valid_setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(file->slave, fp);
}

struct snd_pcm_ops snd_pcm_file_ops = {
	close: snd_pcm_file_close,
	info: snd_pcm_file_info,
	params_info: snd_pcm_file_params_info,
	params: snd_pcm_file_params,
	setup: snd_pcm_file_setup,
	channel_info: snd_pcm_file_channel_info,
	channel_params: snd_pcm_file_channel_params,
	channel_setup: snd_pcm_file_channel_setup,
	dump: snd_pcm_file_dump,
	nonblock: snd_pcm_file_nonblock,
	mmap_status: snd_pcm_file_mmap_status,
	mmap_control: snd_pcm_file_mmap_control,
	mmap_data: snd_pcm_file_mmap_data,
	munmap_status: snd_pcm_file_munmap_status,
	munmap_control: snd_pcm_file_munmap_control,
	munmap_data: snd_pcm_file_munmap_data,
};

struct snd_pcm_fast_ops snd_pcm_file_fast_ops = {
	status: snd_pcm_file_status,
	state: snd_pcm_file_state,
	delay: snd_pcm_file_delay,
	prepare: snd_pcm_file_prepare,
	start: snd_pcm_file_start,
	stop: snd_pcm_file_stop,
	drain: snd_pcm_file_drain,
	pause: snd_pcm_file_pause,
	rewind: snd_pcm_file_rewind,
	writei: snd_pcm_file_writei,
	writen: snd_pcm_file_writen,
	readi: snd_pcm_file_readi,
	readn: snd_pcm_file_readn,
	poll_descriptor: snd_pcm_file_poll_descriptor,
	channels_mask: snd_pcm_file_channels_mask,
	avail_update: snd_pcm_file_avail_update,
	mmap_forward: snd_pcm_file_mmap_forward,
};

int snd_pcm_file_open(snd_pcm_t **handlep, char *fname, int fd, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *handle;
	snd_pcm_file_t *file;
	int err;
	assert(handlep && slave);
	if (fname) {
		fd = open(fname, O_WRONLY|O_CREAT, 0666);
		if (fd < 0)
			return -errno;
	}
	file = calloc(1, sizeof(snd_pcm_file_t));
	if (!file) {
		return -ENOMEM;
	}
	file->fname = fname;
	file->fd = fd;
	file->slave = slave;
	file->close_slave = close_slave;

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(file);
		return -ENOMEM;
	}
	handle->type = SND_PCM_TYPE_FILE;
	handle->stream = slave->stream;
	handle->ops = &snd_pcm_file_ops;
	handle->op_arg = handle;
	handle->fast_ops = &snd_pcm_file_fast_ops;
	handle->fast_op_arg = handle;
	handle->mode = slave->mode;
	handle->private = file;
	err = snd_pcm_init(handle);
	if (err < 0) {
		snd_pcm_close(handle);
		return err;
	}
	*handlep = handle;

	return 0;
}

int _snd_pcm_file_open(snd_pcm_t **pcmp, char *name,
		       snd_config_t *conf, 
		       int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	char *fname = NULL;
	long fd = -1;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "sname") == 0) {
			err = snd_config_string_get(n, &sname);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "file") == 0) {
			err = snd_config_string_get(n, &fname);
			if (err < 0) {
				err = snd_config_integer_get(n, &fd);
				if (err < 0)
					return -EINVAL;
			}
			continue;
		}
		return -EINVAL;
	}
	if (!sname || (!fname && fd < 0))
		return -EINVAL;
	if (fname) {
		fname = strdup(fname);
		if (!fname)
			return -ENOMEM;
	}
	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free(sname);
	if (err < 0)
		return err;
	err = snd_pcm_file_open(pcmp, fname, fd, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
