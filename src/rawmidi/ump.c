/* SPDX-License-Identifier: LGPL-2.1+ */
/**
 * \file rawmidi/ump.c
 * \brief Universal MIDI Protocol (UMP) Interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "local.h"
#include "rawmidi_local.h"
#include "ump_local.h"

static int get_rawmidi_flags(snd_ump_t *ump)
{
	snd_rawmidi_info_t info;
	int err;

	err = snd_rawmidi_info(ump->rawmidi, &info);
	if (err < 0)
		return err;
	if (!(info.flags & SNDRV_RAWMIDI_INFO_UMP))
		return -EINVAL;
	ump->flags = info.flags;
	return 0;
}

/**
 * \brief Opens a new connection to the UMP interface.
 * \param inputp Returned input handle (NULL if not wanted)
 * \param outputp Returned output handle (NULL if not wanted)
 * \param name ASCII identifier of the UMP handle
 * \param mode Open mode
 * \return 0 on success otherwise a negative error code
 *
 * Opens a new connection to the UMP interface specified with
 * an ASCII identifier and mode.
 */
int snd_ump_open(snd_ump_t **inputp, snd_ump_t **outputp, const char *name,
		 int mode)
{
	snd_ump_t *input = NULL, *output = NULL;
	int err;

	if (inputp)
		*inputp = NULL;
	if (outputp)
		*outputp = NULL;
	if (!inputp && !outputp)
		return -EINVAL;

	err = -ENOMEM;
	if (inputp) {
		input = calloc(1, sizeof(*input));
		if (!input)
			goto error;
		input->is_input = 1;
	}
	if (outputp) {
		output = calloc(1, sizeof(*output));
		if (!output)
			goto error;
	}

	err = snd_rawmidi_open(input ? &input->rawmidi : NULL,
			       output ? &output->rawmidi : NULL,
			       name, mode | _SND_RAWMIDI_OPEN_UMP);
	if (err < 0)
		goto error;

	if (input) {
		err = get_rawmidi_flags(input);
		if (err < 0)
			goto error;
	}
	if (output) {
		err = get_rawmidi_flags(output);
		if (err < 0)
			goto error;
	}

	if (inputp)
		*inputp = input;
	if (outputp)
		*outputp = output;

	return 0;

 error:
	if (input) {
		if (input->rawmidi)
			snd_rawmidi_close(input->rawmidi);
		free(input);
	}
	if (output) {
		if (output->rawmidi)
			snd_rawmidi_close(output->rawmidi);
		free(output);
	}
	return err;
}

/**
 * \brief close UMP handle
 * \param ump UMP handle
 * \return 0 on success otherwise a negative error code
 *
 * Closes the specified UMP handle and frees all associated
 * resources.
 */
int snd_ump_close(snd_ump_t *ump)
{
	int err;

	err = snd_rawmidi_close(ump->rawmidi);
	free(ump);
	return err;
}

/**
 * \brief get RawMidi instance associated with the UMP handle
 * \param ump UMP handle
 * \return the associated RawMidi handle
 *
 * Returns the associated RawMidi instance with the given UMP handle
 */
snd_rawmidi_t *snd_ump_rawmidi(snd_ump_t *ump)
{
	return ump->rawmidi;
}

/**
 * \brief get identifier of UMP handle
 * \param ump UMP handle
 * \return ascii identifier of UMP handle
 *
 * Returns the ASCII identifier of given UMP handle. It's the same
 * identifier specified in snd_ump_open().
 */
const char *snd_ump_name(snd_ump_t *ump)
{
	return snd_rawmidi_name(ump->rawmidi);
}

/**
 * \brief get count of poll descriptors for UMP handle
 * \param ump UMP handle
 * \return count of poll descriptors
 */
int snd_ump_poll_descriptors_count(snd_ump_t *ump)
{
	return snd_rawmidi_poll_descriptors_count(ump->rawmidi);
}

/**
 * \brief get poll descriptors
 * \param ump UMP handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */
int snd_ump_poll_descriptors(snd_ump_t *ump, struct pollfd *pfds,
			     unsigned int space)
{
	return snd_rawmidi_poll_descriptors(ump->rawmidi, pfds, space);
}

