//
//	srecord - manipulate eprom load files
//	Copyright (C) 2004-2006 Peter Miller;
//	All rights reserved.
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
//
// MANIFEST: functions to impliment the srec_input_file_aomf class
//


#include <lib/srec/input/file/aomf.h>
#include <lib/srec/record.h>


#define O96_Mod_Hdr                             0x02
#define O96_Mod_End                             0x04
#define O96_Content                             0x06
#define O96_Lin_Num                             0x08
#define O96_Block_Def                           0x0a
#define O96_Block_End                           0x0c
#define O96_Eof                                 0x0e
#define O96_Mod_Anc                             0x10
#define O96_Loc_Sym                             0x12
#define O96_Type_Def                            0x14
#define O96_Pub_Def                             0x16
#define O96_Ext_Def                             0x18
#define O96_Res_Type_1A                         0x1a
#define O96_Res_Type_1C                         0x1c
#define O96_Res_Type_1E                         0x1e
#define O96_Seg_Def                             0x20
#define O96_Fixup                               0x22
#define O96_Res_Type_24                         0x24
#define O96_Lib_Mod_Loc                         0x26
#define O96_Lib_Mod_Nam                         0x28
#define O96_Lib_Dic                             0x2a
#define O96_Res_Type_2C                         0x2c
#define O96_Lib_Hdr                             0x2e
#define O96_Res_Type_30                         0x30
#define O96_Res_Type_32                         0x32
#define O96_Max_Rec_Type                        0x32

static const char *
o96name(int x)
{
    switch (x)
    {
    case O96_Mod_Hdr:
	return "Mod_Hdr";

    case O96_Mod_End:
	return "Mod_End";

    case O96_Content:
	return "Content";

    case O96_Lin_Num:
	return "Lin_Num";

    case O96_Block_Def:
	return "Block_Def";

    case O96_Block_End:
	return "Block_End";

    case O96_Eof:
	return "Eof";

    case O96_Mod_Anc:
	return "Mod_Anc";

    case O96_Loc_Sym:
	return "Loc_Sym";

    case O96_Type_Def:
	return "Type_Def";

    case O96_Pub_Def:
	return "Pub_Def";

    case O96_Ext_Def:
	return "Ext_Def";

    case O96_Seg_Def:
	return "Seg_Def";

    case O96_Fixup:
	return "Fixup";

    case O96_Lib_Mod_Loc:
	return "Lib_Mod_Loc";

    case O96_Lib_Mod_Nam:
	return "Lib_Mod_Nam";

    case O96_Lib_Dic:
	return "Lib_Dic";

    case O96_Lib_Hdr:
	return "Lib_Hdr";
    }
    return "unknown";
}


srec_input_file_aomf::~srec_input_file_aomf()
{
    if (current_buffer)
	delete [] current_buffer;
}


srec_input_file_aomf::srec_input_file_aomf(const char *filename) :
    srec_input_file(filename),
    current_buffer(0),
    current_length(0),
    current_maximum(0),
    current_pos(0),
    current_address(0),
    state(expecting_header)
{
}


int
srec_input_file_aomf::get_byte()
{
    int c = get_char();
    if (c < 0)
	fatal_error("premature end-of-file");
    checksum_add(c);
    return c;
}


int
srec_input_file_aomf::get_word()
{
    // Little endian
    unsigned char n1 = get_byte();
    unsigned char n2 = get_byte();
    unsigned short n = (n1 | (n2 << 8));
    return n;
}


int
srec_input_file_aomf::slurp()
{
    current_pos = 0;
    current_length = 0;
    if (peek_char() < 0)
	return -1;
    checksum_reset();
    int type = get_byte();
    size_t length = get_word();
    if (length == 0)
	fatal_error("invalid record length");
    --length; // includes checksum byte
    if (length > current_maximum)
    {
	if (current_buffer)
	    delete [] current_buffer;
	while (current_maximum < length)
	    current_maximum = current_maximum * 2 + 64;
	current_buffer = new unsigned char [current_maximum];
    }
    current_length = length;
    for (size_t j = 0; j < length; ++j)
	current_buffer[j] = get_byte();
    get_byte();
    if (use_checksums() && checksum_get() != 0)
	fatal_error("checksum mismatch");
    return type;
}


int
srec_input_file_aomf::read(srec_record &record)
{
    for (;;)
    {
	unsigned char c;
	switch (state)
	{
	case expecting_header:
	    if (slurp() != O96_Mod_Hdr)
		fatal_error("Module Header Record expected");
	    state = expecting_data;
	    if (current_length > 0)
	    {
		unsigned nbytes = current_buffer[0];
		// should be exactly (current_length-3)
		if (nbytes > current_length - 1)
		    nbytes = current_length - 1;
		record =
		    srec_record
		    (
			srec_record::type_header,
			0, // address
			current_buffer + 1,
			nbytes
		    );
	    }
	    else
		record = srec_record(srec_record::type_header, 0, 0, 0);
	    current_length = 0;
	    return 1;

	case expecting_eof:
	    if (slurp() >= 0)
		fatal_error("end-of-file expected");
	    return 0;

	case expecting_data:
	    if (current_pos < current_length)
	    {
		size_t nbytes = current_length - current_pos;
		if (nbytes > srec_record::max_data_length)
		    nbytes = srec_record::max_data_length;
		record =
		    srec_record
		    (
			srec_record::type_data,
			current_address,
			current_buffer + current_pos,
			nbytes
		    );
		current_pos += nbytes;
		current_address += nbytes;
		return 1;
	    }
	    c = slurp();
	    switch (c)
	    {
	    case O96_Mod_Hdr:
		fatal_error("too many Module Header Records");

	    case O96_Mod_End:
		state = expecting_eof;
		record = srec_record(srec_record::type_start_address, 0, 0, 0);
		return 1;

	    case O96_Content:
		if (current_length < 3)
		    fatal_error("malformed Content Record");
		current_address = 
		    (
			// strictly speaking, this byte should be ignored
			((unsigned long)current_buffer[0] << 16)
		    |
			// length is little-endian
			((unsigned long)current_buffer[2] << 8)
		    |
			((unsigned long)current_buffer[1])
		    );
		current_pos = 3;
		break;

	    case 0x01:
	    case 0x0E:
	    case O96_Loc_Sym:
	    case O96_Pub_Def:
	    case O96_Ext_Def:
	    case O96_Mod_Anc:
		// Ignore these silently
		current_length = 0;
		break;

	    default:
		// Nothing else should be in an AOMF file,
		// even though it may be valid in an OMF file.
		warning("ignoring %s record (type 0x%02X)", o96name(c), c);
		current_length = 0;
		break;
	    }
	    break;
	}
    }
}


const char *
srec_input_file_aomf::mode()
    const
{
    return "rb";
}


const char *
srec_input_file_aomf::get_file_format_name()
    const
{
    return "Intel Absolute Object Module Format (AOMF)";
}
