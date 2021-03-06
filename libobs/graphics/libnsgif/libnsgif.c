/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 * Copyright 2004 Richard Wilson <richard.wilson@netsurf-browser.org>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 *
 * This file is part of NetSurf's libnsgif, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

//#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "libnsgif.h"


#ifdef NDEBUG
#  define LOG(x) ((void) 0)
#else
#  define LOG(x) do { fprintf(stderr, x), fputc('\n', stderr); } while (0)
#endif /* NDEBUG */



/*    READING GIF FILES
    =================

    The functions provided by this file allow for efficient progressive GIF
    decoding. Whilst the initialisation does not ensure that there is
    sufficient image data to complete the entire frame, it does ensure that
    the information provided is valid. Any subsequent attempts to decode an
    initialised GIF are guaranteed to succeed, and any bytes of the image
    not present are assumed to be totally transparent.

    To begin decoding a GIF, the 'gif' structure must be initialised with
    the 'gif_data' and 'buffer_size' set to their initial values. The
    'buffer_position' should initially be 0, and will be internally updated
    as the decoding commences. The caller should then repeatedly call
    gif_initialise() with the structure until the function returns 1, or
    no more data is available.

    Once the initialisation has begun, the decoder completes the variables
    'frame_count' and 'frame_count_partial'. The former being the total
    number of frames that have been successfully initialised, and the
    latter being the number of frames that a partial amount of data is
    available for. This assists the caller in managing the animation whilst
    decoding is continuing.

    To decode a frame, the caller must use gif_decode_frame() which updates
    the current 'frame_image' to reflect the desired frame. The required
    'disposal_method' is also updated to reflect how the frame should be
    plotted. The caller must not assume that the current 'frame_image' will
    be valid between calls if initialisation is still occurring, and should
    either always request that the frame is decoded (no processing will
    occur if the 'decoded_frame' has not been invalidated by initialisation)
    or perform the check itself.

    It should be noted that gif_finalise() should always be called, even if
    no frames were initialised.  Additionally, it is the responsibility of
    the caller to free 'gif_data'.

    [rjw] - Fri 2nd April 2004
*/

/*    TO-DO LIST
    =================

    + Plain text and comment extensions could be implemented if there is any
    interest in doing so.
*/




/*    Maximum colour table size
*/
#define GIF_MAX_COLOURS 256

/*    Internal flag that the colour table needs to be processed
*/
#define GIF_PROCESS_COLOURS 0xaa000000

/*    Internal flag that a frame is invalid/unprocessed
*/
#define GIF_INVALID_FRAME -1

/* Transparent colour
*/
#define GIF_TRANSPARENT_COLOUR 0x00

/*    GIF Flags
*/
#define GIF_FRAME_COMBINE 1
#define GIF_FRAME_CLEAR 2
#define GIF_FRAME_RESTORE 3
#define GIF_FRAME_QUIRKS_RESTORE 4
#define GIF_IMAGE_SEPARATOR 0x2c
#define GIF_INTERLACE_MASK 0x40
#define GIF_COLOUR_TABLE_MASK 0x80
#define GIF_COLOUR_TABLE_SIZE_MASK 0x07
#define GIF_EXTENSION_INTRODUCER 0x21
#define GIF_EXTENSION_GRAPHIC_CONTROL 0xf9
#define GIF_DISPOSAL_MASK 0x1c
#define GIF_TRANSPARENCY_MASK 0x01
#define GIF_EXTENSION_COMMENT 0xfe
#define GIF_EXTENSION_PLAIN_TEXT 0x01
#define GIF_EXTENSION_APPLICATION 0xff
#define GIF_BLOCK_TERMINATOR 0x00
#define GIF_TRAILER 0x3b

/*    Internal GIF routines
*/
static gif_result gif_initialise_sprite(gif_animation *gif, unsigned int width, unsigned int height);
static gif_result gif_initialise_frame(gif_animation *gif);
static gif_result gif_initialise_frame_extensions(gif_animation *gif, const int frame);
static gif_result gif_skip_frame_extensions(gif_animation *gif);
static unsigned int gif_interlaced_line(int height, int y);



/*    Internal LZW routines
*/
static void gif_init_LZW(gif_animation *gif);
static bool gif_next_LZW(gif_animation *gif);
static int gif_next_code(gif_animation *gif, int code_size);

static const int maskTbl[16] = {0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f,
                0x00ff, 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff};

/**    Initialises necessary gif_animation members.
*/
void gif_create(gif_animation *gif, gif_bitmap_callback_vt *bitmap_callbacks) {
    memset(gif, 0, sizeof(gif_animation));
    gif->bitmap_callbacks = *bitmap_callbacks;
    gif->decoded_frame = GIF_INVALID_FRAME;
}


