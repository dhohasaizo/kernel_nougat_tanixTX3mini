/*
 * drivers/amlogic/display/osd/osd_sync.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/


#ifndef _OSD_SYNC_H_
#define _OSD_SYNC_H_

enum {
	GLES_COMPOSE_MODE = 0,
	DIRECT_COMPOSE_MODE = 1,
	GE2D_COMPOSE_MODE = 2,
};

struct fb_sync_request_s {
	unsigned int xoffset;
	unsigned int yoffset;
	int in_fen_fd;
	int out_fen_fd;
};

struct fb_sync_request_render_s {
	unsigned int    xoffset;
	unsigned int    yoffset;
	int             in_fen_fd;
	int             out_fen_fd;
	int             width;
	int             height;
	int             format;
	int             shared_fd;
	unsigned int    op;
	unsigned int    type; /*direct render or ge2d*/
	unsigned int    dst_x;
	unsigned int    dst_y;
	unsigned int    dst_w;
	unsigned int    dst_h;
	int				byte_stride;
	int				pxiel_stride;
	unsigned int    reserve;
};
#endif
