/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * A tree container.
 * @author Michael Niedermayer <michaelni@gmx.at>
 */

#ifndef AVUTIL_TREE_H
#define AVUTIL_TREE_H

#include "attributes.h"

struct AVTreeNode;
extern const int av_tree_node_size;

struct AVTreeNode *av_tree_node_alloc(void);

void *av_tree_find(const struct AVTreeNode *root, void *key,
                   int (*cmp)(const void *key, const void *b), void *next[2]);

void *av_tree_insert(struct AVTreeNode **rootp, void *key,
                     int (*cmp)(const void *key, const void *b),
                     struct AVTreeNode **next);

void av_tree_destroy(struct AVTreeNode *t);

void av_tree_enumerate(struct AVTreeNode *t, void *opaque,
                       int (*cmp)(void *opaque, void *elem),
                       int (*enu)(void *opaque, void *elem));

#endif