/**    Initialises any workspace held by the animation and attempts to decode
    any information that hasn't already been decoded.
    If an error occurs, all previously decoded frames are retained.

    @return GIF_FRAME_DATA_ERROR for GIF frame data error
        GIF_INSUFFICIENT_FRAME_DATA for insufficient data to process
                  any more frames
        GIF_INSUFFICIENT_MEMORY for memory error
        GIF_DATA_ERROR for GIF error
        GIF_INSUFFICIENT_DATA for insufficient data to do anything
        GIF_OK for successful decoding
        GIF_WORKING for successful decoding if more frames are expected
*/
gif_result gif_initialise(gif_animation *gif, size_t size, unsigned char *data) {
    unsigned char *gif_data;
    unsigned int index;
    gif_result return_value;

    /*     The GIF format is thoroughly documented; a full description
     *    can be found at http://www.w3.org/Graphics/GIF/spec-gif89a.txt
    */

    /*    Initialize values
    */
    gif->buffer_size = (unsigned int)size;
    gif->gif_data = data;
    
    /*    Check for sufficient data to be a GIF (6-byte header + 7-byte logical screen descriptor)
    */
    if (gif->buffer_size < 13) return GIF_INSUFFICIENT_DATA;

    /*    Get our current processing position
    */
    gif_data = gif->gif_data + gif->buffer_position;

    /*    See if we should initialise the GIF
    */
    if (gif->buffer_position == 0) {

        /*    We want everything to be NULL before we start so we've no chance
            of freeing bad pointers (paranoia)
        */
        gif->frame_image = NULL;
        gif->frames = NULL;
        gif->local_colour_table = NULL;
        gif->global_colour_table = NULL;

        /*    The caller may have been lazy and not reset any values
        */
        gif->frame_count = 0;
        gif->frame_count_partial = 0;
        gif->decoded_frame = GIF_INVALID_FRAME;

        /* 6-byte GIF file header is:
         *
         *    +0    3CHARS    Signature ('GIF')
         *    +3    3CHARS    Version ('87a' or '89a')
         */
        if (strncmp((const char *) gif_data, "GIF", 3) != 0)
            return GIF_DATA_ERROR;
        gif_data += 3;

        /*    Ensure GIF reports version 87a or 89a
        */
/*        if ((strncmp(gif_data, "87a", 3) != 0) &&
                (strncmp(gif_data, "89a", 3) != 0))
            LOG(("Unknown GIF format - proceeding anyway"));
*/        gif_data += 3;

        /* 7-byte Logical Screen Descriptor is:
         *
         *    +0    SHORT    Logical Screen Width
         *    +2    SHORT    Logical Screen Height
         *    +4    CHAR    __Packed Fields__
         *             1BIT    Global Colour Table Flag
         *             3BITS    Colour Resolution
         *             1BIT    Sort Flag
         *             3BITS    Size of Global Colour Table
         *    +5    CHAR    Background Colour Index
         *    +6    CHAR    Pixel Aspect Ratio
         */
        gif->width = gif_data[0] | (gif_data[1] << 8);
        gif->height = gif_data[2] | (gif_data[3] << 8);
        gif->global_colours = (gif_data[4] & GIF_COLOUR_TABLE_MASK);
        gif->colour_table_size = (2 << (gif_data[4] & GIF_COLOUR_TABLE_SIZE_MASK));
        gif->background_index = gif_data[5];
        gif->aspect_ratio = gif_data[6];
        gif->loop_count = 1;
        gif_data += 7;

        /*    Some broken GIFs report the size as the screen size they were created in. As
            such, we detect for the common cases and set the sizes as 0 if they are found
            which results in the GIF being the maximum size of the frames.
        */
        if (((gif->width == 640) && (gif->height == 480)) ||
                ((gif->width == 640) && (gif->height == 512)) ||
                ((gif->width == 800) && (gif->height == 600)) ||
                ((gif->width == 1024) && (gif->height == 768)) ||
                ((gif->width == 1280) && (gif->height == 1024)) ||
                ((gif->width == 1600) && (gif->height == 1200)) ||
                ((gif->width == 0) || (gif->height == 0)) ||
                ((gif->width > 2048) || (gif->height > 2048))) {
            gif->width = 1;
            gif->height = 1;
        }

        /*    Allocate some data irrespective of whether we've got any colour tables. We
            always get the maximum size in case a GIF is lying to us. It's far better
            to give the wrong colours than to trample over some memory somewhere.
        */
        gif->global_colour_table = (unsigned int *)calloc(GIF_MAX_COLOURS, sizeof(int));
        gif->local_colour_table = (unsigned int *)calloc(GIF_MAX_COLOURS, sizeof(int));
        if ((gif->global_colour_table == NULL) || (gif->local_colour_table == NULL)) {
            gif_finalise(gif);
            return GIF_INSUFFICIENT_MEMORY;
        }

        /*    Set the first colour to a value that will never occur in reality so we
            know if we've processed it
        */
        gif->global_colour_table[0] = GIF_PROCESS_COLOURS;
        
        /*    Check if the GIF has no frame data (13-byte header + 1-byte termination block)
         *    Although generally useless, the GIF specification does not expressly prohibit this
         */
        if (gif->buffer_size == 14) {
            if (gif_data[0] == GIF_TRAILER)
                return GIF_OK;
            else
                return GIF_INSUFFICIENT_DATA;
        }

        /*    Initialise enough workspace for 4 frames initially
        */
        if ((gif->frames = (gif_frame *)malloc(sizeof(gif_frame))) == NULL) {
            gif_finalise(gif);
            return GIF_INSUFFICIENT_MEMORY;
        }
        gif->frame_holders = 1;

        /*    Initialise the sprite header
        */
        assert(gif->bitmap_callbacks.bitmap_create);
        if ((gif->frame_image = gif->bitmap_callbacks.bitmap_create(gif->width, gif->height)) == NULL) {
            gif_finalise(gif);
            return GIF_INSUFFICIENT_MEMORY;
        }

        /*    Remember we've done this now
        */
        gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);
    }

    /*    Do the colour map if we haven't already. As the top byte is always 0xff or 0x00
        depending on the transparency we know if it's been filled in.
    */
    if (gif->global_colour_table[0] == GIF_PROCESS_COLOURS) {
        /*    Check for a global colour map signified by bit 7
        */
        if (gif->global_colours) {
            if (gif->buffer_size < (gif->colour_table_size * 3 + 12)) {
                return GIF_INSUFFICIENT_DATA;
            }
            for (index = 0; index < gif->colour_table_size; index++) {
                /* Gif colour map contents are r,g,b.
                 *
                 * We want to pack them bytewise into the 
                 * colour table, such that the red component
                 * is in byte 0 and the alpha component is in
                 * byte 3.
                 */
                unsigned char *entry = (unsigned char *) &gif->
                        global_colour_table[index];

                entry[0] = gif_data[0];    /* r */
                entry[1] = gif_data[1];    /* g */
                entry[2] = gif_data[2];    /* b */
                entry[3] = 0xff;    /* a */

                gif_data += 3;
            }
            gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);
        } else {
            /*    Create a default colour table with the first two colours as black and white
            */
            unsigned int *entry = gif->global_colour_table;

            entry[0] = 0x00000000;
            /* Force Alpha channel to opaque */
            ((unsigned char *) entry)[3] = 0xff;

            entry[1] = 0xffffffff;
        }
    }

    /*    Repeatedly try to initialise frames
    */
    while ((return_value = gif_initialise_frame(gif)) == GIF_WORKING);

    /*    If there was a memory error tell the caller
    */
    if ((return_value == GIF_INSUFFICIENT_MEMORY) ||
            (return_value == GIF_DATA_ERROR))
        return return_value;

    /*    If we didn't have some frames then a GIF_INSUFFICIENT_DATA becomes a
        GIF_INSUFFICIENT_FRAME_DATA
    */
    if ((return_value == GIF_INSUFFICIENT_DATA) && (gif->frame_count_partial > 0))
        return GIF_INSUFFICIENT_FRAME_DATA;

    /*    Return how many we got
    */
    return return_value;
}