/**
 * \brief get returned events from poll descriptors
 * \param ump UMP handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents returned events
 * \return zero if success, otherwise a negative error code
 */
int snd_ump_poll_descriptors_revents(snd_ump_t *ump, struct pollfd *pfds,
				     unsigned int nfds, unsigned short *revents)
{
	return snd_rawmidi_poll_descriptors_revents(ump->rawmidi, pfds, nfds,
						    revents);
}

/**
 * \brief set nonblock mode
 * \param ump UMP handle
 * \param nonblock 0 = block, 1 = nonblock mode
 * \return 0 on success otherwise a negative error code
 *
 * The nonblock mode cannot be used when the stream is in
 * #SND_RAWMIDI_APPEND state.
 */
int snd_ump_nonblock(snd_ump_t *ump, int nonblock)
{
	return snd_rawmidi_nonblock(ump->rawmidi, nonblock);
}

/**
 * \brief get information about associated RawMidi handle
 * \param ump UMP handle
 * \param info pointer to a snd_rawmidi_info_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_ump_rawmidi_info(snd_ump_t *ump, snd_rawmidi_info_t *info)
{
	return snd_rawmidi_info(ump->rawmidi, info);
}

/**
 * \brief set parameters about associated RawMidi stream
 * \param ump UMP handle
 * \param params pointer to a snd_rawmidi_params_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_ump_rawmidi_params(snd_ump_t *ump, snd_rawmidi_params_t *params)
{
	return snd_rawmidi_params(ump->rawmidi, params);
}

/**
 * \brief get current parameters about associated RawMidi stream
 * \param ump UMP handle
 * \param params pointer to a snd_rawmidi_params_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_ump_rawmidi_params_current(snd_ump_t *ump, snd_rawmidi_params_t *params)
{
	return snd_rawmidi_params_current(ump->rawmidi, params);
}

/**
 * \brief get status of associated RawMidi stream
 * \param ump UMP handle
 * \param status pointer to a snd_rawmidi_status_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_ump_rawmidi_status(snd_ump_t *ump, snd_rawmidi_status_t *status)
{
	return snd_rawmidi_status(ump->rawmidi, status);
}

/**
 * \brief drop all packets in the rawmidi I/O ring buffer immediately
 * \param ump UMP handle
 * \return 0 on success otherwise a negative error code
 */
int snd_ump_drop(snd_ump_t *ump)
{
	return snd_rawmidi_drop(ump->rawmidi);
}

/**
 * \brief drain all packets in the UMP I/O ring buffer
 * \param ump UMP handle
 * \return 0 on success otherwise a negative error code
 *
 * Waits until all MIDI packets are not drained (sent) to the
 * hardware device.
 */
int snd_ump_drain(snd_ump_t *ump)
{
	return snd_rawmidi_drain(ump->rawmidi);
}

/**
 * \brief write UMP packets to UMP stream
 * \param ump UMP handle
 * \param buffer buffer containing UMP packets
 * \param size output buffer size in bytes
 */
ssize_t snd_ump_write(snd_ump_t *ump, const void *buffer, size_t size)
{
	if (ump->is_input)
		return -EINVAL;
	return snd_rawmidi_write(ump->rawmidi, buffer, size);
}

/**
 * \brief read UMP packets from UMP stream
 * \param ump UMP handle
 * \param buffer buffer to store the input MIDI bytes
 * \param size input buffer size in bytes
 * \retval count of UMP packet in bytes otherwise a negative error code
 */
ssize_t snd_ump_read(snd_ump_t *ump, void *buffer, size_t size)
{
	if (!ump->is_input)
		return -EINVAL;
	return snd_rawmidi_read(ump->rawmidi, buffer, size);
}

/**
 * \brief read UMP packets from UMP stream with timestamp
 * \param ump UMP handle
 * \param[out] tstamp timestamp for the returned UMP packets
 * \param buffer buffer to store the input UMP packets
 * \param size input buffer size in bytes
 * \retval count of UMP packet in bytes otherwise a negative error code
 */
ssize_t snd_ump_tread(snd_ump_t *ump, struct timespec *tstamp, void *buffer,
		      size_t size)
{
	if (!ump->is_input)
		return -EINVAL;
	return snd_rawmidi_tread(ump->rawmidi, tstamp, buffer, size);
}

