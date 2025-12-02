/* -*- mode: c; tab-width: 4; c-basic-offset: 4; c-file-style: "linux" -*- */
//
// Copyright (c) 2009-2011, Wei Mingzhi <whistler_wmz@users.sf.net>.
// Copyright (c) 2011-2024, SDLPAL development team.
// All rights reserved.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Portions based on PalLibrary by Lou Yihua <louyihua@21cn.com>.
// Copyright (c) 2006-2007, Lou Yihua.
//
// Ported to C from C++ and modified for compatibility with Big-Endian
// by Wei Mingzhi <whistler_wmz@users.sf.net>.
//

#include "common.h"

typedef struct _YJ2_TreeNode
{
	unsigned short      weight;
	unsigned short      value;
	struct _YJ2_TreeNode   *parent;
	struct _YJ2_TreeNode   *left;
	struct _YJ2_TreeNode   *right;
} YJ2_TreeNode;

typedef struct _YJ2_Tree
{
	YJ2_TreeNode    *node;
	YJ2_TreeNode   **list;
} YJ2_Tree;

static unsigned char yj2_data1[0x100] =
{
	0x3f, 0x0b, 0x17, 0x03, 0x2f, 0x0a, 0x16, 0x00, 0x2e, 0x09, 0x15, 0x02, 0x2d, 0x01, 0x08, 0x00,
	0x3e, 0x07, 0x14, 0x03, 0x2c, 0x06, 0x13, 0x00, 0x2b, 0x05, 0x12, 0x02, 0x2a, 0x01, 0x04, 0x00,
	0x3d, 0x0b, 0x11, 0x03, 0x29, 0x0a, 0x10, 0x00, 0x28, 0x09, 0x0f, 0x02, 0x27, 0x01, 0x08, 0x00,
	0x3c, 0x07, 0x0e, 0x03, 0x26, 0x06, 0x0d, 0x00, 0x25, 0x05, 0x0c, 0x02, 0x24, 0x01, 0x04, 0x00,
	0x3b, 0x0b, 0x17, 0x03, 0x23, 0x0a, 0x16, 0x00, 0x22, 0x09, 0x15, 0x02, 0x21, 0x01, 0x08, 0x00,
	0x3a, 0x07, 0x14, 0x03, 0x20, 0x06, 0x13, 0x00, 0x1f, 0x05, 0x12, 0x02, 0x1e, 0x01, 0x04, 0x00,
	0x39, 0x0b, 0x11, 0x03, 0x1d, 0x0a, 0x10, 0x00, 0x1c, 0x09, 0x0f, 0x02, 0x1b, 0x01, 0x08, 0x00,
	0x38, 0x07, 0x0e, 0x03, 0x1a, 0x06, 0x0d, 0x00, 0x19, 0x05, 0x0c, 0x02, 0x18, 0x01, 0x04, 0x00,
	0x37, 0x0b, 0x17, 0x03, 0x2f, 0x0a, 0x16, 0x00, 0x2e, 0x09, 0x15, 0x02, 0x2d, 0x01, 0x08, 0x00,
	0x36, 0x07, 0x14, 0x03, 0x2c, 0x06, 0x13, 0x00, 0x2b, 0x05, 0x12, 0x02, 0x2a, 0x01, 0x04, 0x00,
	0x35, 0x0b, 0x11, 0x03, 0x29, 0x0a, 0x10, 0x00, 0x28, 0x09, 0x0f, 0x02, 0x27, 0x01, 0x08, 0x00,
	0x34, 0x07, 0x0e, 0x03, 0x26, 0x06, 0x0d, 0x00, 0x25, 0x05, 0x0c, 0x02, 0x24, 0x01, 0x04, 0x00,
	0x33, 0x0b, 0x17, 0x03, 0x23, 0x0a, 0x16, 0x00, 0x22, 0x09, 0x15, 0x02, 0x21, 0x01, 0x08, 0x00,
	0x32, 0x07, 0x14, 0x03, 0x20, 0x06, 0x13, 0x00, 0x1f, 0x05, 0x12, 0x02, 0x1e, 0x01, 0x04, 0x00,
	0x31, 0x0b, 0x11, 0x03, 0x1d, 0x0a, 0x10, 0x00, 0x1c, 0x09, 0x0f, 0x02, 0x1b, 0x01, 0x08, 0x00,
	0x30, 0x07, 0x0e, 0x03, 0x1a, 0x06, 0x0d, 0x00, 0x19, 0x05, 0x0c, 0x02, 0x18, 0x01, 0x04, 0x00
};
static unsigned char yj2_data2[0x10] =
{
	0x08, 0x05, 0x06, 0x04, 0x07, 0x05, 0x06, 0x03, 0x07, 0x05, 0x06, 0x04, 0x07, 0x04, 0x05, 0x03
};