/**    Updates the sprite memory size

    @return GIF_INSUFFICIENT_MEMORY for a memory error
        GIF_OK for success
*/
static gif_result gif_initialise_sprite(gif_animation *gif, unsigned int width, unsigned int height) {
    unsigned int max_width;
    unsigned int max_height;
    struct bitmap *buffer;

    /*    Check if we've changed
    */
    if ((width <= gif->width) && (height <= gif->height))
        return GIF_OK;

    /*    Get our maximum values
    */
    max_width = (width > gif->width) ? width : gif->width;
    max_height = (height > gif->height) ? height : gif->height;

    /*    Allocate some more memory
    */
    assert(gif->bitmap_callbacks.bitmap_create);
    if ((buffer = gif->bitmap_callbacks.bitmap_create(max_width, max_height)) == NULL)
        return GIF_INSUFFICIENT_MEMORY;
    assert(gif->bitmap_callbacks.bitmap_destroy);
    gif->bitmap_callbacks.bitmap_destroy(gif->frame_image);
    gif->frame_image = buffer;
    gif->width = max_width;
    gif->height = max_height;

    /*    Invalidate our currently decoded image
    */
    gif->decoded_frame = GIF_INVALID_FRAME;
    return GIF_OK;
}


/**    Attempts to initialise the next frame

    @return GIF_INSUFFICIENT_DATA for insufficient data to do anything
        GIF_FRAME_DATA_ERROR for GIF frame data error
        GIF_INSUFFICIENT_MEMORY for insufficient memory to process
        GIF_INSUFFICIENT_FRAME_DATA for insufficient data to complete the frame
        GIF_DATA_ERROR for GIF error (invalid frame header)
        GIF_OK for successful decoding
        GIF_WORKING for successful decoding if more frames are expected
*/
static gif_result gif_initialise_frame(gif_animation *gif) {
    int frame;
    gif_frame *temp_buf;

    unsigned char *gif_data, *gif_end;
    int gif_bytes;
    unsigned int flags = 0;
    unsigned int width, height, offset_x, offset_y;
    unsigned int block_size, colour_table_size;
    bool first_image = true;
    gif_result return_value;

    /*    Get the frame to decode and our data position
    */
    frame = gif->frame_count;

    /*    Get our buffer position etc.
    */
    gif_data = (unsigned char *)(gif->gif_data + gif->buffer_position);
    gif_end = (unsigned char *)(gif->gif_data + gif->buffer_size);
    gif_bytes = (unsigned int)(gif_end - gif_data);

    /*    Check if we've finished
    */
    if ((gif_bytes > 0) && (gif_data[0] == GIF_TRAILER)) return GIF_OK;
    
    /*    Check if we have enough data
     *    The shortest block of data is a 4-byte comment extension + 1-byte block terminator + 1-byte gif trailer
    */
    if (gif_bytes < 6) return GIF_INSUFFICIENT_DATA;

    /*    We could theoretically get some junk data that gives us millions of frames, so
        we ensure that we don't have a silly number
    */
    if (frame > 4096) return GIF_FRAME_DATA_ERROR;

    /*    Get some memory to store our pointers in etc.
    */
    if ((int)gif->frame_holders <= frame) {
        /*    Allocate more memory
        */
        if ((temp_buf = (gif_frame *)realloc(gif->frames,
                    (frame + 1) * sizeof(gif_frame))) == NULL)
            return GIF_INSUFFICIENT_MEMORY;
        gif->frames = temp_buf;
        gif->frame_holders = frame + 1;
    }

    /*    Store our frame pointer. We would do it when allocating except we
        start off with one frame allocated so we can always use realloc.
    */
    gif->frames[frame].frame_pointer = gif->buffer_position;
    gif->frames[frame].display = false;
    gif->frames[frame].virgin = true;
    gif->frames[frame].disposal_method = 0;
    gif->frames[frame].transparency = false;
    gif->frames[frame].frame_delay = 100;
    gif->frames[frame].redraw_required = false;

    /*    Invalidate any previous decoding we have of this frame
    */
    if (gif->decoded_frame == frame)
        gif->decoded_frame = GIF_INVALID_FRAME;

    /*    We pretend to initialise the frames, but really we just skip over all
        the data contained within. This is all basically a cut down version of
        gif_decode_frame that doesn't have any of the LZW bits in it.
    */

    /*    Initialise any extensions
    */
    gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);
    if ((return_value = gif_initialise_frame_extensions(gif, frame)) != GIF_OK)
        return return_value;
    gif_data = (gif->gif_data + gif->buffer_position);
    gif_bytes = (unsigned int)(gif_end - gif_data);

    /*    Check if we've finished
    */
    if ((gif_bytes = (unsigned int)(gif_end - gif_data)) < 1)
        return GIF_INSUFFICIENT_FRAME_DATA;
    else if (gif_data[0] == GIF_TRAILER) {
        gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);
        gif->frame_count = frame + 1;
        return GIF_OK;
    }

    /*    If we're not done, there should be an image descriptor
    */
    if (gif_data[0] != GIF_IMAGE_SEPARATOR) return GIF_FRAME_DATA_ERROR;

    /*    Do some simple boundary checking
    */
    offset_x = gif_data[1] | (gif_data[2] << 8);
    offset_y = gif_data[3] | (gif_data[4] << 8);
    width = gif_data[5] | (gif_data[6] << 8);
    height = gif_data[7] | (gif_data[8] << 8);

    /*    Set up the redraw characteristics. We have to check for extending the area
        due to multi-image frames.
    */
    if (!first_image) {
        if (gif->frames[frame].redraw_x > offset_x) {
            gif->frames[frame].redraw_width += (gif->frames[frame].redraw_x - offset_x);
            gif->frames[frame].redraw_x = offset_x;
        }
        if (gif->frames[frame].redraw_y > offset_y) {
            gif->frames[frame].redraw_height += (gif->frames[frame].redraw_y - offset_y);
            gif->frames[frame].redraw_y = offset_y;
        }
        if ((offset_x + width) > (gif->frames[frame].redraw_x + gif->frames[frame].redraw_width))
            gif->frames[frame].redraw_width = (offset_x + width) - gif->frames[frame].redraw_x;
        if ((offset_y + height) > (gif->frames[frame].redraw_y + gif->frames[frame].redraw_height))
            gif->frames[frame].redraw_height = (offset_y + height) - gif->frames[frame].redraw_y;
    } else {
        first_image = false;
        gif->frames[frame].redraw_x = offset_x;
        gif->frames[frame].redraw_y = offset_y;
        gif->frames[frame].redraw_width = width;
        gif->frames[frame].redraw_height = height;
    }

    /*    if we are clearing the background then we need to redraw enough to cover the previous
        frame too
    */
    gif->frames[frame].redraw_required = ((gif->frames[frame].disposal_method == GIF_FRAME_CLEAR) ||
                        (gif->frames[frame].disposal_method == GIF_FRAME_RESTORE));

    /*    Boundary checking - shouldn't ever happen except with junk data
    */
    if (gif_initialise_sprite(gif, (offset_x + width), (offset_y + height)))
        return GIF_INSUFFICIENT_MEMORY;

    /*    Decode the flags
    */
    flags = gif_data[9];
    colour_table_size = 2 << (flags & GIF_COLOUR_TABLE_SIZE_MASK);

    /*    Move our data onwards and remember we've got a bit of this frame
    */
    gif_data += 10;
    gif_bytes = (unsigned int)(gif_end - gif_data);
    gif->frame_count_partial = frame + 1;

    /*    Skip the local colour table
    */
    if (flags & GIF_COLOUR_TABLE_MASK) {
        gif_data += 3 * colour_table_size;
        if ((gif_bytes = (unsigned int)(gif_end - gif_data)) < 0)
            return GIF_INSUFFICIENT_FRAME_DATA;
    }

    /*    Ensure we have a correct code size
    */
    if (gif_data[0] > GIF_MAX_LZW)
        return GIF_DATA_ERROR;

    /*    Move our pointer to the actual image data
    */
    gif_data++;
    if (--gif_bytes < 0)
        return GIF_INSUFFICIENT_FRAME_DATA;

    /*    Repeatedly skip blocks until we get a zero block or run out of data
     *    These blocks of image data are processed later by gif_decode_frame()
    */
    block_size = 0;
    while (block_size != 1) {
        block_size = gif_data[0] + 1;
        /*    Check if the frame data runs off the end of the file
        */
        if ((int)(gif_bytes - block_size) < 0) {
            /*    Try to recover by signaling the end of the gif.
             *    Once we get garbage data, there is no logical
             *    way to determine where the next frame is.
             *    It's probably better to partially load the gif
             *    than not at all.
            */
            if (gif_bytes >= 2) {
                gif_data[0] = 0;
                gif_data[1] = GIF_TRAILER;
                gif_bytes = 1;
                ++gif_data;
                break;
            } else
                return GIF_INSUFFICIENT_FRAME_DATA;
        } else {
            gif_bytes -= block_size;
            gif_data += block_size;
        }
    }

    /*    Add the frame and set the display flag
    */
    gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);
    gif->frame_count = frame + 1;
    gif->frames[frame].display = true;

    /*    Check if we've finished
    */
    if (gif_bytes < 1)
        return GIF_INSUFFICIENT_FRAME_DATA;
    else
        if (gif_data[0] == GIF_TRAILER) return GIF_OK;
    return GIF_WORKING;
}

