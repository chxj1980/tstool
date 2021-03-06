// Last Update:2019-04-03 15:19:19
/**
 * @file dump.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-03-25
 */

// Last Update:2019-03-14 11:20:46
/**
 * @file flv-dump.c
 * @brief
 * @author felix
 * @version 0.1.00
 * @date 2019-03-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "put_bits.h"
#include "dump.h"

#define ADTS_HEADER_SIZE   7
#define PROFILE_LC 1
#define SAMPLE_RATE_44100 4


static uint8_t h264_aud[] = { 0x00, 0x00, 0x00, 0x01, 0x09, 0xF0 };

static FILE *fp = NULL;

void DbgDumpBuf( int line, const char *func, unsigned char *buf, int size )
{
    int i = 0;
    int j = 0;

#if 0
    if ( size > 192 )
        size = 192;
#endif

    printf("[ %s %d ] size : %d the buffer  is : \n[%02d] ",  func, line, size,  j++  );
    for ( i=0; i<size; i++ ) {
        printf("0x%02x, ", (unsigned char)buf[i] );
        if ( (i+1)%16 == 0 ) {
            printf("\n[%02d] ", j++ );
//            printf("\n ", j++ );
        }
    }
    printf("\n");
}

void AddAdts( unsigned char *buf, int size  )
{
    PutBitContext pb;

    init_put_bits(&pb, buf, ADTS_HEADER_SIZE);

    /* adts_fixed_header */
    put_bits(&pb, 12, 0xfff);   /* syncword */
    put_bits(&pb, 1, 0);        /* ID */
    put_bits(&pb, 2, 0);        /* layer */
    put_bits(&pb, 1, 1);        /* protection_absent */
    put_bits(&pb, 2, PROFILE_LC ); /* profile_objecttype */
    put_bits(&pb, 4, SAMPLE_RATE_44100);
    put_bits(&pb, 1, 0);        /* private_bit */
    put_bits(&pb, 3, 2); /* channel_configuration */
    put_bits(&pb, 1, 0);        /* original_copy */
    put_bits(&pb, 1, 0);        /* home */

    /* adts_variable_header */
    put_bits(&pb, 1, 0);        /* copyright_identification_bit */
    put_bits(&pb, 1, 0);        /* copyright_identification_start */
    put_bits(&pb, 13, ADTS_HEADER_SIZE + size ); /* aac_frame_length */
    put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
    put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */

    flush_put_bits(&pb);
}

void H264ConfigHandle( unsigned char *in, unsigned char *out, int *size )
{
    unsigned char *dst = out;
    unsigned char *ptr = in + 6;// skip version & sps number ...
    int len = AV_RB16( ptr );

    ptr += 2;// sps length
    LOGI("len = %d\n", len );
    ASSERT( len > 0 );
    *dst++ = 0x00;
    *dst++ = 0x00;
    *dst++ = 0x00;
    *dst++ = 0x01;
    memcpy( dst, ptr, len );// sps
    dst += len;
    ptr += len;

    // pps
    *dst++ = 0x00;
    *dst++ = 0x00;
    *dst++ = 0x00;
    *dst++ = 0x01;
    ptr++;// skip pps number
    len = 0;
    len =  ptr[0]<<16 | ptr[1];
    ptr += 2;
    LOGI("len = %d\n", len );
    ASSERT( len > 0 );
    memcpy( dst, ptr, len );
    dst += len;

    LOGI("h264 config length : %ld\n", dst-out );
    *size = dst-out;
}

void PushAVData( unsigned char *buf, int buf_size, int isvideo, int64_t timestamp, int composition_time, char iskey  )
{

    char *pwd = getenv("PWD");
    char filename[128] = { 0 };

    if ( !pwd ) {
        LOGE("get pwd env error\n");
        return;
    }

    sprintf( filename, "%s/av-dump.data", pwd);

    if ( !fp ) {
        LOGI("filename = %s\n", filename );
        fp = fopen( filename, "w+" );
        if ( !fp ) {
            LOGE("open file error\n");
            return;
        } else {
            LOGI("open file success\n");
        }
    }

    if ( memcmp( h264_aud, buf, sizeof(h264_aud) ) == 0 ) {
        buf += sizeof( h264_aud );
        buf_size -= sizeof( h264_aud );
    }

    ASSERT( buf_size > 0 );

//    DUMPBUF( buf, 32 );
//    DUMPBUF( buf+buf_size-32, 32 );
    LOGI("buf_size = %d, isvideo = %d, timestamp = %ld, iskey = %d\n", buf_size, isvideo, timestamp, iskey  );
    fwrite( (char *)&isvideo, 1, 1, fp );
    if ( isvideo ) {
        LOGI("write iskey : %d\n", iskey );
        fwrite( (char*)&iskey, 1, 1, fp );
    }

    fwrite( (char *)&timestamp, 1, 4, fp );
    fwrite( (char *)&composition_time, 1, 4, fp );
    fwrite( (char *)&buf_size, 1, 4, fp );
    fwrite( buf, 1, buf_size, fp );
}

void PushFinish()
{
    fclose( fp );
}

/* frame type & codec : 1byte  */
/* AvcPakcetType */
/* composition_time */
#define NON_NALU_LEN ( 1 +  1 +  3  )

typedef struct {
    uint8_t nalu_type;
    char *str;
} NalUnitInfo;

#define NALU_ITEM( item ) { item, #item }

static NalUnitInfo gNalUnitInfo[] =
{
    NALU_ITEM( NALU_TYPE_SLICE ),
    NALU_ITEM( NALU_TYPE_DPA ),
    NALU_ITEM( NALU_TYPE_DPB ),
    NALU_ITEM( NALU_TYPE_DPC ),
    NALU_ITEM( NALU_TYPE_IDR ),
    NALU_ITEM( NALU_TYPE_SEI ),
    NALU_ITEM( NALU_TYPE_SPS ),
    NALU_ITEM( NALU_TYPE_PPS ),
    NALU_ITEM( NALU_TYPE_AUD ),
    NALU_ITEM( NALU_TYPE_EOSEQ ),
    NALU_ITEM( NALU_TYPE_EOSTREAM ),
    NALU_ITEM( NALU_TYPE_FILL ),
};

char *GetNaluType( uint8_t type )
{
    int i;

    for ( i=0; i<sizeof(gNalUnitInfo)/sizeof(gNalUnitInfo[0]); i++ ) {
        if ( gNalUnitInfo[i].nalu_type == type ) {
            return gNalUnitInfo[i].str;
        }
    }

    LOGE("nalu type error\n");
    exit(1);
}

void DumpNalUnit( uint8_t *data, int datasize )
{
   uint8_t *buf_ptr = data;
   uint8_t *buf_end = data + datasize - NON_NALU_LEN;
   int len = 0, i = 0;

   while( buf_ptr < buf_end ) {
       len = AV_RB32( buf_ptr );
//       LOGI("parse flv video tag nalu data, len = %d\n", len );
       ASSERT( len > 0 );
       buf_ptr += 4;
       uint8_t nalu_type = (*buf_ptr) & 0x1f;
       char *nal_type_str = GetNaluType( nalu_type );
       printf("\t\tnalu index : %d\n", i );
       printf("\t\t\tnalu type : %s\n", nal_type_str );
       printf("\t\t\tnalu length : %d\n", len );
       buf_ptr += len;
       i++;
   }
}



