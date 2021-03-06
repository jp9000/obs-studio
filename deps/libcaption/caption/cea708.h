/**********************************************************************************************/
/* The MIT License                                                                            */
/*                                                                                            */
/* Copyright 2016-2017 Twitch Interactive, Inc. or its affiliates. All Rights Reserved.       */
/*                                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a copy               */
/* of this software and associated documentation files (the "Software"), to deal              */
/* in the Software without restriction, including without limitation the rights               */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell                  */
/* copies of the Software, and to permit persons to whom the Software is                      */
/* furnished to do so, subject to the following conditions:                                   */
/*                                                                                            */
/* The above copyright notice and this permission notice shall be included in                 */
/* all copies or substantial portions of the Software.                                        */
/*                                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR                 */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,                   */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE                */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER                     */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,              */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN                  */
/* THE SOFTWARE.                                                                              */
/**********************************************************************************************/
#ifndef LIBCAPTION_CEA708_H
#define LIBCAPTION_CEA708_H
#ifdef __cplusplus
extern "C" {
#endif

#include "caption.h"
#define CEA608_MAX_SIZE (255)

////////////////////////////////////////////////////////////////////////////////
typedef enum {
    cc_type_ntsc_cc_field_1 = 0,
    cc_type_ntsc_cc_field_2 = 1,
    cc_type_dtvcc_packet_data = 2,
    cc_type_dtvcc_packet_start = 3,
} cea708_cc_type_t;

typedef struct {
    unsigned int marker_bits : 5;
    unsigned int cc_valid : 1;
    unsigned int cc_type : 2; // castable to cea708_cc_type_t
    unsigned int cc_data : 16;
} cc_data_t;

typedef struct {
    unsigned int process_em_data_flag : 1;
    unsigned int process_cc_data_flag : 1;
    unsigned int additional_data_flag : 1;
    unsigned int cc_count : 5;
    unsigned int em_data : 8;
    cc_data_t cc_data[32];
} user_data_t;

/*! \brief
    \param
*/
cc_data_t cea708_encode_cc_data(int cc_valid, cea708_cc_type_t type, uint16_t cc_data);
/*! \brief
    \param
*/
int cea708_cc_count(user_data_t* data);
/*! \brief
    \param
*/
uint16_t cea708_cc_data(user_data_t* data, int index, int* valid, cea708_cc_type_t* type);
////////////////////////////////////////////////////////////////////////////////

typedef enum {
    country_united_states = 181,
} itu_t_t35_country_code_t;

typedef enum {
    t35_provider_direct_tv = 47,
    t35_provider_atsc = 49,
} itu_t_t35_provider_code_t;

typedef struct {
    itu_t_t35_country_code_t country;
    itu_t_t35_provider_code_t provider;
    uint32_t user_identifier;
    uint8_t user_data_type_code;
    uint8_t directv_user_data_length;
    user_data_t user_data;
    double timestamp;
} cea708_t;

static const uint32_t GA94 = (('G' << 24) | ('A' << 16) | ('9' << 8) | '4');
static const uint32_t DTG1 = (('D' << 24) | ('T' << 16) | ('G' << 8) | '1');

/*! \brief
    \param
*/
int cea708_init(cea708_t* cea708, double timestamp); // will confgure using HLS compatiable defaults
/*! \brief
    \param
*/
libcaption_stauts_t cea708_parse_h264(const uint8_t* data, size_t size, cea708_t* cea708);
/*! \brief
    \param
*/
libcaption_stauts_t cea708_parse_h262(const uint8_t* data, size_t size, cea708_t* cea708);
/*! \brief
    \param
*/
libcaption_stauts_t cea708_to_caption_frame(caption_frame_t* frame, cea708_t* cea708);
/*! \brief
    \param
*/
int cea708_add_cc_data(cea708_t* cea708, int valid, cea708_cc_type_t type, uint16_t cc_data);
/*! \brief
    \param
*/
int cea708_render(cea708_t* cea708, uint8_t* data, size_t size);
/*! \brief
    \param
*/
void cea708_dump(cea708_t* cea708);
#ifdef __cplusplus
}
#endif
#endif