/**    Attempts to initialise the frame's extensions

    @return GIF_INSUFFICIENT_FRAME_DATA for insufficient data to complete the frame
        GIF_OK for successful initialisation
*/
static gif_result gif_initialise_frame_extensions(gif_animation *gif, const int frame) {
    unsigned char *gif_data, *gif_end;
    int gif_bytes;
    unsigned int block_size;

    /*    Get our buffer position etc.
    */
    gif_data = (unsigned char *)(gif->gif_data + gif->buffer_position);
    gif_end = (unsigned char *)(gif->gif_data + gif->buffer_size);
    
    /*    Initialise the extensions
    */
    while (gif_data[0] == GIF_EXTENSION_INTRODUCER) {
        ++gif_data;
        gif_bytes = (unsigned int)(gif_end - gif_data);

        /*    Switch on extension label
        */
        switch(gif_data[0]) {
            /* 6-byte Graphic Control Extension is:
             *
             *    +0    CHAR    Graphic Control Label
             *    +1    CHAR    Block Size
             *    +2    CHAR    __Packed Fields__
             *            3BITS    Reserved
             *            3BITS    Disposal Method
             *            1BIT    User Input Flag
             *            1BIT    Transparent Color Flag
             *    +3    SHORT    Delay Time
             *    +5    CHAR    Transparent Color Index
            */
            case GIF_EXTENSION_GRAPHIC_CONTROL:
                if (gif_bytes < 6) return GIF_INSUFFICIENT_FRAME_DATA;
                gif->frames[frame].frame_delay = gif_data[3] | (gif_data[4] << 8);
                if (gif_data[2] & GIF_TRANSPARENCY_MASK) {
                    gif->frames[frame].transparency = true;
                    gif->frames[frame].transparency_index = gif_data[5];
                }
                gif->frames[frame].disposal_method = ((gif_data[2] & GIF_DISPOSAL_MASK) >> 2);
                /*    I have encountered documentation and GIFs in the wild that use
                 *    0x04 to restore the previous frame, rather than the officially
                 *    documented 0x03.  I believe some (older?) software may even actually
                 *    export this way.  We handle this as a type of "quirks" mode.
                */
                if (gif->frames[frame].disposal_method == GIF_FRAME_QUIRKS_RESTORE)
                    gif->frames[frame].disposal_method = GIF_FRAME_RESTORE;
                gif_data += (2 + gif_data[1]);
                break;

            /* 14-byte+ Application Extension is:
             *
             *    +0    CHAR    Application Extension Label
             *    +1    CHAR    Block Size
             *    +2    8CHARS    Application Identifier
             *    +10    3CHARS    Appl. Authentication Code
             *    +13    1-256    Application Data (Data sub-blocks)
            */
            case GIF_EXTENSION_APPLICATION:
                if (gif_bytes < 17) return GIF_INSUFFICIENT_FRAME_DATA;
                if ((gif_data[1] == 0x0b) &&
                    (strncmp((const char *) gif_data + 2,
                        "NETSCAPE2.0", 11) == 0) &&
                    (gif_data[13] == 0x03) &&
                    (gif_data[14] == 0x01)) {
                        gif->loop_count = gif_data[15] | (gif_data[16] << 8);
                }
                gif_data += (2 + gif_data[1]);
                break;

            /*    Move the pointer to the first data sub-block
             *    Skip 1 byte for the extension label
            */
            case GIF_EXTENSION_COMMENT:
                ++gif_data;
                break;

            /*    Move the pointer to the first data sub-block
             *    Skip 2 bytes for the extension label and size fields
             *    Skip the extension size itself
            */
            default:
                gif_data += (2 + gif_data[1]);
        }

        /*    Repeatedly skip blocks until we get a zero block or run out of data
         *    This data is ignored by this gif decoder
        */
        gif_bytes = (unsigned int)(gif_end - gif_data);
        block_size = 0;
        while (gif_data[0] != GIF_BLOCK_TERMINATOR) {
            block_size = gif_data[0] + 1;
            if ((gif_bytes -= block_size) < 0)
                return GIF_INSUFFICIENT_FRAME_DATA;
            gif_data += block_size;
        }
        ++gif_data;
    }

    /*    Set buffer position and return
    */
    gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);
    return GIF_OK;
}


