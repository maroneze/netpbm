/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 */
/*
 * Runput.c - General purpose Run Length Encoding.
 *
 * Author:      Spencer W. Thomas
 *              Computer Science Dept.
 *              University of Utah
 * Date:        Mon Aug  9 1982
 * Copyright (c) 1982,1986 Spencer W. Thomas
 *
 * $Id: Runput.c,v 3.0.1.1 1992/01/28 18:17:40 spencer Exp $
 *
 * Modified by: Todd W. Fuqua
 *      Date:   Jul 22 1984
 * convert to new RLE format to make room for larger frame buffers
 */

/* THIS IS WAY OUT OF DATE.  See rle.5.
 * The output file format is:
 *
 * Word 0:      A "magic" number.  The top byte of the word contains
 *              the letter 'R' or the letter 'W'.  'W' indicates that
 *              only black and white information was saved.  The bottom
 *              byte is one of the following:
 *      ' ':    Means a straight "box" save, -S flag was given.
 *      'B':    Image saved with background color, clear screen to
 *              background before restoring image.
 *      'O':    Image saved in overlay mode.
 *
 * Words 1-6:   The structure
 * {     short   xpos,                  Lower left corner
 *             ypos,
 *             xsize,                   Size of saved box
 *             ysize;
 *     char    rgb[3];                  Background color
 *     char    map;                     flag for map presence
 * }
 *
 * If the map flag is non-zero, then the color map will follow as
 * 3*256 16 bit words, first the red map, then the green map, and
 * finally the blue map.
 *
 * Following the setup information is the Run Length Encoded image.
 * Each instruction consists of a 4-bit opcode, a 12-bit datum and
 * possibly one or more following words (all words are 16 bits).  The
 * instruction opcodes are:
 *
 * SkipLines (1):   The bottom 10 bits are an unsigned number to be added to
 *                  current Y position.
 *
 * SetColor (2):    The datum indicates which color is to be loaded with
 *                  the data described by the following ByteData and
 *                  RunData instructions.  0->red, 1->green, 2->blue.  The
 *                  operation also resets the X position to the initial
 *                  X (i.e. a carriage return operation is performed).
 *
 * SkipPixels (3):  The bottom 10 bits are an unsigned number to be
 *                  added to the current X position.
 *
 * ByteData (5):    The datum is one less than the number of bytes of
 *                  color data following.  If the number of bytes is
 *                  odd, a filler byte will be appended to the end of
 *                  the byte string to make an integral number of 16-bit
 *                  words.  The bytes are in PDP-11 order.  The X
 *                  position is incremented to follow the last byte of
 *                  data.
 *
 * RunData (6):     The datum is one less than the run length.  The
 *                  following word contains (in its lower 8 bits) the
 *                  color of the run.  The X position is incremented to
 *                  follow the last byte in the run.
 */

#include    <string.h>
#include        <stdio.h>

#include        "rle_put.h"
#include        "rle.h"
#include        "rle_code.h"
#include    "vaxshort.h"
#include    "Runput.h"

#define UPPER 255                       /* anything bigger ain't a byte */

/*
 * Macros to make writing instructions with correct byte order easier.
 */
/* Write a two-byte value in little_endian order. */
#define put16(a)    (putc((a)&0xff,rle_fd),putc((char)(((a)>>8)&0xff),rle_fd))

/* short instructions */
#define mk_short_1(oper,a1)             /* one argument short */ \
    putc(oper,rle_fd), putc((char)a1,rle_fd)

#define mk_short_2(oper,a1,a2)          /* two argument short */ \
    putc(oper,rle_fd), putc((char)a1,rle_fd), put16(a2)

/* long instructions */
#define mk_long_1(oper,a1)              /* one argument long */ \
    putc((char)(LONG|oper),rle_fd), putc('\0', rle_fd), put16(a1)

#define mk_long_2(oper,a1,a2)           /* two argument long */ \
    putc((char)(LONG|oper),rle_fd), putc('\0', rle_fd), \
    put16(a1), put16(a2)