static void yj2_adjust_tree(YJ2_Tree tree, unsigned short value)
{
	YJ2_TreeNode* node = tree.list[value];
	YJ2_TreeNode tmp;
	YJ2_TreeNode* tmp1;
	YJ2_TreeNode* temp;
	while (node->value != 0x280)
	{
		temp = node + 1;
		while (node->weight == temp->weight)
			temp++;
		temp--;
		if (temp != node)
		{
			tmp1 = node->parent;
			node->parent = temp->parent;
			temp->parent = tmp1;
			if (node->value > 0x140)
			{
				node->left->parent = temp;
				node->right->parent = temp;
			}
			else
				tree.list[node->value] = temp;
			if (temp->value > 0x140)
			{
				temp->left->parent = node;
				temp->right->parent = node;
			}
			else
				tree.list[temp->value] = node;
			tmp = *node; *node = *temp; *temp = tmp;
			node = temp;
		}
		node->weight++;
		node = node->parent;
	}
	node->weight++;
}

static int yj2_build_tree(YJ2_Tree *tree)
{
	int i, ptr;
	YJ2_TreeNode** list;
	YJ2_TreeNode* node;
	if ((tree->list = list = (YJ2_TreeNode **)malloc(sizeof(YJ2_TreeNode*) * 321)) == NULL)
		return 0;
	if ((tree->node = node = (YJ2_TreeNode *)malloc(sizeof(YJ2_TreeNode) * 641)) == NULL)
	{
		free(list);
		return 0;
	}
	memset(list, 0, 321 * sizeof(YJ2_TreeNode*));
	memset(node, 0, 641 * sizeof(YJ2_TreeNode));
	for (i = 0; i <= 0x140; i++)
		list[i] = node + i;
	for (i = 0; i <= 0x280; i++)
	{
		node[i].value = i;
		node[i].weight = 1;
	}
	tree->node[0x280].parent = tree->node + 0x280;
	for (i = 0, ptr = 0x141; ptr <= 0x280; i += 2, ptr++)
	{
		node[ptr].left = node + i;
		node[ptr].right = node + i + 1;
		node[i].parent = node[i + 1].parent = node + ptr;
		node[ptr].weight = node[i].weight + node[i + 1].weight;
	}
	return 1;
}

static int yj2_bt(const unsigned char* data, unsigned int pos)
{
	return (data[pos >> 3] & (unsigned char)(1 << (pos & 0x7))) >> (pos & 0x7);
}

// YJ2_Decompress
INT Decompress(const VOID *Source, VOID *Destination, INT DestSize) {
	int Length;
	unsigned int ptr = 0;
	unsigned char* src = (unsigned char*)Source + 4;
	unsigned char* dest;
	YJ2_Tree tree;
	YJ2_TreeNode* node;

	if (Source == NULL)
		return -1;

	if (!yj2_build_tree(&tree))
		return -1;

	Length = SDL_Swap32LE(*((unsigned int*)Source));
	if (Length > DestSize)
		return -1;
	dest = (unsigned char*)Destination;

	while (1)
	{
		unsigned short val;
		node = tree.node + 0x280;
		while (node->value > 0x140)
		{
			if (yj2_bt(src, ptr))
				node = node->right;
			else
				node = node->left;
			ptr++;
		}
		val = node->value;
		if (tree.node[0x280].weight == 0x8000)
		{
			int i;
			for (i = 0; i < 0x141; i++)
				if (tree.list[i]->weight & 0x1)
					yj2_adjust_tree(tree, i);
			for (i = 0; i <= 0x280; i++)
				tree.node[i].weight >>= 1;
		}
		yj2_adjust_tree(tree, val);
		if (val > 0xff)
		{
			int i;
			unsigned int temp, tmp, pos;
			unsigned char* pre;
			for (i = 0, temp = 0; i < 8; i++, ptr++)
				temp |= (unsigned int)yj2_bt(src, ptr) << i;
			tmp = temp & 0xff;
			for (; i < yj2_data2[tmp & 0xf] + 6; i++, ptr++)
				temp |= (unsigned int)yj2_bt(src, ptr) << i;
			temp >>= yj2_data2[tmp & 0xf];
			pos = (temp & 0x3f) | ((unsigned int)yj2_data1[tmp] << 6);
			if (pos == 0xfff)
				break;
			pre = dest - pos - 1;
			for (i = 0; i < val - 0xfd; i++)
				*dest++ = *pre++;
		}
		else
		{
			*dest++ = (unsigned char)val;
		}
	}

	free(tree.list);
	free(tree.node);
	return Length;
}
