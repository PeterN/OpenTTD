/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file newgrf_townname.h
 * Header of Action 0F "universal holder" structure and functions
 */

#ifndef NEWGRF_TOWNNAME_H
#define NEWGRF_TOWNNAME_H

#include <vector>
#include "strings_type.h"

struct NamePart {
	byte prob;     ///< The relative probability of the following name to appear in the bottom 7 bits.
	union {
		char *text;    ///< If probability bit 7 is clear
		byte id;       ///< If probability bit 7 is set
	} data;
};

struct NamePartList {
	byte partcount;
	byte bitstart;
	byte bitcount;
	uint16_t maxprob;
	NamePart *parts;
};

struct GRFTownName {
	uint32_t grfid;
	byte nb_gen;
	byte id[128];
	StringID name[128];
	byte nbparts[128];
	NamePartList *partlist[128];
	GRFTownName *next;
};

GRFTownName *AddGRFTownName(uint32_t grfid);
GRFTownName *GetGRFTownName(uint32_t grfid);
void DelGRFTownName(uint32_t grfid);
void CleanUpGRFTownNames();
char *GRFTownNameGenerate(char *buf, uint32_t grfid, uint16_t gen, uint32_t seed, const char *last);
uint32_t GetGRFTownNameId(int gen);
uint16_t GetGRFTownNameType(int gen);
StringID GetGRFTownNameName(uint gen);

const std::vector<StringID>& GetGRFTownNameList();

#endif /* NEWGRF_TOWNNAME_H */