/**
 * \brief get size of the snd_ump_endpoint_info_t structure in bytes
 * \return size of the snd_ump_endpoint_info_t structure in bytes
 */
size_t snd_ump_endpoint_info_sizeof(void)
{
	return sizeof(snd_ump_endpoint_info_t);
}

/**
 * \brief allocate the snd_ump_endpoint_info_t structure
 * \param ptr returned pointer
 * \return 0 on success otherwise a negative error code if fails
 *
 * Allocates a new snd_rawmidi_status_t structure using the standard
 * malloc C library function.
 */
int snd_ump_endpoint_info_malloc(snd_ump_endpoint_info_t **info)
{
	*info = calloc(1, sizeof(snd_ump_endpoint_info_t));
	if (!*info)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees the snd_ump_endpoint_info_t structure
 * \param status pointer to the snd_ump_endpoint_info_t structure to free
 *
 * Frees the given snd_ump_endpoint_info_t structure using the standard
 * free C library function.
 */
void snd_ump_endpoint_info_free(snd_ump_endpoint_info_t *info)
{
	free(info);
}

/**
 * \brief copy one snd_ump_endpoint_info_t structure to another
 * \param dst destination snd_ump_endpoint_info_t structure
 * \param src source snd_ump_endpoint_info_t structure
 */
void snd_ump_endpoint_info_copy(snd_ump_endpoint_info_t *dst,
				const snd_ump_endpoint_info_t *src)
{
	*dst = *src;
}

/**
 * \brief get card number of UMP endpoint
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return the card number of the given UMP endpoint
 */
int snd_ump_endpoint_info_get_card(const snd_ump_endpoint_info_t *info)
{
	return info->card;
}

/**
 * \brief get device number of UMP endpoint
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return the device number of the given UMP endpoint
 */
int snd_ump_endpoint_info_get_device(const snd_ump_endpoint_info_t *info)
{
	return info->device;
}

/**
 * \brief get UMP endpoint info flags
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return UMP endpoint flag bits
 */
unsigned int snd_ump_endpoint_info_get_flags(const snd_ump_endpoint_info_t *info)
{
	return info->flags;
}

/**
 * \brief get UMP endpoint protocol capability bits
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return UMP endpoint protocol capability bits
 */
unsigned int snd_ump_endpoint_info_get_protocol_caps(const snd_ump_endpoint_info_t *info)
{
	return info->protocol_caps;
}

/**
 * \brief get the current UMP endpoint protocol
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return UMP endpoint protocol bits
 */
unsigned int snd_ump_endpoint_info_get_protocol(const snd_ump_endpoint_info_t *info)
{
	return info->protocol;
}

/**
 * \brief get the number of UMP blocks belonging to the endpoint
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return number of UMP blocks
 */
unsigned int snd_ump_endpoint_info_get_num_blocks(const snd_ump_endpoint_info_t *info)
{
	return info->num_blocks;
}

/**
 * \brief get UMP version number
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return UMP version number
 */
unsigned int snd_ump_endpoint_info_get_version(const snd_ump_endpoint_info_t *info)
{
	return info->version;
}

/**
 * \brief get UMP endpoint name string
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return UMP endpoint name string
 */
const char *snd_ump_endpoint_info_get_name(const snd_ump_endpoint_info_t *info)
{
	return (const char *)info->name;
}

/**
 * \brief get UMP endpoint product ID string
 * \param info pointer to a snd_ump_endpoint_info_t structure
 * \return UMP endpoint product ID string
 */
const char *snd_ump_endpoint_info_get_product_id(const snd_ump_endpoint_info_t *info)
{
	return (const char *)info->product_id;
}

/**
 * \brief get endpoint information about UMP handle
 * \param ump UMP handle
 * \param info pointer to a snd_ump_endpoint_info_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_ump_endpoint_info(snd_ump_t *ump, snd_ump_endpoint_info_t *info)
{
	return _snd_rawmidi_ump_endpoint_info(ump->rawmidi, info);
}

/**
 * \brief get size of the snd_ump_block_info_t structure in bytes
 * \return size of the snd_ump_block_info_t structure in bytes
 */
size_t snd_ump_block_info_sizeof(void)
{
	return sizeof(snd_ump_block_info_t);
}

/**
 * \brief allocate the snd_ump_block_info_t structure
 * \param ptr returned pointer
 * \return 0 on success otherwise a negative error code if fails
 *
 * Allocates a new snd_ump_block_info_t structure using the standard
 * malloc C library function.
 */
int snd_ump_block_info_malloc(snd_ump_block_info_t **info)
{
	*info = calloc(1, sizeof(snd_ump_block_info_t));
	if (!*info)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees the snd_ump_block_info_t structure
 * \param status pointer to the snd_ump_block_info_t structure to free
 *
 * Frees the given snd_ump_block_info_t structure using the standard
 * free C library function.
 */
void snd_ump_block_info_free(snd_ump_block_info_t *info)
{
	free(info);
}

/**
 * \brief copy one snd_ump_block_info_t structure to another
 * \param dst destination snd_ump_block_info_t structure
 * \param src source snd_ump_block_info_t structure
 */
void snd_ump_block_info_copy(snd_ump_block_info_t *dst,
			     const snd_ump_block_info_t *src)
{
	*dst = *src;
}

/**
 * \brief get card number of UMP block
 * \param info pointer to a snd_ump_block_info_t structure
 * \return the card number of the given UMP block
 */
int snd_ump_block_info_get_card(const snd_ump_block_info_t *info)
{
	return info->card;
}

/**
 * \brief get device number of UMP block
 * \param info pointer to a snd_ump_block_info_t structure
 * \return the device number of the given UMP block
 */
int snd_ump_block_info_get_device(const snd_ump_block_info_t *info)
{
	return info->device;
}

/**
 * \brief get UMP block ID
 * \param info pointer to a snd_ump_block_info_t structure
 * \return ID number of the given UMP block
 */
unsigned int snd_ump_block_info_get_block_id(const snd_ump_block_info_t *info)
{
	return info->block_id;
}

/**
 * \brief set UMP block ID for query
 * \param info pointer to a snd_ump_block_info_t structure
 * \param id the ID number for query
 */
void snd_ump_block_info_set_block_id(snd_ump_block_info_t *info,
				     unsigned int id)
{
	info->block_id = id;
}

/**
 * \brief get UMP block activeness
 * \param info pointer to a snd_ump_block_info_t structure
 * \return 1 if the block is active or 0 if inactive
 */
unsigned int snd_ump_block_info_get_active(const snd_ump_block_info_t *info)
{
	return info->active;
}

/**
 * \brief get UMP block information flags
 * \param info pointer to a snd_ump_block_info_t structure
 * \return info flag bits for the given UMP block
 */
unsigned int snd_ump_block_info_get_flags(const snd_ump_block_info_t *info)
{
	return info->flags;
}

/**
 * \brief get UMP block direction
 * \param info pointer to a snd_ump_block_info_t structure
 * \return direction of UMP block (input,output,bidirectional)
 */
unsigned int snd_ump_block_info_get_direction(const snd_ump_block_info_t *info)
{
	return info->direction;
}

/**
 * \brief get first UMP group ID belonging to the block
 * \param info pointer to a snd_ump_block_info_t structure
 * \return the first UMP group ID belonging to the block
 */
unsigned int snd_ump_block_info_get_first_group(const snd_ump_block_info_t *info)
{
	return info->first_group;
}

/**
 * \brief get number of UMP groups belonging to the block
 * \param info pointer to a snd_ump_block_info_t structure
 * \return the number of UMP groups belonging to the block
 */
unsigned int snd_ump_block_info_get_num_groups(const snd_ump_block_info_t *info)
{
	return info->num_groups;
}

/**
 * \brief get the name string of UMP block
 * \param info pointer to a snd_ump_block_info_t structure
 * \return the name string of UMP block
 */
const char *snd_ump_block_info_get_name(const snd_ump_block_info_t *info)
{
	return (const char *)info->name;
}

/**
 * \brief get UMP block information
 * \param info pointer to a snd_ump_block_info_t structure
 * \return 0 on success otherwise a negative error code
 *
 * The caller should fill the block ID to query at first via
 * snd_ump_block_info_set_block_id().
 */
int snd_ump_block_info(snd_ump_t *ump, snd_ump_block_info_t *info)
{
	return _snd_rawmidi_ump_block_info(ump->rawmidi, info);
}