/* choose between long and short format instructions */
/* NOTE: these macros can only be used where a STATEMENT is legal */

#define mk_inst_1(oper,a1)              /* one argument inst */ \
    if (a1>UPPER) (mk_long_1(oper,a1)); else (mk_short_1(oper,a1))

#define mk_inst_2(oper,a1,a2)           /* two argument inst */ \
    if (a1>UPPER) (mk_long_2(oper,a1,a2)); else (mk_short_2(oper,a1,a2))

/*
 * Opcode definitions
 */
#define     RSkipLines(n)           mk_inst_1(RSkipLinesOp,(n))

#define     RSetColor(c)            mk_short_1(RSetColorOp,(c))
                                    /* has side effect of performing */
                                    /* "carriage return" action */

#define     RSkipPixels(n)          mk_inst_1(RSkipPixelsOp,(n))

#define     RNewLine                RSkipLines(1)

#define     RByteData(n)            mk_inst_1(RByteDataOp,n)
                                        /* followed by ((n+1)/2)*2 bytes */
                                        /* of data.  If n is odd, last */
                                        /* byte will be ignored */
                                        /* "cursor" is left at pixel */
                                        /* following last pixel written */

#define     RRunData(n,c)           mk_inst_2(RRunDataOp,(n),(c))
                                        /* next word contains color data */
                                        /* "cursor" is left at pixel after */
                                        /* end of run */

#define     REOF                    mk_inst_1(REOFOp,0)
                                        /* Really opcode only */

/*****************************************************************
 * TAG( RunSetup )
 * Put out initial setup data for RLE files.
 */
void
RunSetup(rle_hdr * the_hdr)
{
    struct XtndRsetup setup;
    FILE * rle_fd = the_hdr->rle_file;

    put16( RLE_MAGIC );

    if ( the_hdr->background == 2 )
        setup.h_flags = H_CLEARFIRST;
    else if ( the_hdr->background == 0 )
        setup.h_flags = H_NO_BACKGROUND;
    else
        setup.h_flags = 0;
    if ( the_hdr->alpha )
        setup.h_flags |= H_ALPHA;
    if ( the_hdr->comments != NULL && *the_hdr->comments != NULL )
        setup.h_flags |= H_COMMENT;

    setup.h_ncolors = the_hdr->ncolors;
    setup.h_pixelbits = 8;              /* Grinnell dependent */
    if ( the_hdr->ncmap > 0 && the_hdr->cmap == NULL )
    {
        fprintf( stderr,
       "%s: Color map of size %d*%d specified, but not supplied, writing %s\n",
                 the_hdr->cmd, the_hdr->ncmap, (1 << the_hdr->cmaplen),
                 the_hdr->file_name );
        the_hdr->ncmap = 0;
    }
    setup.h_cmaplen = the_hdr->cmaplen; /* log2 of color map size */
    setup.h_ncmap = the_hdr->ncmap;     /* no of color channels */
    vax_pshort(setup.hc_xpos,the_hdr->xmin);
    vax_pshort(setup.hc_ypos,the_hdr->ymin);
    vax_pshort(setup.hc_xlen,the_hdr->xmax - the_hdr->xmin + 1);
    vax_pshort(setup.hc_ylen,the_hdr->ymax - the_hdr->ymin + 1);
    fwrite((char *)&setup, SETUPSIZE, 1, rle_fd);
    if ( the_hdr->background != 0 )
    {
        int i;
        rle_pixel *background =
            (rle_pixel *)malloc( (unsigned)(the_hdr->ncolors + 1) );
        int *bg_color;
        /*
         * If even number of bg color bytes, put out one more to get to
         * 16 bit boundary.
         */
        bg_color = the_hdr->bg_color;
        for ( i = 0; i < the_hdr->ncolors; i++ )
            background[i] =  *bg_color++;
        /* Extra byte, if written, should be 0. */
        background[i] = 0;
        fwrite((char *)background, (the_hdr->ncolors / 2) * 2 + 1, 1, rle_fd);
        free( background );
    }
    else
        putc( '\0', rle_fd );
    if (the_hdr->ncmap > 0)
    {
        /* Big-endian machines are harder */
        int i, nmap = (1 << the_hdr->cmaplen) *
                               the_hdr->ncmap;
        char *h_cmap = (char *)malloc( nmap * 2 );
        if ( h_cmap == NULL )
        {
            fprintf( stderr,
             "%s: Malloc failed for color map of size %d, writing %s\n",
                     the_hdr->cmd, nmap, the_hdr->file_name );
            exit( 1 );
        }
        for ( i = 0; i < nmap; i++ )
            vax_pshort( &h_cmap[i*2], the_hdr->cmap[i] );

        fwrite( h_cmap, nmap, 2, rle_fd );
        free( h_cmap );
    }

    /* Now write out comments if given */
    if ( setup.h_flags & H_COMMENT )
    {
        int comlen;
        CONST_DECL char ** com_p;

        /* Get the total length of comments */
        comlen = 0;
        for ( com_p = the_hdr->comments; *com_p != NULL; com_p++ )
            comlen += 1 + strlen( *com_p );

        put16( comlen );
        for ( com_p = the_hdr->comments; *com_p != NULL; com_p++ )
            fwrite( *com_p, 1, strlen( *com_p ) + 1, rle_fd );

        if ( comlen & 1 )       /* if odd length, round up */
            putc( '\0', rle_fd );
    }
}