/**    Decodes a GIF frame.

    @return GIF_FRAME_DATA_ERROR for GIF frame data error
        GIF_INSUFFICIENT_FRAME_DATA for insufficient data to complete the frame
        GIF_DATA_ERROR for GIF error (invalid frame header)
        GIF_INSUFFICIENT_DATA for insufficient data to do anything
        GIF_INSUFFICIENT_MEMORY for insufficient memory to process
        GIF_OK for successful decoding
        If a frame does not contain any image data, GIF_OK is returned and
            gif->current_error is set to GIF_FRAME_NO_DISPLAY
*/
gif_result gif_decode_frame(gif_animation *gif, unsigned int frame) {
    unsigned int index = 0;
    unsigned char *gif_data, *gif_end;
    int gif_bytes;
    unsigned int width, height, offset_x, offset_y;
    unsigned int flags, colour_table_size, interlace;
    unsigned int *colour_table;
    unsigned int *frame_data = 0;    // Set to 0 for no warnings
    unsigned int *frame_scanline;
    unsigned int save_buffer_position;
    gif_result return_value = GIF_OK;
    unsigned int x, y, decode_y, burst_bytes;
    int last_undisposed_frame = (frame - 1);
    register unsigned char colour;

    /*    Ensure we have a frame to decode
    */
    if (frame >= gif->frame_count_partial)
        return GIF_INSUFFICIENT_DATA;

    /*    Ensure this frame is supposed to be decoded
    */
    if (gif->frames[frame].display == false) {
        gif->current_error = GIF_FRAME_NO_DISPLAY;
        return GIF_OK;
    }

    if ((!gif->clear_image) && ((int)frame == gif->decoded_frame))
        return GIF_OK;

    /*    Get the start of our frame data and the end of the GIF data
    */
    gif_data = gif->gif_data + gif->frames[frame].frame_pointer;
    gif_end = gif->gif_data + gif->buffer_size;
    gif_bytes = (unsigned int)(gif_end - gif_data);

    /*    Check if we have enough data
     *    The shortest block of data is a 10-byte image descriptor + 1-byte gif trailer
    */
    if (gif_bytes < 12) return GIF_INSUFFICIENT_FRAME_DATA;

    /*    Save the buffer position
    */
    save_buffer_position = gif->buffer_position;
    gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);

    /*    Skip any extensions because we all ready processed them
    */
    if ((return_value = gif_skip_frame_extensions(gif)) != GIF_OK)
        goto gif_decode_frame_exit;
    gif_data = (gif->gif_data + gif->buffer_position);
    gif_bytes = (unsigned int)(gif_end - gif_data);

    /*    Ensure we have enough data for the 10-byte image descriptor + 1-byte gif trailer
    */
    if (gif_bytes < 12) {
        return_value = GIF_INSUFFICIENT_FRAME_DATA;
        goto gif_decode_frame_exit;
    }

    /* 10-byte Image Descriptor is:
     *
     *    +0    CHAR    Image Separator (0x2c)
     *    +1    SHORT    Image Left Position
     *    +3    SHORT    Image Top Position
     *    +5    SHORT    Width
     *    +7    SHORT    Height
     *    +9    CHAR    __Packed Fields__
     *            1BIT    Local Colour Table Flag
     *            1BIT    Interlace Flag
     *            1BIT    Sort Flag
     *            2BITS    Reserved
     *            3BITS    Size of Local Colour Table
    */
    if (gif_data[0] != GIF_IMAGE_SEPARATOR) {
        return_value = GIF_DATA_ERROR;
        goto gif_decode_frame_exit;
    }
    offset_x = gif_data[1] | (gif_data[2] << 8);
    offset_y = gif_data[3] | (gif_data[4] << 8);
    width = gif_data[5] | (gif_data[6] << 8);
    height = gif_data[7] | (gif_data[8] << 8);

    /*    Boundary checking - shouldn't ever happen except unless the data has been
        modified since initialisation.
    */
    if ((offset_x + width > gif->width) || (offset_y + height > gif->height)) {
        return_value = GIF_DATA_ERROR;
        goto gif_decode_frame_exit;
    }

    /*    Decode the flags
    */
    flags = gif_data[9];
    colour_table_size = 2 << (flags & GIF_COLOUR_TABLE_SIZE_MASK);
    interlace = flags & GIF_INTERLACE_MASK;

    /*    Move our pointer to the colour table or image data (if no colour table is given)
    */
    gif_data += 10;
    gif_bytes = (unsigned int)(gif_end - gif_data);

    /*    Set up the colour table
    */
    if (flags & GIF_COLOUR_TABLE_MASK) {
        if (gif_bytes < (int)(3 * colour_table_size)) {
            return_value = GIF_INSUFFICIENT_FRAME_DATA;
            goto gif_decode_frame_exit;
        }
        colour_table = gif->local_colour_table;
        if (!gif->clear_image) {
            for (index = 0; index < colour_table_size; index++) {
                /* Gif colour map contents are r,g,b.
                 *
                 * We want to pack them bytewise into the 
                 * colour table, such that the red component
                 * is in byte 0 and the alpha component is in
                 * byte 3.
                 */
                unsigned char *entry = 
                    (unsigned char *) &colour_table[index];

                entry[0] = gif_data[0];    /* r */
                entry[1] = gif_data[1];    /* g */
                entry[2] = gif_data[2];    /* b */
                entry[3] = 0xff;    /* a */

                gif_data += 3;
            }
        } else {
            gif_data += 3 * colour_table_size;
        }
        gif_bytes = (unsigned int)(gif_end - gif_data);
    } else {
        colour_table = gif->global_colour_table;
    }

    /*    Check if we've finished
    */
    if (gif_bytes < 1) {
        return_value = GIF_INSUFFICIENT_FRAME_DATA;
        goto gif_decode_frame_exit;
    } else if (gif_data[0] == GIF_TRAILER) {
        return_value = GIF_OK;
        goto gif_decode_frame_exit;
    }

    /*    Get the frame data
    */
    assert(gif->bitmap_callbacks.bitmap_get_buffer);
    frame_data = (void *)gif->bitmap_callbacks.bitmap_get_buffer(gif->frame_image);
    if (!frame_data)
        return GIF_INSUFFICIENT_MEMORY;

    /*    If we are clearing the image we just clear, if not decode
    */
    if (!gif->clear_image) {
        /*    Ensure we have enough data for a 1-byte LZW code size + 1-byte gif trailer
        */
        if (gif_bytes < 2) {
            return_value = GIF_INSUFFICIENT_FRAME_DATA;
            goto gif_decode_frame_exit;
        /*    If we only have a 1-byte LZW code size + 1-byte gif trailer, we're finished
        */
        } else if ((gif_bytes == 2) && (gif_data[1] == GIF_TRAILER)) {
            return_value = GIF_OK;
            goto gif_decode_frame_exit;
        }

        /*    If the previous frame's disposal method requires we restore the background
         *    colour or this is the first frame, clear the frame data
        */
        if ((frame == 0) || (gif->decoded_frame == GIF_INVALID_FRAME)) {
            memset((char*)frame_data, GIF_TRANSPARENT_COLOUR, gif->width * gif->height * sizeof(int));
            gif->decoded_frame = frame;
            /* The line below would fill the image with its background color, but because GIFs support
             * transparency we likely wouldn't want to do that. */
            /* memset((char*)frame_data, colour_table[gif->background_index], gif->width * gif->height * sizeof(int)); */
        } else if ((frame != 0) && (gif->frames[frame - 1].disposal_method == GIF_FRAME_CLEAR)) {
            gif->clear_image = true;
            if ((return_value = gif_decode_frame(gif, (frame - 1))) != GIF_OK)
                goto gif_decode_frame_exit;
            gif->clear_image = false;
        /*    If the previous frame's disposal method requires we restore the previous
         *    image, find the last image set to "do not dispose" and get that frame data
        */
        } else if ((frame != 0) && (gif->frames[frame - 1].disposal_method == GIF_FRAME_RESTORE)) {
                while ((last_undisposed_frame != -1) && (gif->frames[last_undisposed_frame--].disposal_method == GIF_FRAME_RESTORE)) {
                }
                /*    If we don't find one, clear the frame data
                */
                if (last_undisposed_frame == -1) {
                    /* see notes above on transparency vs. background color */
                    memset((char*)frame_data, GIF_TRANSPARENT_COLOUR, gif->width * gif->height * sizeof(int));
                } else {
                    if ((return_value = gif_decode_frame(gif, last_undisposed_frame)) != GIF_OK)
                        goto gif_decode_frame_exit;
                    /*    Get this frame's data
                    */
                    assert(gif->bitmap_callbacks.bitmap_get_buffer);
                    frame_data = (void *)gif->bitmap_callbacks.bitmap_get_buffer(gif->frame_image);
                    if (!frame_data)
                        return GIF_INSUFFICIENT_MEMORY;
                }
        }
        gif->decoded_frame = frame;

        /*    Initialise the LZW decoding
        */
        gif->set_code_size = gif_data[0];
        gif->buffer_position = (unsigned int)(gif_data - gif->gif_data + 1);

        /*    Set our code variables
        */
        gif->code_size = gif->set_code_size + 1;
        gif->clear_code = (1 << gif->set_code_size);
        gif->end_code = gif->clear_code + 1;
        gif->max_code_size = gif->clear_code << 1;
        gif->max_code = gif->clear_code + 2;
        gif->curbit = gif->lastbit = 0;
        gif->last_byte = 2;
        gif->get_done = false;
        gif->direct = gif->buf;
        gif_init_LZW(gif);

        /*    Decompress the data
        */
        for (y = 0; y < height; y++) {
            if (interlace)
                decode_y = gif_interlaced_line(height, y) + offset_y;
            else
                decode_y = y + offset_y;
            frame_scanline = frame_data + offset_x + (decode_y * gif->width);

            /*    Rather than decoding pixel by pixel, we try to burst out streams
                of data to remove the need for end-of data checks every pixel.
            */
            x = width;
            while (x > 0) {
                burst_bytes = (unsigned int)(gif->stack_pointer - gif->stack);
                if (burst_bytes > 0) {
                    if (burst_bytes > x)
                        burst_bytes = x;
                    x -= burst_bytes;
                    while (burst_bytes-- > 0) {
                        colour = *--gif->stack_pointer;
                        if (((gif->frames[frame].transparency) &&
                            (colour != gif->frames[frame].transparency_index)) ||
                            (!gif->frames[frame].transparency))
                                *frame_scanline = colour_table[colour];
                        frame_scanline++;
                    }
                } else {
                    if (!gif_next_LZW(gif)) {
                        /*    Unexpected end of frame, try to recover
                        */
                        if (gif->current_error == GIF_END_OF_FRAME)
                            return_value = GIF_OK;
                        else
                            return_value = gif->current_error;
                        goto gif_decode_frame_exit;
                    }
                }
            }
        }
    } else {
        /*    Clear our frame
        */
        if (gif->frames[frame].disposal_method == GIF_FRAME_CLEAR) {
            for (y = 0; y < height; y++) {
                frame_scanline = frame_data + offset_x + ((offset_y + y) * gif->width);
                if (gif->frames[frame].transparency)
                    memset(frame_scanline, GIF_TRANSPARENT_COLOUR, width * 4);
                else
                    memset(frame_scanline, colour_table[gif->background_index], width * 4);
            }
        }
    }