/*****************************************************************
 * TAG( RunSkipBlankLines )
 * Skip one or more blank lines in the RLE file.
 */
void
RunSkipBlankLines(int nblank, rle_hdr * the_hdr)
{
    FILE * rle_fd = the_hdr->rle_file;
    RSkipLines(nblank);
}

/*****************************************************************
 * TAG( RunSetColor )
 * Select a color and do carriage return.
 * color: 0 = Red, 1 = Green, 2 = Blue.
 */
void
RunSetColor(int c, rle_hdr * the_hdr)
{
    FILE * rle_fd = the_hdr->rle_file;
    RSetColor(c);
}

/*****************************************************************
 * TAG( RunSkipPixels )
 * Skip a run of background.
 */

/* ARGSUSED */
void
RunSkipPixels(int nskip, int last, int wasrun, rle_hdr * the_hdr)
{
    FILE * rle_fd = the_hdr->rle_file;
    if (! last && nskip > 0)
    {
        RSkipPixels(nskip);
    }
}

/*****************************************************************
 * TAG( RunNewScanLine )
 * Perform a newline action.  Since CR is implied by the Set Color
 * operation, only generate code if the newline flag is true.
 */
void
RunNewScanLine(int flag, rle_hdr * the_hdr)
{
    FILE * rle_fd = the_hdr->rle_file;
    if (flag)
    {
        RNewLine;
    }
}

/*****************************************************************
 * TAG( Runputdata )
 * Put one or more pixels of byte data into the output file.
 */
void
Runputdata(rle_pixel * buf, int n, rle_hdr * the_hdr)
{
    FILE * rle_fd = the_hdr->rle_file;
    if (n == 0)
        return;

    RByteData(n-1);
    fwrite((char *)buf, n, 1, rle_fd);
    if ( n & 1 )
        putc( 0, rle_fd );
}

/*****************************************************************
 * TAG( Runputrun )
 * Output a single color run.
 */

/* ARGSUSED */
void
Runputrun(int color, int n, int last, rle_hdr * the_hdr)
{
    FILE * rle_fd = the_hdr->rle_file;
    RRunData(n-1,color);
}


/*****************************************************************
 * TAG( RunputEof )
 * Output an EOF opcode
 */
void
RunputEof(rle_hdr * the_hdr)
{
    FILE * rle_fd = the_hdr->rle_file;
    REOF;
}