gif_decode_frame_exit:

    /*    Check if we should test for optimisation
    */
    if (gif->frames[frame].virgin) {
        if (gif->bitmap_callbacks.bitmap_test_opaque)
            gif->frames[frame].opaque = gif->bitmap_callbacks.bitmap_test_opaque(gif->frame_image);
        else
            gif->frames[frame].opaque = false;
        gif->frames[frame].virgin = false;
    }
    if (gif->bitmap_callbacks.bitmap_set_opaque)
        gif->bitmap_callbacks.bitmap_set_opaque(gif->frame_image, gif->frames[frame].opaque);
    if (gif->bitmap_callbacks.bitmap_modified)
        gif->bitmap_callbacks.bitmap_modified(gif->frame_image);

    /*    Restore the buffer position
    */
    gif->buffer_position = save_buffer_position;

    /*    Success!
    */
    return return_value;

}

/**    Skips the frame's extensions (which have been previously initialised)

    @return GIF_INSUFFICIENT_FRAME_DATA for insufficient data to complete the frame
        GIF_OK for successful decoding
*/
static gif_result gif_skip_frame_extensions(gif_animation *gif) {
    unsigned char *gif_data, *gif_end;
    int gif_bytes;
    unsigned int block_size;

    /*    Get our buffer position etc.
    */
    gif_data = (unsigned char *)(gif->gif_data + gif->buffer_position);
    gif_end = (unsigned char *)(gif->gif_data + gif->buffer_size);
    gif_bytes = (unsigned int)(gif_end - gif_data);

    /*    Skip the extensions
    */
    while (gif_data[0] == GIF_EXTENSION_INTRODUCER) {
        ++gif_data;

        /*    Switch on extension label
        */
        switch(gif_data[0]) {
            /*    Move the pointer to the first data sub-block
             *    1 byte for the extension label
            */
            case GIF_EXTENSION_COMMENT:
                ++gif_data;
                break;

            /*    Move the pointer to the first data sub-block
             *    2 bytes for the extension label and size fields
             *    Skip the extension size itself
            */
            default:
                gif_data += (2 + gif_data[1]);
        }

        /*    Repeatedly skip blocks until we get a zero block or run out of data
         *    This data is ignored by this gif decoder
        */
        gif_bytes = (unsigned int)(gif_end - gif_data);
        block_size = 0;
        while (gif_data[0] != GIF_BLOCK_TERMINATOR) {
            block_size = gif_data[0] + 1;
            if ((gif_bytes -= block_size) < 0)
                return GIF_INSUFFICIENT_FRAME_DATA;
            gif_data += block_size;
        }
        ++gif_data;
    }

    /*    Set buffer position and return
    */
    gif->buffer_position = (unsigned int)(gif_data - gif->gif_data);
    return GIF_OK;
}

static unsigned int gif_interlaced_line(int height, int y) {
    if ((y << 3) < height) return (y << 3);
    y -= ((height + 7) >> 3);
    if ((y << 3) < (height - 4)) return (y << 3) + 4;
    y -= ((height + 3) >> 3);
    if ((y << 2) < (height - 2)) return (y << 2) + 2;
    y -= ((height + 1) >> 2);
    return (y << 1) + 1;
}

/*    Releases any workspace held by the animation
*/
void gif_finalise(gif_animation *gif) {
    /*    Release all our memory blocks
    */
    if (gif->frame_image) {
        assert(gif->bitmap_callbacks.bitmap_destroy);
        gif->bitmap_callbacks.bitmap_destroy(gif->frame_image);
    }
    gif->frame_image = NULL;
    free(gif->frames);
    gif->frames = NULL;
    free(gif->local_colour_table);
    gif->local_colour_table = NULL;
    free(gif->global_colour_table);
    gif->global_colour_table = NULL;
}

/**
 * Initialise LZW decoding
 */
void gif_init_LZW(gif_animation *gif) {
    int i;

    gif->current_error = 0;
    if (gif->clear_code >= (1 << GIF_MAX_LZW)) {
        gif->stack_pointer = gif->stack;
        gif->current_error = GIF_FRAME_DATA_ERROR;
        return;
    }

    /* initialise our table */
    memset(gif->table, 0x00, (1 << GIF_MAX_LZW) * 8);
    for (i = 0; i < gif->clear_code; ++i)
        gif->table[1][i] = i;

    /* update our LZW parameters */
    gif->code_size = gif->set_code_size + 1;
    gif->max_code_size = gif->clear_code << 1;
    gif->max_code = gif->clear_code + 2;
    gif->stack_pointer = gif->stack;
    do {
        gif->firstcode = gif->oldcode = gif_next_code(gif, gif->code_size);
    } while (gif->firstcode == gif->clear_code);
    *gif->stack_pointer++ =gif->firstcode;
}


static bool gif_next_LZW(gif_animation *gif) {
    int code, incode;
    int block_size;
    int new_code;

    code = gif_next_code(gif, gif->code_size);
    if (code < 0) {
          gif->current_error = code;
        return false;
    } else if (code == gif->clear_code) {
        gif_init_LZW(gif);
        return true;
    } else if (code == gif->end_code) {
        /* skip to the end of our data so multi-image GIFs work */
        if (gif->zero_data_block) {
            gif->current_error = GIF_FRAME_DATA_ERROR;
            return false;
        }
        block_size = 0;
        while (block_size != 1) {
            block_size = gif->gif_data[gif->buffer_position] + 1;
            gif->buffer_position += block_size;
        }
        gif->current_error = GIF_FRAME_DATA_ERROR;
        return false;
    }

    incode = code;
    if (code >= gif->max_code) {
        *gif->stack_pointer++ = gif->firstcode;
        code = gif->oldcode;
    }

    /* The following loop is the most important in the GIF decoding cycle as every
     * single pixel passes through it.
     *
     * Note: our gif->stack is always big enough to hold a complete decompressed chunk. */
    while (code >= gif->clear_code) {
        *gif->stack_pointer++ = gif->table[1][code];
        new_code = gif->table[0][code];
        if (new_code < gif->clear_code) {
            code = new_code;
            break;
        }
        *gif->stack_pointer++ = gif->table[1][new_code];
        code = gif->table[0][new_code];
        if (code == new_code) {
              gif->current_error = GIF_FRAME_DATA_ERROR;
            return false;
        }
    }

    *gif->stack_pointer++ = gif->firstcode = gif->table[1][code];

    if ((code = gif->max_code) < (1 << GIF_MAX_LZW)) {
        gif->table[0][code] = gif->oldcode;
        gif->table[1][code] = gif->firstcode;
        ++gif->max_code;
        if ((gif->max_code >= gif->max_code_size) && (gif->max_code_size < (1 << GIF_MAX_LZW))) {
            gif->max_code_size = gif->max_code_size << 1;
            ++gif->code_size;
        }
    }
    gif->oldcode = incode;
    return true;
}

static int gif_next_code(gif_animation *gif, int code_size) {
    int i, j, end, count, ret;
    unsigned char *b;

    (void)code_size;

    end = gif->curbit + gif->code_size;
    if (end >= gif->lastbit) {
        if (gif->get_done)
            return GIF_END_OF_FRAME;
        gif->buf[0] = gif->direct[gif->last_byte - 2];
        gif->buf[1] = gif->direct[gif->last_byte - 1];

        /* get the next block */
        gif->direct = gif->gif_data + gif->buffer_position;
        gif->zero_data_block = ((count = gif->direct[0]) == 0);
        if ((gif->buffer_position + count) >= gif->buffer_size)
            return GIF_INSUFFICIENT_FRAME_DATA;
        if (count == 0)
            gif->get_done = true;
        else {
            gif->direct -= 1;
            gif->buf[2] = gif->direct[2];
            gif->buf[3] = gif->direct[3];
        }
        gif->buffer_position += count + 1;

        /* update our variables */
        gif->last_byte = 2 + count;
        gif->curbit = (gif->curbit - gif->lastbit) + 16;
        gif->lastbit = (2 + count) << 3;
        end = gif->curbit + gif->code_size;
    }

    i = gif->curbit >> 3;
    if (i < 2)
        b = gif->buf;
    else
        b = gif->direct;

    ret = b[i];
    j = (end >> 3) - 1;
    if (i <= j) {
        ret |= (b[i + 1] << 8);
        if (i < j)
            ret |= (b[i + 2] << 16);
    }
    ret = (ret >> (gif->curbit % 8)) & maskTbl[gif->code_size];
    gif->curbit += gif->code_size;
    return ret;
}
